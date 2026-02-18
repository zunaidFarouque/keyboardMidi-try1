#pragma once
#include <JuceHeader.h>
#include <cstdint>
#include <string>
#include <vector>

// Touchpad tab type selector.
// - Mixer      = vertical CC faders
// - DrumPad    = classic finger-drumming / pad grid
// - ChordPad   = chord trigger grid
// Harmonic-style behaviour is implemented as a DrumPad layout mode rather
// than a separate type.
enum class TouchpadType {
  Mixer = 0,
  DrumPad = 1,
  ChordPad = 2
};

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

// Per-layout mode for DrumPad-derived layouts (note / performance grids).
// This lets us keep legacy "Drum Pad / Launcher" sessions working while
// adding richer grid behaviours without changing the high-level TouchpadType
// used in presets.
enum class DrumPadLayoutMode {
  Classic = 0,      // Legacy drum pad: chromatic grid from midiNoteStart
  HarmonicGrid = 1  // Isomorphic harmonic grid (rowInterval + scale filter)
};

// Named layout group for touchpad layouts. Groups live in TouchpadMixerManager
// and layouts refer to them by ID.
struct TouchpadLayoutGroup {
  int id = 0;
  std::string name;
};

// Config for one touchpad mapping row in the Touchpad tab.
// This is independent from the global Mapping list; we keep a full ValueTree
// for the mapping so we can reuse the existing mapping engine (GridCompiler,
// InputProcessor, MappingDefinition, etc.) without re-implementing every
// property by hand.
struct TouchpadMappingConfig {
  // Shared header fields (same semantics as TouchpadMixerConfig)
  std::string name = "Touchpad Mapping";
  int layerId = 0;
  int layoutGroupId = 0; // 0 = none, >0 = TouchpadLayoutGroup::id
  int midiChannel = 1;   // Shared with Mapping ValueTree "channel"
  TouchpadLayoutRegion region; // Active region on touchpad (0-1, normalized)
  int zIndex = 0;
  bool regionLock = false;

  // Underlying mapping ValueTree (type "Mapping").
  // Must use the same schema/property IDs as the main mapping engine
  // (see MappingTypes / MappingDefinition / MappingInspector).
  juce::ValueTree mapping;
};

// Config for one touchpad strip (serialized in preset / session).
// type determines which controls apply:
// - Mixer       = vertical CC faders
// - DrumPad*    = finger drumming / note / performance grid family
struct TouchpadMixerConfig {
  TouchpadType type = TouchpadType::Mixer;
  std::string name = "Touchpad Mixer";
  int layerId = 0;
  // Optional layout group: 0 = none (follows layer only), >0 = group ID
  // Layout groups are used for conditional visibility / soloing.
  int layoutGroupId = 0;
  std::string layoutGroupName;
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
  // Z-index for stacking when regions overlap on same layer (higher = on top)
  int zIndex = 0;
  // Region lock: finger locked to this layout until release; shows ghost at edge when outside
  bool regionLock = false;

  // DrumPad-style grid fields (used when type is a grid layout:
  // DrumPad/HarmonicGrid/ChordPad).
  int drumPadRows = 2;
  int drumPadColumns = 4;
  int drumPadMidiNoteStart = 60;
  int drumPadBaseVelocity = 100;
  int drumPadVelocityRandom = 0;
  float drumPadDeadZoneLeft = 0.0f;
  float drumPadDeadZoneRight = 0.0f;
  float drumPadDeadZoneTop = 0.0f;
  float drumPadDeadZoneBottom = 0.0f;
  DrumPadLayoutMode drumPadLayoutMode = DrumPadLayoutMode::Classic;

  // Harmonic Grid specific (used when DrumPad layoutMode == HarmonicGrid)
  // Rows/columns come from drumPadRows/drumPadColumns.
  // Base note comes from drumPadMidiNoteStart.
  int harmonicRowInterval = 5;          // semitones between rows (e.g. 5 = P4)
  bool harmonicUseScaleFilter = false;  // future: constrain to global scale

  // Chord Pad specific (used when type == ChordPad)
  // Rows/columns come from drumPadRows/drumPadColumns.
  // Root for first pad comes from drumPadMidiNoteStart.
  int chordPadPreset = 0;              // 0=Diatonic I–VII, 1=Pop, 2=Extended
  bool chordPadLatchMode = true;       // true=Latch, false=Momentary

  // Legacy Drum+FX Split-specific fields (no longer used).
  int drumFxSplitSplitRow = 1;
  int fxCcStart = 20;
  int fxOutputMin = 0;
  int fxOutputMax = 127;
  bool fxToggleMode = true;
};

// Precomputed mode flags (avoids per-frame branching)
static constexpr uint8_t kMixerModeUseFinger1 = 1 << 0;  // Quick vs Precision
static constexpr uint8_t kMixerModeLock = 1 << 1;        // Lock vs Free
static constexpr uint8_t kMixerModeRelative = 1 << 2;   // Absolute vs Relative
static constexpr uint8_t kMixerModeMuteButtons = 1 << 3; // Mute buttons enabled
static constexpr uint8_t kMixerModeRegionLock = 1 << 4;  // Region lock enabled
static constexpr float kMuteButtonRegionTop = 0.85f;     // Bottom 15% = mute

// Compiled entry for runtime (no ValueTree in hot path).
struct TouchpadMixerEntry {
  int layerId = 0;
  int layoutGroupId = 0;
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
  bool regionLock = false;
};

// Compiled entry for DrumPad strip (O(1) runtime).
// Grid is stretched to fit within region; region defines active area.
// layoutMode selects between Classic and Harmonic behaviours.
struct TouchpadDrumPadEntry {
  int layerId = 0;
  int layoutGroupId = 0;
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
  bool regionLock = false;
  DrumPadLayoutMode layoutMode = DrumPadLayoutMode::Classic;

  // Harmonic mode parameters (only used when layoutMode == HarmonicGrid)
  int harmonicRowInterval = 5;
  bool harmonicUseScaleFilter = false;
};

// Compiled entry for Chord Pad layout.
// For Phase 1 we support a small set of factory chord presets; chords are
// generated at runtime from (preset, baseNote, pad index).
struct TouchpadChordPadEntry {
  int layerId = 0;
  int layoutGroupId = 0;
  int rows = 0;
  int columns = 0;
  int midiChannel = 1;
  int baseVelocity = 100;
  int velocityRandom = 0;
  int baseRootNote = 60; // root for pad 0; others derived from this
  int presetId = 0;
  bool latchMode = true;
  float regionLeft = 0.0f, regionTop = 0.0f, regionRight = 1.0f,
        regionBottom = 1.0f;
  float invRegionWidth = 1.0f, invRegionHeight = 1.0f;
  bool regionLock = false;
};

// Compiled entry for Drum+FX Split layout.
// Bottom rows = drums (note grid), top rows = FX pads (CC toggles/momentary).
struct TouchpadDrumFxSplitEntry {
  int layerId = 0;
  int layoutGroupId = 0;
  int rows = 0;
  int columns = 0;
  int splitRow = 1; // FX region starts at this row index (0..rows)

  // Drum section
  int drumMidiNoteStart = 36;
  int drumMidiChannel = 1;
  int drumBaseVelocity = 100;
  int drumVelocityRandom = 0;

  // FX section
  int fxMidiChannel = 1;
  int fxCcStart = 20;
  int fxCcCount = 8;
  int fxOutputMin = 0;
  int fxOutputMax = 127;
  bool fxToggleMode = true;

  // Region
  float regionLeft = 0.0f, regionTop = 0.0f, regionRight = 1.0f,
        regionBottom = 1.0f;
  float invRegionWidth = 1.0f, invRegionHeight = 1.0f;
  bool regionLock = false;
};
