#pragma once
#include "MappingDefinition.h"
#include "TouchpadMixerTypes.h"

// Schema-driven UI for Touchpad Mixer strip (same pattern as MappingDefinition +
// MappingInspector). Use TouchpadMixerDefinition::getSchema() and build
// controls from it; read/write TouchpadMixerConfig by propertyId.
class TouchpadMixerDefinition {
public:
  static InspectorSchema getSchema();
};
