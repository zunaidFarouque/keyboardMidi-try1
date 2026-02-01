#pragma once
#include <JuceHeader.h>

namespace ColourContrast {

/// Returns black or white for legible text on the given key fill color.
/// Used by the visualizer so text contrast is based on the key fill (not
/// backdrop/underlay).
juce::Colour getTextColorForKeyFill(juce::Colour keyFillColor);

} // namespace ColourContrast
