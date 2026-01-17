#include "Zone.h"
#include "ScaleUtilities.h"
#include "KeyboardLayoutUtils.h"
#include <algorithm>
#include <map>
#include <set>

Zone::Zone()
    : name("Untitled Zone"),
      targetAliasHash(0),
      rootNote(60),
      scaleName("Major"),
      chromaticOffset(0),
      degreeOffset(0),
      isTransposeLocked(false),
      layoutStrategy(LayoutStrategy::Linear),
      gridInterval(5),
      zoneColor(juce::Colours::transparentBlack),
      midiChannel(1) {
  // Cache will be rebuilt when zone is added to ZoneManager
}

void Zone::rebuildCache(const std::vector<int>& intervals) {
  keyToNoteCache.clear();

  if (inputKeyCodes.empty())
    return;

  if (layoutStrategy == LayoutStrategy::Linear) {
    // Linear mode: Map each key to its index in inputKeyCodes
    for (size_t i = 0; i < inputKeyCodes.size(); ++i) {
      int keyCode = inputKeyCodes[i];
      int degree = static_cast<int>(i) + degreeOffset;
      int baseNote = ScaleUtilities::calculateMidiNote(rootNote, intervals, degree);
      int relativeNote = baseNote - rootNote; // Store relative to root for transpose
      keyToNoteCache[keyCode] = relativeNote;
    }
  } else if (layoutStrategy == LayoutStrategy::Grid) {
    // Grid mode: Calculate based on keyboard geometry
    const auto& layout = KeyboardLayoutUtils::getLayout();
    
    // Get anchor key (first key in inputKeyCodes)
    if (inputKeyCodes.empty())
      return;

    auto anchorKeyIt = layout.find(inputKeyCodes[0]);
    if (anchorKeyIt == layout.end())
      return; // Anchor key not in layout

    const auto& anchorKey = anchorKeyIt->second;

    // Process each key in inputKeyCodes
    for (int keyCode : inputKeyCodes) {
      auto currentKeyIt = layout.find(keyCode);
      if (currentKeyIt == layout.end())
        continue; // Skip keys not in layout

      const auto& currentKey = currentKeyIt->second;
      
      // Calculate delta
      int deltaCol = static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
      int deltaRow = currentKey.row - anchorKey.row;

      // Degree = deltaCol + (deltaRow * gridInterval)
      int degree = deltaCol + (deltaRow * gridInterval) + degreeOffset;
      int baseNote = ScaleUtilities::calculateMidiNote(rootNote, intervals, degree);
      int relativeNote = baseNote - rootNote; // Store relative to root for transpose
      keyToNoteCache[keyCode] = relativeNote;
    }
  } else if (layoutStrategy == LayoutStrategy::Piano) {
    // Piano mode: Force Chromatic scale (Major scale intervals for white keys)
    // Major scale intervals: {0, 2, 4, 5, 7, 9, 11} = C, D, E, F, G, A, B
    const std::vector<int> majorIntervals = {0, 2, 4, 5, 7, 9, 11};
    const auto& layout = KeyboardLayoutUtils::getLayout();

    // Group keys by row
    std::map<int, std::vector<std::pair<int, float>>> keysByRow; // row -> [(keyCode, col), ...]
    
    for (int keyCode : inputKeyCodes) {
      auto keyIt = layout.find(keyCode);
      if (keyIt == layout.end())
        continue; // Skip keys not in layout

      int row = keyIt->second.row;
      float col = keyIt->second.col;
      keysByRow[row].push_back({keyCode, col});
    }

    if (keysByRow.size() < 2) {
      // Need at least 2 rows for Piano mode
      return;
    }

    // Identify rows: Max row = White Keys (bottom), Max-1 row = Black Keys (top)
    auto rowIt = keysByRow.rbegin();
    if (rowIt == keysByRow.rend())
      return;

    // Bottom row (White Keys)
    auto& whiteKeys = rowIt->second;
    std::sort(whiteKeys.begin(), whiteKeys.end(), 
              [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                return a.second < b.second; // Sort by column
              });

    // Top row (Black Keys)
    ++rowIt;
    if (rowIt == keysByRow.rend())
      return; // Need both rows

    auto& blackKeys = rowIt->second;
    std::sort(blackKeys.begin(), blackKeys.end(),
              [](const std::pair<int, float>& a, const std::pair<int, float>& b) {
                return a.second < b.second; // Sort by column
              });

    // Map white keys to diatonic notes (C, D, E, F, G, A, B)
    // Start from degreeOffset to allow transposition
    int diatonicIndex = degreeOffset;
    for (const auto& whiteKeyPair : whiteKeys) {
      int whiteKeyCode = whiteKeyPair.first;
      float whiteCol = whiteKeyPair.second;

      // Calculate note for white key using Major scale
      int degree = diatonicIndex;
      int baseNote = ScaleUtilities::calculateMidiNote(rootNote, majorIntervals, degree);
      int relativeNote = baseNote - rootNote;
      keyToNoteCache[whiteKeyCode] = relativeNote;

      // Check for black key above this white key
      // Black keys are positioned between white keys (roughly col + 0.5)
      // Musical check: Does this white key have a sharp?
      // C, D, F, G, A have sharps (indices 0, 1, 3, 4, 5)
      // E and B don't have sharps (indices 2, 6)
      bool hasSharp = (diatonicIndex % 7 != 2) && (diatonicIndex % 7 != 6);

      if (hasSharp) {
        // Look for black key spatially aligned (within 0.3 units of whiteCol + 0.5)
        for (const auto& blackKeyPair : blackKeys) {
          int blackKeyCode = blackKeyPair.first;
          float blackCol = blackKeyPair.second;
          
          float expectedCol = whiteCol + 0.5f;
          if (std::abs(blackCol - expectedCol) < 0.3f) {
            // Map black key to sharp (white note + 1 semitone)
            keyToNoteCache[blackKeyCode] = relativeNote + 1;
            break; // One black key per white key
          }
        }
      }
      // If no sharp or black key not found, black key produces no sound (strict mode)

      ++diatonicIndex;
    }
  }
}

std::optional<MidiAction> Zone::processKey(InputID input, int globalChromTrans, int globalDegTrans) {
  // Check 1: Does input.deviceHandle match targetAliasHash?
  if (input.deviceHandle != targetAliasHash)
    return std::nullopt;

  // Lookup in cache
  auto it = keyToNoteCache.find(input.keyCode);
  if (it == keyToNoteCache.end())
    return std::nullopt; // Key not in cache

  // Get relative note from cache
  int relativeNote = it->second;

  // Calculate effective transposition (respect isTransposeLocked)
  int effDegTrans = isTransposeLocked ? 0 : globalDegTrans;
  int effChromTrans = isTransposeLocked ? 0 : globalChromTrans;

  // Calculate final note: root + relative + offsets + transpose
  // For degree transpose, we need to convert it to semitones
  // This is a simplification - in Piano mode, degree transpose doesn't make much sense
  // but we'll apply it as chromatic transpose for consistency
  int finalNote = rootNote + relativeNote + chromaticOffset + effChromTrans;

  // Clamp to valid MIDI range
  finalNote = juce::jlimit(0, 127, finalNote);

  // Return MIDI action (Type: Note, Channel: this->midiChannel, Data1: finalNote, Data2: 100)
  MidiAction action;
  action.type = ActionType::Note;
  action.channel = this->midiChannel;
  action.data1 = finalNote;
  action.data2 = 100;

  return action;
}

void Zone::removeKey(int keyCode) {
  inputKeyCodes.erase(
    std::remove(inputKeyCodes.begin(), inputKeyCodes.end(), keyCode),
    inputKeyCodes.end()
  );
}

juce::ValueTree Zone::toValueTree() const {
  juce::ValueTree vt("Zone");
  
  vt.setProperty("name", name, nullptr);
  vt.setProperty("targetAliasHash", static_cast<int64>(targetAliasHash), nullptr);
  vt.setProperty("zoneColor", zoneColor.toString(), nullptr);
  vt.setProperty("midiChannel", midiChannel, nullptr);
  vt.setProperty("rootNote", rootNote, nullptr);
  vt.setProperty("scaleName", scaleName, nullptr);
  vt.setProperty("chromaticOffset", chromaticOffset, nullptr);
  vt.setProperty("degreeOffset", degreeOffset, nullptr);
  vt.setProperty("isTransposeLocked", isTransposeLocked, nullptr);
  vt.setProperty("layoutStrategy", static_cast<int>(layoutStrategy), nullptr);
  vt.setProperty("gridInterval", gridInterval, nullptr);
  
  // Serialize inputKeyCodes as comma-separated string
  juce::StringArray keyCodesArray;
  for (int keyCode : inputKeyCodes) {
    keyCodesArray.add(juce::String(keyCode));
  }
  vt.setProperty("inputKeyCodes", keyCodesArray.joinIntoString(","), nullptr);
  
  return vt;
}

std::shared_ptr<Zone> Zone::fromValueTree(const juce::ValueTree& vt) {
  if (!vt.isValid() || !vt.hasType("Zone"))
    return nullptr;
  
  auto zone = std::make_shared<Zone>();
  
  zone->name = vt.getProperty("name", "Untitled Zone").toString();
  zone->targetAliasHash = static_cast<uintptr_t>(vt.getProperty("targetAliasHash", 0).operator int64());
  zone->rootNote = vt.getProperty("rootNote", 60);
  zone->midiChannel = vt.getProperty("midiChannel", 1);
  // Handle migration: if old "scale" property exists, convert to scaleName
  if (vt.hasProperty("scale")) {
    // Migrate old enum to scale name
    int scaleEnum = vt.getProperty("scale", static_cast<int>(1)); // 1 = Major
    switch (scaleEnum) {
      case 0: zone->scaleName = "Chromatic"; break;
      case 1: zone->scaleName = "Major"; break;
      case 2: zone->scaleName = "Minor"; break;
      case 3: zone->scaleName = "Pentatonic Major"; break;
      case 4: zone->scaleName = "Pentatonic Minor"; break;
      case 5: zone->scaleName = "Blues"; break;
      default: zone->scaleName = "Major"; break;
    }
  } else {
    zone->scaleName = vt.getProperty("scaleName", "Major").toString();
  }
  zone->chromaticOffset = vt.getProperty("chromaticOffset", 0);
  zone->degreeOffset = vt.getProperty("degreeOffset", 0);
  zone->isTransposeLocked = vt.getProperty("isTransposeLocked", false);
  zone->layoutStrategy = static_cast<LayoutStrategy>(vt.getProperty("layoutStrategy", static_cast<int>(LayoutStrategy::Linear)).operator int());
  zone->gridInterval = vt.getProperty("gridInterval", 5);
  
  // Load zone color (default to transparent if not found)
  juce::String colorStr = vt.getProperty("zoneColor", "").toString();
  if (colorStr.isNotEmpty()) {
    zone->zoneColor = juce::Colour::fromString(colorStr);
  } else {
    zone->zoneColor = juce::Colours::transparentBlack;
  }
  
  // Deserialize inputKeyCodes from comma-separated string
  juce::String keyCodesStr = vt.getProperty("inputKeyCodes", "").toString();
  if (keyCodesStr.isNotEmpty()) {
    juce::StringArray keyCodesArray;
    keyCodesArray.addTokens(keyCodesStr, ",", "");
    zone->inputKeyCodes.clear();
    for (const auto& keyStr : keyCodesArray) {
      int keyCode = keyStr.getIntValue();
      if (keyCode > 0) {
        zone->inputKeyCodes.push_back(keyCode);
      }
    }
  }
  
  return zone;
}
