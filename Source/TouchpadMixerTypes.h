#pragma once
#include <cstdint>
#include <string>
#include <vector>

// Touchpad tab type selector. Mixer = vertical CC faders; DrumPad = finger drumming grid.
enum class TouchpadType { Mixer = 0, DrumPad = 1 };

// Universal region for touchpad layouts. Defines where on the touchpad (0-1 normalized)
// this layout is active. Content is stretched to fit within the region.
struct TouchpadLayoutRegion {
  float left = 0.0f;
  float top = 0.0f;
  float right = 1.0f;
  float bottom = 1.0f;
  // Invariant: left < right, top < bottom
};

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
// type determines which controls apply; Mixer = vertical CC faders; DrumPad = finger drumming.
struct TouchpadMixerConfig {
  TouchpadType type = TouchpadType::Mixer;
  std::string name = "Touchpad Mixer";
  int layerId = 0;
  // Mixer fields (used when type == Mixer)
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

  // Universal: where this layout is active on the touchpad (default: full pad)
  TouchpadLayoutRegion region;

  // DrumPad fields (used when type == DrumPad)
  int drumPadRows = 2;
  int drumPadColumns = 4;
  int drumPadMidiNoteStart = 60;
  int drumPadBaseVelocity = 100;
  int drumPadVelocityRandom = 0;
  float drumPadDeadZoneLeft = 0.0f;
  float drumPadDeadZoneRight = 0.0f;
  float drumPadDeadZoneTop = 0.0f;
  float drumPadDeadZoneBottom = 0.0f;
};

// Precomputed mode flags (avoids per-frame branching)
static constexpr uint8_t kMixerModeUseFinger1 = 1 << 0;  // Quick vs Precision
static constexpr uint8_t kMixerModeLock = 1 << 1;        // Lock vs Free
static constexpr uint8_t kMixerModeRelative = 1 << 2;    // Absolute vs Relative
static constexpr uint8_t kMixerModeMuteButtons = 1 << 3; // Mute buttons enabled
static constexpr float kMuteButtonRegionTop = 0.85f;     // Bottom 15% = mute

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
  // Region: where this layout is active; precomputed for O(1) lookup
  float regionLeft = 0.0f, regionTop = 0.0f, regionRight = 1.0f,
        regionBottom = 1.0f;
  float invRegionWidth = 1.0f, invRegionHeight = 1.0f;
};

// Compiled entry for DrumPad strip (O(1) runtime).
// Grid is stretched to fit within region; region defines active area.
struct TouchpadDrumPadEntry {
  int layerId = 0;
  int rows = 0;
  int columns = 0;
  int numPads = 0;
  int midiNoteStart = 0;
  int midiChannel = 1;
  int baseVelocity = 100;
  int velocityRandom = 0;
  float regionLeft = 0.0f, regionTop = 0.0f, regionRight = 1.0f,
        regionBottom = 1.0f;
  float invRegionWidth = 1.0f, invRegionHeight = 1.0f;
};
