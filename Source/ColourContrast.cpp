#include "ColourContrast.h"

namespace ColourContrast {

juce::Colour getTextColorForKeyFill(juce::Colour keyFillColor) {
  float brightness = keyFillColor.getPerceivedBrightness();
  return (brightness > 0.7f) ? juce::Colours::black : juce::Colours::white;
}

} // namespace ColourContrast
