#pragma once

#include "MappingTypes.h"

// Helper types and functions for touchpad pitch-pad layout. Both the runtime
// (InputProcessor) and UI (VisualizerComponent) should use these so that what
// you see matches what you hear.
// PitchPadBand and PitchPadLayout are defined in MappingTypes.h.

struct PitchSample {
  // Effective step value for this X position. In a resting band this will be
  // an integer (exact step). In a transition band this will be a fractional
  // value between two neighboring steps to support smooth glides.
  float step = 0.0f;
  bool inRestingBand = false;
  // Normalized position within the current band [0, 1].
  float localT = 0.0f;
};

// Build a 1D layout of resting and transition bands that covers [0,1] on X.
// - Resting bands: one per pitch step, centered within its band.
// - Transition bands: between adjacent steps when there is remaining width.
PitchPadLayout buildPitchPadLayout(const PitchPadConfig &config);

// Map a normalized X in [0,1] into a (possibly fractional) step using the
// given layout. The returned 'step' is measured in step units (semitones for
// PitchBend, scale steps for SmartScaleBend).
PitchSample mapXToStep(const PitchPadLayout &layout, float x);
