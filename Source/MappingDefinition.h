#pragma once
#include "MappingTypes.h"
#include <JuceHeader.h>
#include <map>
#include <vector>

struct InspectorControl {
  juce::String propertyId; // ValueTree property (e.g. "data1")
  juce::String label;      // Display Label
  bool sameLine = false;   // Phase 55.6: share row with previous control
  float widthWeight =
      1.0f; // Phase 55.6: fraction of row width (e.g. 0.5 = half)
  bool autoWidth =
      false; // Phase 55.10: if true, fit to content; ignore widthWeight

  // Phase 55.7: If set, this control is enabled only when the target property
  // is true
  juce::String enabledConditionProperty;

  enum class Type { Slider, ComboBox, Toggle, LabelOnly, Color, Separator, TextEditor };
  juce::Justification separatorAlign =
      juce::Justification::centred; // Phase 55.9: for Separator type
  Type controlType = Type::Slider;
  bool isEnabled = true;

  // Slider Constraints
  double min = 0.0;
  double max = 127.0;
  double step = 1.0;
  juce::String suffix;

  // When > 0, slider displays in semitones (-valueScaleRange to
  // +valueScaleRange) and stored value (e.g. data2) is raw 0-16383; used for
  // PitchBend peak.
  int valueScaleRange = 0;

  // Formatting
  enum class Format { Integer, NoteName, CommandName, LayerName };
  Format valueFormat = Format::Integer;

  // ComboBox Options (ID -> Text)
  std::map<int, juce::String> options;
};

using InspectorSchema = std::vector<InspectorControl>;

class MappingDefinition {
public:
  // Factory: Inspects the mapping state and returns the UI schema.
  // pitchBendRange: global PB range in semitones (for Expression PitchBend peak
  // slider).
  static InspectorSchema getSchema(const juce::ValueTree &mapping,
                                   int pitchBendRange = 12);

  // Helper to get friendly name for ActionType (e.g. "Note", "CC")
  static juce::String getTypeName(ActionType type);

  // Command ID -> display name (e.g. "Sustain Momentary", "Layer Momentary")
  static std::map<int, juce::String> getCommandOptions();

  // Layer index -> display name (0 -> "Base", 1 -> "Layer 1", ...)
  static std::map<int, juce::String> getLayerOptions();

  // Phase 55.9: Helper to create separator items for schema
  static InspectorControl
  createSeparator(const juce::String &label = "",
                  juce::Justification align = juce::Justification::centred);

  // Touchpad mapping: event id (0-10) -> display name
  static juce::String getTouchpadEventName(int eventId);
  static std::map<int, juce::String> getTouchpadEventOptions();

  // Single source of truth for mapping enabled state (property "enabled", default true)
  static bool isMappingEnabled(const juce::ValueTree &mapping);
};
