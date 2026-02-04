#include "PitchPadUtilities.h"
#include <cmath>

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
    restBand.invSpan =
        (restWidth > 0.0f) ? (1.0f / restWidth) : 0.0f;
    restBand.step = step;
    restBand.isRest = true;
    layout.bands.push_back(restBand);
    x = restBand.xEnd;

    // Transition band to next step (except after last)
    if (i < stepCount - 1 && transitionWidth > 0.0f) {
      PitchPadBand transBand;
      transBand.xStart = x;
      transBand.xEnd = x + transitionWidth;
      transBand.invSpan =
          (transitionWidth > 0.0f) ? (1.0f / transitionWidth) : 0.0f;
      transBand.step = step; // Transition from 'step' to 'step + 1'
      transBand.isRest = false;
      layout.bands.push_back(transBand);
      x = transBand.xEnd;
    }
  }

  // Ensure final band covers the end of the interval.
  if (!layout.bands.empty()) {
    auto &back = layout.bands.back();
    back.xEnd = 1.0f;
    float span = back.xEnd - back.xStart;
    back.invSpan = (span > 0.0f) ? (1.0f / span) : 0.0f;
  }

  return layout;
}

PitchSample mapXToStep(const PitchPadLayout &layout, float x) {
  PitchSample sample;

  if (layout.bands.empty())
    return sample;

  x = juce::jlimit(0.0f, 1.0f, x);

  for (const auto &band : layout.bands) {
    if (x >= band.xStart && x < band.xEnd) {
      float u = (x - band.xStart) * band.invSpan;
      if (band.isRest) {
        sample.step = static_cast<float>(band.step);
        sample.inRestingBand = true;
        sample.localT = 0.0f;
      } else {
        // Smoothly interpolate between this band's base step and the next step.
        float base = static_cast<float>(band.step);
        float frac = juce::jlimit(0.0f, 1.0f, u);
        sample.step = base + frac; // step .. step + 1
        sample.inRestingBand = false;
        sample.localT = frac;
      }
      return sample;
    }
  }

  // Fallback: clamp to last band.
  const auto &last = layout.bands.back();
  sample.step = static_cast<float>(last.step);
  sample.inRestingBand = last.isRest;
  return sample;
}
