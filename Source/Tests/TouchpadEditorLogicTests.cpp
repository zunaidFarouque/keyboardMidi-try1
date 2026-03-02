#include "../TouchpadEditorLogic.h"
#include <gtest/gtest.h>

TEST(TouchpadEditorLogic, ApplyLayoutBasicFields) {
  TouchpadLayoutConfig cfg;
  cfg.numFaders = 5;
  cfg.ccStart = 50;

  EXPECT_TRUE(TouchpadEditorLogic::applyConfig(&cfg, nullptr, "numFaders", 8));
  EXPECT_EQ(cfg.numFaders, 8);

  EXPECT_TRUE(TouchpadEditorLogic::applyConfig(&cfg, nullptr, "ccStart", 64));
  EXPECT_EQ(cfg.ccStart, 64);
}

TEST(TouchpadEditorLogic, ApplyMappingHeaderFields) {
  TouchpadMappingConfig cfg;
  cfg.layerId = 0;
  cfg.layoutGroupId = 0;
  cfg.midiChannel = 1;

  EXPECT_TRUE(
      TouchpadEditorLogic::applyConfig(nullptr, &cfg, "layerId", juce::var(3)));
  EXPECT_EQ(cfg.layerId, 2); // combo 3 -> id 2

  EXPECT_TRUE(TouchpadEditorLogic::applyConfig(
      nullptr, &cfg, "layoutGroupId", juce::var(5)));
  EXPECT_EQ(cfg.layoutGroupId, 5);

  EXPECT_TRUE(TouchpadEditorLogic::applyConfig(
      nullptr, &cfg, "midiChannel", juce::var(4)));
  EXPECT_EQ(cfg.midiChannel, 4);
}

TEST(TouchpadEditorLogic, ApplyMappingValueReleaseBehavior) {
  juce::ValueTree mapping("Mapping");

  EXPECT_TRUE(TouchpadEditorLogic::applyMappingValueProperty(
      mapping, "releaseBehavior", juce::var(2)));
  EXPECT_EQ(mapping.getProperty("releaseBehavior").toString(),
            juce::String("Sustain until retrigger"));
}

