#pragma once
#include "MappingTypes.h"
#include "ScaleUtilities.h"
#include <JuceHeader.h>
#include <vector>
#include <optional>

class Zone {
public:
  enum class LayoutStrategy {
    Linear,
    Grid
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

  // Process a key input and return MIDI action if this zone matches
  // Intervals are provided by ZoneManager (looked up from ScaleLibrary)
  std::optional<MidiAction> processKey(InputID input, const std::vector<int>& intervals, int globalChromTrans, int globalDegTrans);

  // Remove a key from inputKeyCodes
  void removeKey(int keyCode);

  // Serialization
  juce::ValueTree toValueTree() const;
  static std::shared_ptr<Zone> fromValueTree(const juce::ValueTree& vt);
};
