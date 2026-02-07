#pragma once
#include "MappingDefinition.h"
#include "TouchpadMixerTypes.h"

// Schema-driven UI for Touchpad strip (same pattern as MappingDefinition +
// MappingInspector). Use getSchema(type) and build controls from it; read/write
// TouchpadMixerConfig by propertyId.
class TouchpadMixerDefinition {
public:
  /// Returns schema for the given strip type. Use when building editor UI.
  static InspectorSchema getSchema(TouchpadType type);
  /// Backward compat: returns Mixer schema.
  static InspectorSchema getSchema() { return getSchema(TouchpadType::Mixer); }
};
