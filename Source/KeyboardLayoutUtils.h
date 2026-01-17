#pragma once
#include <JuceHeader.h>
#include <map>

struct KeyGeometry {
  int row;      // Row number (-1 = function row, 0-4 = main block, etc.)
  float col;    // Column position (offset from left)
  float width;  // Key width multiplier (1.0 = standard width)
  float height; // Key height multiplier (1.0 = standard height, 2.0 = tall keys like Enter/+)
  juce::String label; // Physical key label (e.g., "Q", "Space")
  
  KeyGeometry() : row(0), col(0.0f), width(1.0f), height(1.0f), label("") {}
  KeyGeometry(int r, float c, float w, const juce::String& l) : row(r), col(c), width(w), height(1.0f), label(l) {}
  KeyGeometry(int r, float c, float w, const juce::String& l, float h) : row(r), col(c), width(w), height(h), label(l) {}
};

class KeyboardLayoutUtils {
public:
  // Get the keyboard layout map (keyCode -> KeyGeometry)
  static const std::map<int, KeyGeometry> &getLayout();

  // Calculate screen bounds for a key
  static juce::Rectangle<float> getKeyBounds(int keyCode, float keySize, float padding);
};
