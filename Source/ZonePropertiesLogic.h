#pragma once
#include "Zone.h"
#include <JuceHeader.h>

namespace ZonePropertiesLogic {

// Applies a UI control value to a zone member (same rules as ZonePropertiesPanel
// sliders/combos/toggles). value: for Slider = int/double, for ComboBox =
// selected option id (int), for Toggle = bool. Returns true if propertyKey was
// recognized and applied.
bool setZonePropertyFromKey(Zone *zone, const juce::String &propertyKey,
                            const juce::var &value);

} // namespace ZonePropertiesLogic
