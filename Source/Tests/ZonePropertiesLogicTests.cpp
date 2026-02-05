#include "../ChordUtilities.h"
#include "../Zone.h"
#include "../ZonePropertiesLogic.h"
#include <gtest/gtest.h>

TEST(ZonePropertiesLogicTest, SetRootNote) {
  Zone z;
  z.rootNote = 60;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "rootNote", 64));
  EXPECT_EQ(z.rootNote, 64);
}

TEST(ZonePropertiesLogicTest, SetBaseVelocity) {
  Zone z;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "baseVelocity", 100));
  EXPECT_EQ(z.baseVelocity, 100);
}

TEST(ZonePropertiesLogicTest, SetGlideTimeMs) {
  Zone z;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "glideTimeMs", 80));
  EXPECT_EQ(z.glideTimeMs, 80);
}

TEST(ZonePropertiesLogicTest, SetUseGlobalRoot) {
  Zone z;
  z.useGlobalRoot = false;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "useGlobalRoot", true));
  EXPECT_TRUE(z.useGlobalRoot);
}

TEST(ZonePropertiesLogicTest, SetPlayMode_Direct) {
  Zone z;
  z.playMode = Zone::PlayMode::Strum;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "playMode", 1));
  EXPECT_EQ(z.playMode, Zone::PlayMode::Direct);
}

TEST(ZonePropertiesLogicTest, SetPlayMode_Strum) {
  Zone z;
  z.playMode = Zone::PlayMode::Direct;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "playMode", 2));
  EXPECT_EQ(z.playMode, Zone::PlayMode::Strum);
}

TEST(ZonePropertiesLogicTest, SetChordType_Triad) {
  Zone z;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "chordType", 2));
  EXPECT_EQ(z.chordType, ChordUtilities::ChordType::Triad);
}

TEST(ZonePropertiesLogicTest, SetPolyphonyMode_Legato) {
  Zone z;
  z.polyphonyMode = PolyphonyMode::Poly;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "polyphonyMode", 3));
  EXPECT_EQ(z.polyphonyMode, PolyphonyMode::Legato);
}

TEST(ZonePropertiesLogicTest, SetInstrumentMode_Guitar) {
  Zone z;
  z.instrumentMode = Zone::InstrumentMode::Piano;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "instrumentMode", 2));
  EXPECT_EQ(z.instrumentMode, Zone::InstrumentMode::Guitar);
}

TEST(ZonePropertiesLogicTest, SetLayoutStrategy_Piano) {
  Zone z;
  z.layoutStrategy = Zone::LayoutStrategy::Linear;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "layoutStrategy", 3));
  EXPECT_EQ(z.layoutStrategy, Zone::LayoutStrategy::Piano);
}

TEST(ZonePropertiesLogicTest, SetDelayReleaseOn) {
  Zone z;
  z.delayReleaseOn = false;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "delayReleaseOn", true));
  EXPECT_TRUE(z.delayReleaseOn);
}

TEST(ZonePropertiesLogicTest, SetGhostVelocityScale) {
  Zone z;
  EXPECT_TRUE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "ghostVelocityScale", 0.5));
  EXPECT_FLOAT_EQ(z.ghostVelocityScale, 0.5f);
}

TEST(ZonePropertiesLogicTest, UnknownKey_ReturnsFalse) {
  Zone z;
  EXPECT_FALSE(ZonePropertiesLogic::setZonePropertyFromKey(&z, "unknownKey", 1));
}

TEST(ZonePropertiesLogicTest, NullZone_ReturnsFalse) {
  EXPECT_FALSE(ZonePropertiesLogic::setZonePropertyFromKey(nullptr, "rootNote", 60));
}
