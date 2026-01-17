#pragma once
#include "MappingTypes.h"
#include "ScaleUtilities.h"
#include "ChordUtilities.h"
#include <JuceHeader.h>
#include <vector>
#include <optional>
#include <unordered_map>

class Zone {
public:
  enum class LayoutStrategy {
    Linear,
    Grid,
    Piano
  };

  enum class PlayMode {
    Direct,  // Play immediately on key press
    Strum    // Buffer notes, play on trigger key
  };

  Zone();

  // Properties
  juce::String name;
  uintptr_t targetAliasHash; // The Alias this zone listens to (0 = "Any / Master")
  std::vector<int> inputKeyCodes; // The physical keys, ordered
  int rootNote; // Base MIDI note
  juce::String scaleName; // Scale name (looked up from ScaleLibrary)
  int chromaticOffset; // Global transpose override
  int degreeOffset; // Scale degree shift
  bool isTransposeLocked; // If true, ignore global transpose
  LayoutStrategy layoutStrategy = LayoutStrategy::Linear; // Layout calculation method
  int gridInterval = 5; // For Grid mode: semitones per row (default 5 = perfect 4th)
  juce::Colour zoneColor; // Visual color for this zone
  int midiChannel = 1; // MIDI output channel (1-16)
  ChordUtilities::ChordType chordType = ChordUtilities::ChordType::None; // Chord type (None = single note)
  ChordUtilities::Voicing voicing = ChordUtilities::Voicing::Close; // Chord voicing
  int strumSpeedMs = 0; // Strum speed in milliseconds (0 = no strum, all notes at once)
  PlayMode playMode = PlayMode::Direct; // Play mode (Direct = immediate, Strum = buffered)

  // Performance cache: Pre-compiled key-to-chord mappings (compilation strategy).
  // Config-time: rebuildCache() runs ChordUtilities::generateChord, ScaleUtilities; fills this map.
  // Play-time: getNotesForKey() does O(1) find + O(k) transpose (k = chord size). No chord/scale math.
  std::unordered_map<int, std::vector<int>> keyToChordCache; // keyCode -> relative notes (to root)

  // Config-time: (re)build keyToChordCache when zone/scale/chord/keys change.
  void rebuildCache(const std::vector<int>& intervals);

  // Play-time: O(1) lookup + O(k) transpose. Returns final MIDI notes or nullopt if not in zone.
  std::optional<std::vector<int>> getNotesForKey(int keyCode, int globalChromTrans, int globalDegTrans);

  // Process a key input and return MIDI action if this zone matches
  // Note: Returns first note of chord for backward compatibility
  std::optional<MidiAction> processKey(InputID input, int globalChromTrans, int globalDegTrans);

  // Remove a key from inputKeyCodes
  void removeKey(int keyCode);

  // Get input key codes (for ZoneManager lookup table)
  const std::vector<int>& getInputKeyCodes() const { return inputKeyCodes; }

  // Serialization
  juce::ValueTree toValueTree() const;
  static std::shared_ptr<Zone> fromValueTree(const juce::ValueTree& vt);
};
