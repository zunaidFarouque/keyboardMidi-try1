#include "Zone.h"
#include "KeyboardLayoutUtils.h"
#include "MidiNoteUtilities.h"
#include "ScaleUtilities.h"
#include <algorithm>
#include <map>
#include <set>

Zone::Zone()
    : name("Untitled Zone"), targetAliasHash(0), rootNote(60),
      scaleName("Major"), chromaticOffset(0), degreeOffset(0),
      layoutStrategy(LayoutStrategy::Linear), gridInterval(5),
      zoneColor(juce::Colours::transparentBlack), midiChannel(1) {
  // Cache will be rebuilt when zone is added to ZoneManager
}

void Zone::rebuildCache(const std::vector<int> &scaleIntervals,
                        int effectiveRoot) {
  keyToChordCache.clear();
  keyToLabelCache.clear();
  cacheEffectiveRoot = effectiveRoot;

  // Compilation: chord generation (ChordUtilities::generateChord,
  // ScaleUtilities) runs only here. Disable chords in Piano mode (Piano mode
  // ignores scales)
  bool useChords = (layoutStrategy != LayoutStrategy::Piano) &&
                   (chordType != ChordUtilities::ChordType::None);

  if (inputKeyCodes.empty())
    return;

  // Helper lambda to process a key's cache entry
  auto processKeyCache = [this, &scaleIntervals, effectiveRoot,
                          useChords](int keyCode, int degree, int baseNote) {
    // Generate chord or single note (absolute pitches)
    std::vector<ChordUtilities::ChordNote> chordNotes;
    if (useChords) {
      if (instrumentMode == InstrumentMode::Piano) {
        chordNotes = ChordUtilities::generateChordForPiano(
            effectiveRoot, scaleIntervals, degree, chordType,
            static_cast<ChordUtilities::PianoVoicingStyle>(pianoVoicingStyle),
            strictGhostHarmony, voicingMagnetSemitones);
      } else {
        int fretMin = (guitarPlayerPosition == GuitarPlayerPosition::Campfire)
                          ? 0
                          : juce::jlimit(0, 12, guitarFretAnchor);
        int fretMax = (guitarPlayerPosition == GuitarPlayerPosition::Campfire)
                          ? 4
                          : juce::jlimit(0, 24, guitarFretAnchor + 3);
        chordNotes = ChordUtilities::generateChordForGuitar(
            effectiveRoot, scaleIntervals, degree, chordType, fretMin, fretMax);
      }
    } else {
      chordNotes = {ChordUtilities::ChordNote(baseNote, false)};
    }

    // Add bass note if enabled (bass is the root of the chord, shifted down)
    if (addBassNote) {
      // Bass note is the root of the chord (baseNote), shifted down by
      // bassOctaveOffset octaves
      int bassPitch = baseNote + (bassOctaveOffset * 12);
      bassPitch = juce::jlimit(0, 127, bassPitch);
      chordNotes.emplace_back(bassPitch, false); // Bass is not a ghost note
      // Sort to ensure bass is first (lowest pitch)
      std::sort(
          chordNotes.begin(), chordNotes.end(),
          [](const ChordUtilities::ChordNote &a,
             const ChordUtilities::ChordNote &b) { return a.pitch < b.pitch; });
    }

    // Convert to relative chord (relative to effectiveRoot) for storage
    std::vector<ChordUtilities::ChordNote> relativeChord;
    for (const auto &cn : chordNotes) {
      relativeChord.emplace_back(cn.pitch - effectiveRoot, cn.isGhost);
    }
    keyToChordCache[keyCode] = relativeChord;

    // Cache label (Roman numeral or note name)
    juce::String label;
    if (showRomanNumerals && useChords) {
      label = ScaleUtilities::getRomanNumeral(degree, scaleIntervals);
    } else {
      label = MidiNoteUtilities::getMidiNoteName(baseNote);
    }
    keyToLabelCache[keyCode] = label;
  };

  if (layoutStrategy == LayoutStrategy::Linear) {
    // Linear mode: Map each key to its index in inputKeyCodes
    for (size_t i = 0; i < inputKeyCodes.size(); ++i) {
      int keyCode = inputKeyCodes[i];
      int degree = static_cast<int>(i) + degreeOffset;
      int baseNote = ScaleUtilities::calculateMidiNote(effectiveRoot,
                                                       scaleIntervals, degree);
      processKeyCache(keyCode, degree, baseNote);
    }
  } else if (layoutStrategy == LayoutStrategy::Grid) {
    // Grid mode: Calculate based on keyboard geometry
    const auto &layout = KeyboardLayoutUtils::getLayout();

    // Get anchor key (first key in inputKeyCodes)
    if (inputKeyCodes.empty())
      return;

    auto anchorKeyIt = layout.find(inputKeyCodes[0]);
    if (anchorKeyIt == layout.end())
      return; // Anchor key not in layout

    const auto &anchorKey = anchorKeyIt->second;

    // Process each key in inputKeyCodes
    for (int keyCode : inputKeyCodes) {
      auto currentKeyIt = layout.find(keyCode);
      if (currentKeyIt == layout.end())
        continue; // Skip keys not in layout

      const auto &currentKey = currentKeyIt->second;

      // Calculate delta
      int deltaCol =
          static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
      int deltaRow = currentKey.row - anchorKey.row;

      // Degree = deltaCol + (deltaRow * gridInterval)
      int degree = deltaCol + (deltaRow * gridInterval) + degreeOffset;
      int baseNote = ScaleUtilities::calculateMidiNote(effectiveRoot,
                                                       scaleIntervals, degree);
      processKeyCache(keyCode, degree, baseNote);
    }
  } else if (layoutStrategy == LayoutStrategy::Piano) {
    // Piano mode: Force Chromatic scale (Major scale intervals for white keys)
    // Major scale intervals: {0, 2, 4, 5, 7, 9, 11} = C, D, E, F, G, A, B
    const std::vector<int> majorIntervals = {0, 2, 4, 5, 7, 9, 11};
    const auto &layout = KeyboardLayoutUtils::getLayout();

    // Group keys by row
    std::map<int, std::vector<std::pair<int, float>>>
        keysByRow; // row -> [(keyCode, col), ...]

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

    // Identify rows: Max row = White Keys (bottom), Max-1 row = Black Keys
    // (top)
    auto rowIt = keysByRow.rbegin();
    if (rowIt == keysByRow.rend())
      return;

    // Bottom row (White Keys)
    auto &whiteKeys = rowIt->second;
    std::sort(
        whiteKeys.begin(), whiteKeys.end(),
        [](const std::pair<int, float> &a, const std::pair<int, float> &b) {
          return a.second < b.second; // Sort by column
        });

    // Top row (Black Keys)
    ++rowIt;
    if (rowIt == keysByRow.rend())
      return; // Need both rows

    auto &blackKeys = rowIt->second;
    std::sort(
        blackKeys.begin(), blackKeys.end(),
        [](const std::pair<int, float> &a, const std::pair<int, float> &b) {
          return a.second < b.second; // Sort by column
        });

    // Map white keys to diatonic notes (C, D, E, F, G, A, B)
    // Start from degreeOffset to allow transposition
    int diatonicIndex = degreeOffset;
    for (const auto &whiteKeyPair : whiteKeys) {
      int whiteKeyCode = whiteKeyPair.first;
      float whiteCol = whiteKeyPair.second;

      // Calculate note for white key using Major scale (Piano mode doesn't use
      // processKeyCache helper)
      int degree = diatonicIndex;
      int baseNote = ScaleUtilities::calculateMidiNote(effectiveRoot,
                                                       majorIntervals, degree);
      int relativeNote = baseNote - effectiveRoot;
      // Piano mode: Store single note only (chords disabled in Piano mode)
      keyToChordCache[whiteKeyCode] = {
          ChordUtilities::ChordNote(relativeNote, false)};

      // Cache label
      juce::String label;
      if (showRomanNumerals) {
        label = ScaleUtilities::getRomanNumeral(degree, majorIntervals);
      } else {
        label = MidiNoteUtilities::getMidiNoteName(baseNote);
      }
      keyToLabelCache[whiteKeyCode] = label;

      // Check for black key above this white key
      // Black keys are positioned between white keys (roughly col + 0.5)
      // Musical check: Does this white key have a sharp?
      // C, D, F, G, A have sharps (indices 0, 1, 3, 4, 5)
      // E and B don't have sharps (indices 2, 6)
      bool hasSharp = (diatonicIndex % 7 != 2) && (diatonicIndex % 7 != 6);

      if (hasSharp) {
        // Look for black key spatially aligned (within 0.3 units of whiteCol +
        // 0.5)
        for (const auto &blackKeyPair : blackKeys) {
          int blackKeyCode = blackKeyPair.first;
          float blackCol = blackKeyPair.second;

          float expectedCol = whiteCol + 0.5f;
          if (std::abs(blackCol - expectedCol) < 0.3f) {
            // Map black key to sharp (white note + 1 semitone)
            // Piano mode: Store single note only
            int whiteRelativeNote = baseNote - effectiveRoot;
            keyToChordCache[blackKeyCode] = {
                ChordUtilities::ChordNote(whiteRelativeNote + 1, false)};

            // Cache label for black key
            int blackNote = baseNote + 1;
            juce::String label;
            if (showRomanNumerals) {
              // For black keys, show the sharp version of the white key's Roman
              // numeral
              label =
                  ScaleUtilities::getRomanNumeral(degree, majorIntervals) + "#";
            } else {
              label = MidiNoteUtilities::getMidiNoteName(blackNote);
            }
            keyToLabelCache[blackKeyCode] = label;
            break; // One black key per white key
          }
        }
      }
      // If no sharp or black key not found, black key produces no sound (strict
      // mode)

      ++diatonicIndex;
    }
  }
}

// Play-time: O(1) hash lookup + O(k) transpose apply (k = chord size, typically
// 3–5). When scaleIntervals provided and degree transpose non-zero, applies
// scale-degree shift via ScaleUtilities.
std::optional<std::vector<ChordUtilities::ChordNote>>
Zone::getNotesForKey(int keyCode, int globalChromTrans, int globalDegTrans,
                    const std::vector<int> *scaleIntervals) {
  auto it = keyToChordCache.find(keyCode);
  if (it == keyToChordCache.end())
    return std::nullopt;

  const std::vector<ChordUtilities::ChordNote> &relativeChordNotes = it->second;
  int effChromTrans = ignoreGlobalTranspose ? 0 : globalChromTrans;
  int effDegTrans = ignoreGlobalTranspose ? 0 : globalDegTrans;
  bool applyDegreeTranspose = (scaleIntervals != nullptr &&
                              !scaleIntervals->empty() && effDegTrans != 0);

  std::vector<ChordUtilities::ChordNote> finalChordNotes;
  finalChordNotes.reserve(relativeChordNotes.size());
  for (const auto &cn : relativeChordNotes) {
    int finalNote;
    if (applyDegreeTranspose) {
      int baseNote =
          cacheEffectiveRoot + cn.pitch + chromaticOffset;
      int degree = ScaleUtilities::findScaleDegree(
          baseNote, cacheEffectiveRoot, *scaleIntervals);
      int newDegree = degree + effDegTrans;
      int noteInScale =
          ScaleUtilities::calculateMidiNote(cacheEffectiveRoot,
                                            *scaleIntervals, newDegree);
      finalNote = juce::jlimit(0, 127, noteInScale + effChromTrans);
    } else {
      finalNote = cacheEffectiveRoot + cn.pitch + chromaticOffset + effChromTrans;
      finalNote = juce::jlimit(0, 127, finalNote);
    }
    finalChordNotes.emplace_back(finalNote, cn.isGhost);
  }
  return finalChordNotes;
}

std::optional<MidiAction> Zone::processKey(InputID input, int globalChromTrans,
                                           int globalDegTrans,
                                           const std::vector<int> *scaleIntervals) {
  // Check 1: Does input.deviceHandle match targetAliasHash?
  if (input.deviceHandle != targetAliasHash)
    return std::nullopt;

  // Get chord notes for this key
  auto chordNotes = getNotesForKey(input.keyCode, globalChromTrans,
                                   globalDegTrans, scaleIntervals);
  if (!chordNotes.has_value() || chordNotes->empty())
    return std::nullopt;

  // Return MIDI action with first note (for backward compatibility)
  MidiAction action;
  action.type = ActionType::Note;
  action.channel = this->midiChannel;
  action.data1 = chordNotes->front().pitch; // First note of chord
  action.data2 = 100;

  return action;
}

void Zone::removeKey(int keyCode) {
  inputKeyCodes.erase(
      std::remove(inputKeyCodes.begin(), inputKeyCodes.end(), keyCode),
      inputKeyCodes.end());
}

juce::ValueTree Zone::toValueTree() const {
  juce::ValueTree vt("Zone");

  vt.setProperty("name", name, nullptr);
  vt.setProperty("layerID", layerID, nullptr);
  vt.setProperty("keyboardGroupId", keyboardGroupId, nullptr);
  vt.setProperty("targetAliasHash", static_cast<int64>(targetAliasHash),
                 nullptr);
  vt.setProperty("zoneColor", zoneColor.toString(), nullptr);
  vt.setProperty("midiChannel", midiChannel, nullptr);
  vt.setProperty("rootNote", rootNote, nullptr);
  vt.setProperty("scaleName", scaleName, nullptr);
  vt.setProperty("chromaticOffset", chromaticOffset, nullptr);
  vt.setProperty("degreeOffset", degreeOffset, nullptr);
  vt.setProperty("ignoreGlobalTranspose", ignoreGlobalTranspose, nullptr);
  vt.setProperty("layoutStrategy", static_cast<int>(layoutStrategy), nullptr);
  vt.setProperty("gridInterval", gridInterval, nullptr);
  vt.setProperty("chordType", static_cast<int>(chordType), nullptr);
  vt.setProperty("strumSpeedMs", strumSpeedMs, nullptr);
  vt.setProperty("strumTimingVariationOn", strumTimingVariationOn, nullptr);
  vt.setProperty("strumTimingVariationMs", strumTimingVariationMs, nullptr);
  vt.setProperty("playMode", static_cast<int>(playMode), nullptr);
  vt.setProperty("ignoreGlobalSustain", ignoreGlobalSustain, nullptr);
  vt.setProperty("releaseBehavior", static_cast<int>(releaseBehavior), nullptr);
  vt.setProperty("delayReleaseOn", delayReleaseOn, nullptr);
  vt.setProperty("overrideTimer", overrideTimer, nullptr);
  vt.setProperty("releaseDurationMs", releaseDurationMs, nullptr);
  vt.setProperty("baseVel", baseVelocity, nullptr);
  vt.setProperty("randVel", velocityRandom, nullptr);
  vt.setProperty("strictGhost", strictGhostHarmony, nullptr);
  vt.setProperty("ghostVelScale", ghostVelocityScale, nullptr);
  vt.setProperty("addBassNote", addBassNote, nullptr);
  vt.setProperty("bassOctaveOffset", bassOctaveOffset, nullptr);
  vt.setProperty("instrumentMode", static_cast<int>(instrumentMode), nullptr);
  vt.setProperty("pianoVoicingStyle", static_cast<int>(pianoVoicingStyle),
                 nullptr);
  vt.setProperty("voicingMagnetSemitones", voicingMagnetSemitones, nullptr);
  vt.setProperty("guitarPlayerPosition", static_cast<int>(guitarPlayerPosition),
                 nullptr);
  vt.setProperty("guitarFretAnchor", guitarFretAnchor, nullptr);
  vt.setProperty("strumPattern", static_cast<int>(strumPattern), nullptr);
  vt.setProperty("strumGhostNotes", strumGhostNotes, nullptr);
  vt.setProperty("showRomanNumerals", showRomanNumerals, nullptr);
  vt.setProperty("useGlobalScale", useGlobalScale, nullptr);
  vt.setProperty("useGlobalRoot", useGlobalRoot, nullptr);
  vt.setProperty("globalRootOctaveOffset", globalRootOctaveOffset, nullptr);
  vt.setProperty("polyphonyMode", static_cast<int>(polyphonyMode), nullptr);
  vt.setProperty("glideTimeMs", glideTimeMs, nullptr);
  vt.setProperty("isAdaptiveGlide", isAdaptiveGlide, nullptr);
  vt.setProperty("maxGlideTimeMs", maxGlideTimeMs, nullptr);

  // Serialize inputKeyCodes as comma-separated string
  juce::StringArray keyCodesArray;
  for (int keyCode : inputKeyCodes) {
    keyCodesArray.add(juce::String(keyCode));
  }
  vt.setProperty("inputKeyCodes", keyCodesArray.joinIntoString(","), nullptr);

  return vt;
}

std::shared_ptr<Zone> Zone::fromValueTree(const juce::ValueTree &vt) {
  if (!vt.isValid() || !vt.hasType("Zone"))
    return nullptr;

  auto zone = std::make_shared<Zone>();

  zone->name = vt.getProperty("name", "Untitled Zone").toString();
  zone->layerID = juce::jlimit(0, 8, (int)vt.getProperty("layerID", 0));
  zone->keyboardGroupId = juce::jmax(0, (int)vt.getProperty("keyboardGroupId", 0));
  zone->targetAliasHash = static_cast<uintptr_t>(
      vt.getProperty("targetAliasHash", 0).operator int64());
  zone->rootNote = vt.getProperty("rootNote", 60);
  zone->midiChannel = vt.getProperty("midiChannel", 1);
  // Handle migration: if old "scale" property exists, convert to scaleName
  if (vt.hasProperty("scale")) {
    // Migrate old enum to scale name
    int scaleEnum = vt.getProperty("scale", static_cast<int>(1)); // 1 = Major
    switch (scaleEnum) {
    case 0:
      zone->scaleName = "Chromatic";
      break;
    case 1:
      zone->scaleName = "Major";
      break;
    case 2:
      zone->scaleName = "Minor";
      break;
    case 3:
      zone->scaleName = "Pentatonic Major";
      break;
    case 4:
      zone->scaleName = "Pentatonic Minor";
      break;
    case 5:
      zone->scaleName = "Blues";
      break;
    default:
      zone->scaleName = "Major";
      break;
    }
  } else {
    zone->scaleName = vt.getProperty("scaleName", "Major").toString();
  }
  zone->chromaticOffset = vt.getProperty("chromaticOffset", 0);
  zone->degreeOffset = vt.getProperty("degreeOffset", 0);
  if (vt.hasProperty("ignoreGlobalTranspose"))
    zone->ignoreGlobalTranspose =
        vt.getProperty("ignoreGlobalTranspose", false);
  else
    zone->ignoreGlobalTranspose = vt.getProperty("isTransposeLocked", false);
  zone->layoutStrategy = static_cast<LayoutStrategy>(
      vt.getProperty("layoutStrategy", static_cast<int>(LayoutStrategy::Linear))
          .operator int());
  zone->gridInterval = vt.getProperty("gridInterval", 5);
  zone->chordType = static_cast<ChordUtilities::ChordType>(
      vt.getProperty("chordType",
                     static_cast<int>(ChordUtilities::ChordType::None))
          .operator int());
  // Legacy: "voicing" property no longer used (old architecture removed)
  zone->strumSpeedMs = vt.getProperty("strumSpeedMs", 0);
  zone->strumTimingVariationOn =
      vt.getProperty("strumTimingVariationOn", false);
  zone->strumTimingVariationMs = vt.getProperty("strumTimingVariationMs", 0);
  zone->playMode = static_cast<PlayMode>(
      vt.getProperty("playMode", static_cast<int>(PlayMode::Direct))
          .operator int());
  if (vt.hasProperty("ignoreGlobalSustain"))
    zone->ignoreGlobalSustain = vt.getProperty("ignoreGlobalSustain", false);
  else
    zone->ignoreGlobalSustain = !vt.getProperty("allowSustain", true);
  zone->releaseBehavior = static_cast<ReleaseBehavior>(
      vt.getProperty("releaseBehavior",
                     static_cast<int>(ReleaseBehavior::Normal))
          .operator int());
  zone->delayReleaseOn = vt.getProperty("delayReleaseOn", false);
  zone->overrideTimer = vt.getProperty("overrideTimer", false);
  zone->releaseDurationMs = vt.getProperty("releaseDurationMs", 0);
  zone->baseVelocity = vt.getProperty("baseVel", 100);
  zone->velocityRandom = vt.getProperty("randVel", 0);
  zone->strictGhostHarmony = vt.getProperty("strictGhost", true);
  zone->ghostVelocityScale =
      static_cast<float>(vt.getProperty("ghostVelScale", 0.6));

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
    for (const auto &keyStr : keyCodesArray) {
      int keyCode = keyStr.getIntValue();
      if (keyCode > 0) {
        zone->inputKeyCodes.push_back(keyCode);
      }
    }
  }

  // Load new properties (with defaults)
  zone->addBassNote = vt.getProperty("addBassNote", false);
  zone->bassOctaveOffset = vt.getProperty("bassOctaveOffset", -1);
  zone->instrumentMode = static_cast<InstrumentMode>(
      juce::jlimit(0, 1, (int)vt.getProperty("instrumentMode", 0)));
  zone->pianoVoicingStyle = static_cast<PianoVoicingStyle>(
      juce::jlimit(0, 2, (int)vt.getProperty("pianoVoicingStyle", 1)));
  zone->voicingMagnetSemitones =
      juce::jlimit(-6, 6, (int)vt.getProperty("voicingMagnetSemitones", 0));
  zone->guitarPlayerPosition = static_cast<GuitarPlayerPosition>(
      juce::jlimit(0, 1, (int)vt.getProperty("guitarPlayerPosition", 0)));
  zone->guitarFretAnchor =
      juce::jlimit(1, 12, (int)vt.getProperty("guitarFretAnchor", 5));
  zone->strumPattern = static_cast<StrumPattern>(
      juce::jlimit(0, 2, (int)vt.getProperty("strumPattern", 0)));
  zone->strumGhostNotes = vt.getProperty("strumGhostNotes", false);
  zone->showRomanNumerals = vt.getProperty("showRomanNumerals", false);
  zone->useGlobalScale = vt.getProperty("useGlobalScale", false);
  zone->useGlobalRoot = vt.getProperty("useGlobalRoot", false);
  zone->globalRootOctaveOffset =
      juce::jlimit(-2, 2, (int)vt.getProperty("globalRootOctaveOffset", 0));
  zone->polyphonyMode = static_cast<PolyphonyMode>(
      vt.getProperty("polyphonyMode", static_cast<int>(PolyphonyMode::Poly))
          .operator int());
  zone->glideTimeMs = vt.getProperty("glideTimeMs", 50);
  zone->isAdaptiveGlide = vt.getProperty("isAdaptiveGlide", false);
  zone->maxGlideTimeMs = vt.getProperty("maxGlideTimeMs", 200);

  return zone;
}

juce::String Zone::getKeyLabel(int keyCode) const {
  auto it = keyToLabelCache.find(keyCode);
  if (it != keyToLabelCache.end()) {
    return it->second;
  }
  return ""; // Return empty string if not found
}
