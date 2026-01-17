#include "Zone.h"
#include "ScaleUtilities.h"
#include "KeyboardLayoutUtils.h"
#include <algorithm>

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
      zoneColor(juce::Colours::transparentBlack) {
}

std::optional<MidiAction> Zone::processKey(InputID input, const std::vector<int>& intervals, int globalChromTrans, int globalDegTrans) {
  // Check 1: Does input.deviceHandle match targetAliasHash?
  if (input.deviceHandle != targetAliasHash)
    return std::nullopt;

  int degree = 0;

  if (layoutStrategy == LayoutStrategy::Linear) {
    // Linear mode: Find index of input.keyCode in inputKeyCodes
    int index = -1;
    for (size_t i = 0; i < inputKeyCodes.size(); ++i) {
      if (inputKeyCodes[i] == input.keyCode) {
        index = static_cast<int>(i);
        break;
      }
    }

    if (index < 0)
      return std::nullopt;

    degree = index;
  } else {
    // Grid mode: Calculate degree based on keyboard geometry
    const auto& layout = KeyboardLayoutUtils::getLayout();
    
    // Check if current key is in the layout
    auto currentKeyIt = layout.find(input.keyCode);
    if (currentKeyIt == layout.end())
      return std::nullopt;

    // Get anchor key (first key in inputKeyCodes)
    if (inputKeyCodes.empty())
      return std::nullopt;

    auto anchorKeyIt = layout.find(inputKeyCodes[0]);
    if (anchorKeyIt == layout.end())
      return std::nullopt; // Anchor key not in layout - fallback to Linear?

    const auto& currentKey = currentKeyIt->second;
    const auto& anchorKey = anchorKeyIt->second;

    // Calculate delta
    int deltaCol = static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
    int deltaRow = currentKey.row - anchorKey.row;

    // Degree = deltaCol + (deltaRow * gridInterval)
    degree = deltaCol + (deltaRow * gridInterval);
  }

  // Calculate effective transposition (respect isTransposeLocked)
  int effDegTrans = isTransposeLocked ? 0 : globalDegTrans;
  int effChromTrans = isTransposeLocked ? 0 : globalChromTrans;

  // Calculate final degree and note
  int finalDegree = degree + degreeOffset + effDegTrans;
  int baseNote = ScaleUtilities::calculateMidiNote(rootNote, intervals, finalDegree);
  int finalNote = baseNote + chromaticOffset + effChromTrans;

  // Clamp to valid MIDI range
  finalNote = juce::jlimit(0, 127, finalNote);

  // Return MIDI action (Type: Note, Channel: 1, Data1: finalNote, Data2: 100)
  MidiAction action;
  action.type = ActionType::Note;
  action.channel = 1;
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
