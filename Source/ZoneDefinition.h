#pragma once
#include "Zone.h"
#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>

class Zone;

// Descriptor for one zone property control or special row. Visibility is
// encoded in the schema (only include rows that pass visible(zone)).
struct ZoneControl {
  juce::String label;
  bool sameLine = false;
  float widthWeight = 1.0f;
  bool autoWidth = false;

  enum class Type {
    Slider,
    ComboBox,
    Toggle,
    LabelOnly,
    Separator,
    CustomAlias,
    CustomScale,
    CustomKeyAssign,
    CustomChipList,
    CustomColor,
    CustomLayer,
    CustomName,
  };
  Type controlType = Type::Slider;
  juce::Justification separatorAlign = juce::Justification::centred;

  // For standard Slider/ComboBox/Toggle: which Zone member (panel maps to get/set)
  juce::String propertyKey;

  // If set, only include this control when visible(z) is true
  std::function<bool(const Zone *)> visible;

  // Slider
  double min = 0.0;
  double max = 127.0;
  double step = 1.0;
  juce::String suffix;

  // ComboBox: id -> display text
  std::map<int, juce::String> options;

  // If true, changing this property requires rebuildZoneCache()
  bool affectsCache = false;
};

using ZoneSchema = std::vector<ZoneControl>;

class ZoneDefinition {
public:
  // Returns the UI schema for the given zone. Only includes controls that
  // pass their visibility predicate (if any). Order defines UI order.
  static ZoneSchema getSchema(const Zone *zone);

  static ZoneControl createSeparator(
      const juce::String &label = "",
      juce::Justification align = juce::Justification::centred);
};
