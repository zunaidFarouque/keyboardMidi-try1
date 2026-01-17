#pragma once
#include <JuceHeader.h>
#include <cstdint>

class KeyNameUtilities {
public:
  // Get a friendly name for a virtual key code
  static juce::String getKeyName(int virtualKeyCode);

  // Get a friendly device name from a device handle
  static juce::String getFriendlyDeviceName(uintptr_t deviceHandle);
};
