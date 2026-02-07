#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Touchpad Mixer: divide touchpad into N vertical faders (CC only).
// Quick/Precision x Absolute/Relative x Lock/Free.

enum class TouchpadMixerQuickPrecision {
  Quick = 0,    // One finger = direct CC
  Precision = 1 // One finger = overlay only; second finger = apply CC
};

enum class TouchpadMixerAbsRel {
  Absolute = 0, // Y position maps to CC value
  Relative = 1  // Y movement (delta) adjusts CC
};

enum class TouchpadMixerLockFree {
  Lock = 0, // First fader touched is fixed until finger lift
  Free = 1  // Finger can swipe to another fader
};

// Config for one mixer strip (serialized in preset / session).
struct TouchpadMixerConfig {
  std::string name = "Touchpad Mixer";
  int layerId = 0;
  int numFaders = 5;
  int ccStart = 50;
  int midiChannel = 1;
  float inputMin = 0.0f;
  float inputMax = 1.0f;
  int outputMin = 0;
  int outputMax = 127;
  TouchpadMixerQuickPrecision quickPrecision =
      TouchpadMixerQuickPrecision::Quick;
  TouchpadMixerAbsRel absRel = TouchpadMixerAbsRel::Absolute;
  TouchpadMixerLockFree lockFree = TouchpadMixerLockFree::Free;
  bool muteButtonsEnabled = false;
  // Mute = send CC 0 for that fader until toggled again (no separate mute CC)
};

// Compiled entry for runtime (no ValueTree in hot path).
struct TouchpadMixerEntry {
  int layerId = 0;
  int numFaders = 0;
  int ccStart = 0;
  int midiChannel = 1;
  float inputMin = 0.0f;
  float inputMax = 1.0f;
  float invInputRange = 1.0f;
  int outputMin = 0;
  int outputMax = 127;
  TouchpadMixerQuickPrecision quickPrecision =
      TouchpadMixerQuickPrecision::Quick;
  TouchpadMixerAbsRel absRel = TouchpadMixerAbsRel::Absolute;
  TouchpadMixerLockFree lockFree = TouchpadMixerLockFree::Free;
  bool muteButtonsEnabled = false;
};
