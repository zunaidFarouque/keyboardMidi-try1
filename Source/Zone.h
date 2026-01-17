#pragma once
#include "MappingTypes.h"
#include "ScaleUtilities.h"
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

  // Performance cache: Pre-compiled key-to-note mappings
  std::unordered_map<int, int> keyToNoteCache; // Key Code -> Relative Note Number (before transpose)

  // Rebuild the cache when zone properties change
  void rebuildCache(const std::vector<int>& intervals);

  // Process a key input and return MIDI action if this zone matches
  // Note: Intervals are now used only during rebuildCache, not during processKey
  std::optional<MidiAction> processKey(InputID input, int globalChromTrans, int globalDegTrans);

  // Remove a key from inputKeyCodes
  void removeKey(int keyCode);

  // Serialization
  juce::ValueTree toValueTree() const;
  static std::shared_ptr<Zone> fromValueTree(const juce::ValueTree& vt);
};
