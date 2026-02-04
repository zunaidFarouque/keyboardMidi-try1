#include "../MappingTypes.h"
#include "../PitchPadUtilities.h"

#include <gtest/gtest.h>

TEST(PitchPadUtilitiesTest, LayoutCoversUnitIntervalWithRestAndTransitions) {
  PitchPadConfig cfg;
  cfg.minStep = -1;
  cfg.maxStep = 1;
  cfg.restingSpacePercent = 10.0f; // 10% per step

  PitchPadLayout layout = buildPitchPadLayout(cfg);

  // We expect alternating rest/transition bands with contiguous coverage.
  ASSERT_FALSE(layout.bands.empty());

  EXPECT_LE(layout.bands.front().xStart, 0.0f + 1e-5f);
  EXPECT_NEAR(layout.bands.back().xEnd, 1.0f, 1e-5f);

  // Ensure there are resting bands for each step.
  int restCount = 0;
  for (const auto &b : layout.bands) {
    if (b.isRest)
      ++restCount;
  }
  EXPECT_EQ(restCount, cfg.maxStep - cfg.minStep + 1);
}

TEST(PitchPadUtilitiesTest, MapXToStepRespectsRestingBands) {
  PitchPadConfig cfg;
  cfg.minStep = -1;
  cfg.maxStep = 1;
  cfg.restingSpacePercent = 30.0f; // Large rest bands for clearer sampling

  PitchPadLayout layout = buildPitchPadLayout(cfg);

  // Sample a few representative X positions and ensure we get stable steps.
  PitchSample left = mapXToStep(layout, 0.05f);
  PitchSample center = mapXToStep(layout, 0.5f);
  PitchSample right = mapXToStep(layout, 0.95f);

  EXPECT_TRUE(left.inRestingBand);
  EXPECT_TRUE(center.inRestingBand);
  EXPECT_TRUE(right.inRestingBand);

  // Monotonicity: left step <= center step <= right step
  EXPECT_LE(left.step, center.step);
  EXPECT_LE(center.step, right.step);
}
