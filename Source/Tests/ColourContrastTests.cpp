#include "../ColourContrast.h"
#include <gtest/gtest.h>

// Visualizer text contrast: text color must be based on key fill (not
// backdrop). getTextColorForKeyFill returns black for bright fills, white for
// dark fills.

TEST(ColourContrast, DarkKeyFillGivesWhiteText) {
  juce::Colour darkFill(0xff333333); // Key body fill used in visualizer
  juce::Colour text = ColourContrast::getTextColorForKeyFill(darkFill);
  EXPECT_EQ(text, juce::Colours::white);
}

TEST(ColourContrast, BrightKeyFillGivesBlackText) {
  juce::Colour yellowFill = juce::Colours::yellow; // Pressed key
  juce::Colour text = ColourContrast::getTextColorForKeyFill(yellowFill);
  EXPECT_EQ(text, juce::Colours::black);
}

TEST(ColourContrast, CyanLatchedKeyGivesBlackText) {
  juce::Colour cyanFill = juce::Colours::cyan; // Latched key
  juce::Colour text = ColourContrast::getTextColorForKeyFill(cyanFill);
  EXPECT_EQ(text, juce::Colours::black);
}

TEST(ColourContrast, BoundaryNearThreshold) {
  // Threshold is 0.7f (brightness > 0.7 -> black). Black = 0, white = 1.
  juce::Colour nearDark(0xff666666);   // below threshold -> white text
  juce::Colour nearBright(0xffcccccc); // above threshold -> black text
  EXPECT_EQ(ColourContrast::getTextColorForKeyFill(nearDark),
            juce::Colours::white);
  EXPECT_EQ(ColourContrast::getTextColorForKeyFill(nearBright),
            juce::Colours::black);
}
