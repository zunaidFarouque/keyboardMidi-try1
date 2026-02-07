#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Touchpad tab type selector. Mixer = vertical CC faders; more types later.
enum class TouchpadType { Mixer = 0 };

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

// Config for one touchpad strip (serialized in preset / session).
// type determines which controls apply; Mixer = vertical CC faders.
struct TouchpadMixerConfig {
  TouchpadType type = TouchpadType::Mixer;
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

// Precomputed mode flags (avoids per-frame branching)
static constexpr uint8_t kMixerModeUseFinger1 = 1 << 0;   // Quick vs Precision
static constexpr uint8_t kMixerModeLock = 1 << 1;         // Lock vs Free
static constexpr uint8_t kMixerModeRelative = 1 << 2;     // Absolute vs Relative
static constexpr uint8_t kMixerModeMuteButtons = 1 << 3;  // Mute buttons enabled
static constexpr float kMuteButtonRegionTop = 0.85f;      // Bottom 15% = mute

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
  uint8_t modeFlags = 0;        // kMixerMode* bits
  float effectiveYScale = 1.0f; // 1.0 or 1/0.85 when muteButtonsEnabled
  TouchpadMixerQuickPrecision quickPrecision =
      TouchpadMixerQuickPrecision::Quick;
  TouchpadMixerAbsRel absRel = TouchpadMixerAbsRel::Absolute;
  TouchpadMixerLockFree lockFree = TouchpadMixerLockFree::Free;
  bool muteButtonsEnabled = false;
};
