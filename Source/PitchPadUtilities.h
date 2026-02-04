#pragma once

#include "MappingTypes.h"

// Helper types and functions for touchpad pitch-pad layout. Both the runtime
// (InputProcessor) and UI (VisualizerComponent) should use these so that what
// you see matches what you hear.

struct PitchPadBand {
  float xStart = 0.0f; // Inclusive
  float xEnd = 0.0f;   // Exclusive
  int step = 0;        // Discrete pitch step (semitone or scale step offset)
  bool isRest = false; // true = resting band, false = transition band
};

struct PitchPadLayout {
  std::vector<PitchPadBand> bands;
};

struct PitchSample {
  int step = 0;
  bool inRestingBand = false;
};

// Build a 1D layout of resting and transition bands that covers [0,1] on X.
// - Resting bands: one per integer step in [minStep, maxStep].
// - Transition bands: between adjacent steps when there is remaining width.
PitchPadLayout buildPitchPadLayout(const PitchPadConfig &config);

// Map a normalized X in [0,1] into a discrete step using the given layout.
PitchSample mapXToStep(const PitchPadLayout &layout, float x);
