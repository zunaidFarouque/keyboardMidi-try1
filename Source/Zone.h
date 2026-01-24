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

  enum class ReleaseBehavior {
    Normal,  // Release to keep playing for N ms, then stop
    Sustain  // Release to sustain for N ms (notes continue, then sustain)
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
  ChordUtilities::Voicing voicing = ChordUtilities::Voicing::RootPosition; // Chord voicing
  int strumSpeedMs = 0; // Strum speed in milliseconds (0 = no strum, all notes at once)
  PlayMode playMode = PlayMode::Direct; // Play mode (Direct = immediate, Strum = buffered)
  bool allowSustain = true; // If false (e.g. Drums), sustain pedal does not hold notes
  ReleaseBehavior releaseBehavior = ReleaseBehavior::Normal; // How to handle key release in strum mode
  int releaseDurationMs = 0; // Continue playing for N ms after release (0 = stop immediately)
  int baseVelocity = 100; // Base MIDI velocity (1-127)
  int velocityRandom = 0; // Velocity randomization range (0-64)
  bool strictGhostHarmony = true; // Ghost note harmony mode (true = strict 1/5, false = loose 7/9)
  float ghostVelocityScale = 0.6f; // Velocity multiplier for ghost notes (0.0-1.0)
  bool addBassNote = false; // If true, add a bass note (root shifted down)
  int bassOctaveOffset = -1; // Octave offset for bass note (-3 to -1)
  bool showRomanNumerals = false; // If true, display Roman numerals instead of note names

  // Performance cache: Pre-compiled key-to-chord mappings (compilation strategy).
  // Config-time: rebuildCache() runs ChordUtilities::generateChord, ScaleUtilities; fills this map.
  // Play-time: getNotesForKey() does O(1) find + O(k) transpose (k = chord size). No chord/scale math.
  std::unordered_map<int, std::vector<ChordUtilities::ChordNote>> keyToChordCache; // keyCode -> chord notes (with ghost flags)
  std::unordered_map<int, juce::String> keyToLabelCache; // keyCode -> display label (note name or Roman numeral)

  // Config-time: (re)build keyToChordCache when zone/scale/chord/keys change.
  void rebuildCache(const std::vector<int>& intervals);

  // Play-time: O(1) lookup + O(k) transpose. Returns final MIDI chord notes (with ghost flags) or nullopt if not in zone.
  std::optional<std::vector<ChordUtilities::ChordNote>> getNotesForKey(int keyCode, int globalChromTrans, int globalDegTrans);
  
  // Get display label for a key (note name or Roman numeral)
  juce::String getKeyLabel(int keyCode) const;

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
