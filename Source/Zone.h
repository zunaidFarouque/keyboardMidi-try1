#pragma once
#include "MappingTypes.h"
#include "ScaleUtilities.h"
#include <JuceHeader.h>
#include <vector>
#include <optional>

class Zone {
public:
  Zone();

  // Properties
  juce::String name;
  uintptr_t targetAliasHash; // The Alias this zone listens to (0 = "Any / Master")
  std::vector<int> inputKeyCodes; // The physical keys, ordered
  int rootNote; // Base MIDI note
  ScaleUtilities::ScaleType scale;
  int chromaticOffset; // Global transpose override
  int degreeOffset; // Scale degree shift
  bool isTransposeLocked; // If true, ignore global transpose

  // Process a key input and return MIDI action if this zone matches
  std::optional<MidiAction> processKey(InputID input, int globalChromTrans, int globalDegTrans);
};
