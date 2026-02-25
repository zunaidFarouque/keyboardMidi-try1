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

  // Optional default when property is missing; slider/UI shows this until set.
  juce::var defaultValue;

  // Phase 55.7: If set, this control is enabled only when the target property
  // is true
  juce::String enabledConditionProperty;

  enum class Type {
    Slider,
    ComboBox,
    Toggle,
    LabelOnly,
    Color,
    Separator,
    TextEditor,
    Button
  };
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

// Schema contract (shared across editors)
//
// MappingDefinition::getSchema is the single source of truth for mapping UI
// controls that are shared across all editors (Mappings tab, Touchpad tab,
// and any future consumers).
//
// - Keyboard vs touchpad mappings are distinguished by the mapping ValueTree
//   property "inputAlias". When inputAlias is "Touchpad", the mapping is
//   treated as a touchpad mapping (isTouchpadMapping == true).
// - The forTouchpadEditor flag tells getSchema where the schema will be
//   rendered: false for the generic Mappings tab, true for the Touchpad tab.
//
// Rules:
// - Shared controls (type, CC target, controller number, envelope controls,
//   basic Expression controls, etc.) must be added unconditionally so both
//   editors see the same core parameters.
// - Touchpad‑only controls (Pitch pad, Slide, Encoder, CC release behaviour,
//   touchpad hold behaviour, etc.) must always be gated by the mapping alias
//   (inputAlias == "Touchpad") and, when they are only meaningful in the
//   Touchpad tab UI, also by forTouchpadEditor.
// - Keyboard mappings should never see touchpad‑only controls. If a new
//   control is only relevant for a specific UI (for example, a Touchpad‑tab‑
//   only layout/header widget), it should be defined in that editor
//   (TouchpadMixerDefinition, TouchpadMixerEditorComponent, MappingInspector)
//   instead of being added here without alias/forTouchpadEditor guards.
//
// Any changes to this schema should be validated in both contexts:
// - MappingInspector::rebuildUI (Mappings tab)
// - TouchpadMixerEditorComponent::rebuildUI (Touchpad tab)
class MappingDefinition {
public:
  // Factory: Inspects the mapping state and returns the UI schema.
  // pitchBendRange: global PB range in semitones (for Expression PitchBend peak
  // slider).
  // forTouchpadEditor: when true, omit Enabled and channel from schema (they
  // live in the touchpad header).
  static InspectorSchema getSchema(const juce::ValueTree &mapping,
                                   int pitchBendRange = 12,
                                   bool forTouchpadEditor = false);

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

  // Single source of truth for mapping enabled state (property "enabled", default true)
  static bool isMappingEnabled(const juce::ValueTree &mapping);

  // Single source of truth for default value when a property is missing.
  // Returns juce::var() if no default is defined.
  static juce::var getDefaultValue(juce::StringRef propertyId);
};
