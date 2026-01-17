#pragma once
#include <JuceHeader.h>
#include <map>

struct KeyGeometry {
  int row;      // Row number (0 = top row, 4 = spacebar)
  float col;    // Column position (offset from left)
  float width;  // Key width multiplier (1.0 = standard width)
  juce::String label; // Physical key label (e.g., "Q", "Space")
};

class KeyboardLayoutUtils {
public:
  // Get the keyboard layout map (keyCode -> KeyGeometry)
  static const std::map<int, KeyGeometry> &getLayout();

  // Calculate screen bounds for a key
  static juce::Rectangle<float> getKeyBounds(int keyCode, float keySize, float padding);
};
