#include "PitchPadUtilities.h"

PitchPadLayout buildPitchPadLayout(const PitchPadConfig &config) {
  PitchPadLayout layout;

  const int minStep = config.minStep;
  const int maxStep = config.maxStep;
  const int stepCount = (maxStep >= minStep) ? (maxStep - minStep + 1) : 0;

  if (stepCount <= 0) {
    return layout;
  }

  const float restPct = juce::jlimit(0.0f, 80.0f, config.restingSpacePercent);
  const float restWidthTotal =
      (restPct / 100.0f) * static_cast<float>(stepCount);
  const float remaining = juce::jmax(0.0f, 1.0f - restWidthTotal);
  const int transitionCount = stepCount > 1 ? (stepCount - 1) : 0;
  const float transitionWidth =
      transitionCount > 0 ? remaining / static_cast<float>(transitionCount)
                          : 0.0f;

  const float restWidth = (restPct > 0.0f && stepCount > 0)
                              ? (restWidthTotal / static_cast<float>(stepCount))
                              : 0.0f;

  float x = 0.0f;

  for (int i = 0; i < stepCount; ++i) {
    const int step = minStep + i;

    // Resting band for this step
    PitchPadBand restBand;
    restBand.xStart = x;
    restBand.xEnd = x + restWidth;
    restBand.step = step;
    restBand.isRest = true;
    layout.bands.push_back(restBand);
    x = restBand.xEnd;

    // Transition band to next step (except after last)
    if (i < stepCount - 1 && transitionWidth > 0.0f) {
      PitchPadBand transBand;
      transBand.xStart = x;
      transBand.xEnd = x + transitionWidth;
      transBand.step = step; // Lower step; sample helper can choose semantics.
      transBand.isRest = false;
      layout.bands.push_back(transBand);
      x = transBand.xEnd;
    }
  }

  // Ensure final band ends exactly at 1.0 to avoid gaps from float math.
  if (!layout.bands.empty()) {
    layout.bands.back().xEnd = 1.0f;
  }

  return layout;
}

PitchSample mapXToStep(const PitchPadLayout &layout, float x) {
  PitchSample sample;

  if (layout.bands.empty()) {
    return sample;
  }

  x = juce::jlimit(0.0f, 1.0f, x);

  for (const auto &band : layout.bands) {
    if (x >= band.xStart && x < band.xEnd) {
      sample.step = band.step;
      sample.inRestingBand = band.isRest;
      return sample;
    }
  }

  // Fallback: due to float rounding, clamp to last band.
  const auto &last = layout.bands.back();
  sample.step = last.step;
  sample.inRestingBand = last.isRest;
  return sample;
}
