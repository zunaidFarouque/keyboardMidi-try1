#pragma once

#include "TouchpadLayoutTypes.h"

#include <JuceHeader.h>

// Core logic for applying Touchpad editor UI changes to layout and mapping
// configs. This mirrors the rules currently used in TouchpadEditorPanel but
// lives in MIDIQy_Core so it is testable without GUI.
namespace TouchpadEditorLogic {

// Apply a property change coming from the Touchpad editor UI.
// - If layoutCfg is non-null, layout-related properties are applied there.
// - If mappingCfg is non-null and mappingCfg->mapping.isValid(), mapping
//   header and touchpad-specific mapping properties are applied there.
// Returns true if anything was changed.
bool applyConfig(TouchpadLayoutConfig *layoutCfg,
                 TouchpadMappingConfig *mappingCfg,
                 const juce::String &propertyId, const juce::var &value);

// Apply a property change to the underlying mapping ValueTree for touchpad
// mappings (release behavior, CC mode, encoder settings, pitch pad, etc.).
// Returns true if the mapping ValueTree was changed.
bool applyMappingValueProperty(juce::ValueTree &mapping,
                               const juce::String &propertyId,
                               const juce::var &value);

} // namespace TouchpadEditorLogic

