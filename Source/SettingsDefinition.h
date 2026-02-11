#pragma once

#include "MappingDefinition.h"

// Schema-driven UI for the Settings tab.
// Reuses InspectorControl / InspectorSchema so layout logic matches
// MappingInspector and TouchpadMixerEditorComponent.
class SettingsDefinition {
public:
  /// Returns the full schema for the Settings panel.
  static InspectorSchema getSchema();
};

