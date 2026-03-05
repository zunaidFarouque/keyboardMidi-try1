#include "../ZonePresets.h"
#include "../ScaleLibrary.h"
#include "../ZoneManager.h"
#include <gtest/gtest.h>

namespace {

bool hasLayoutStrategy(const std::vector<std::shared_ptr<Zone>> &zones,
                       Zone::LayoutStrategy strategy) {
  for (const auto &z : zones) {
    if (z && z->layoutStrategy == strategy)
      return true;
  }
  return false;
}

} // namespace

TEST(ZonePresets, BottomRowChordsPresetBasics) {
  auto zones = ZonePresets::createPresetZones(ZonePresetId::BottomRowChords);
  ASSERT_EQ(zones.size(), 1u);
  auto z = zones.front();
  ASSERT_TRUE(z);
  EXPECT_EQ(z->layoutStrategy, Zone::LayoutStrategy::Linear);
  EXPECT_EQ(z->chordType, ChordUtilities::ChordType::Triad);
  EXPECT_FALSE(z->inputKeyCodes.empty());
}

TEST(ZonePresets, BottomTwoRowsPianoPresetBasics) {
  auto zones = ZonePresets::createPresetZones(ZonePresetId::BottomTwoRowsPiano);
  ASSERT_EQ(zones.size(), 1u);
  auto z = zones.front();
  ASSERT_TRUE(z);
  EXPECT_EQ(z->layoutStrategy, Zone::LayoutStrategy::Piano);
  EXPECT_EQ(z->chordType, ChordUtilities::ChordType::None);
  EXPECT_FALSE(z->inputKeyCodes.empty());
}

TEST(ZonePresets, QwerNumbersPianoPresetBasics) {
  auto zones = ZonePresets::createPresetZones(ZonePresetId::QwerNumbersPiano);
  ASSERT_EQ(zones.size(), 1u);
  auto z = zones.front();
  ASSERT_TRUE(z);
  EXPECT_EQ(z->layoutStrategy, Zone::LayoutStrategy::Piano);
  EXPECT_EQ(z->chordType, ChordUtilities::ChordType::None);
  EXPECT_FALSE(z->inputKeyCodes.empty());
}

TEST(ZonePresets, JankoFourRowsPresetBasics) {
  auto zones = ZonePresets::createPresetZones(ZonePresetId::JankoFourRows);
  ASSERT_EQ(zones.size(), 1u);
  auto z = zones.front();
  ASSERT_TRUE(z);
  EXPECT_EQ(z->layoutStrategy, Zone::LayoutStrategy::Janko);
  EXPECT_EQ(z->chordType, ChordUtilities::ChordType::None);
  EXPECT_FALSE(z->inputKeyCodes.empty());
}

TEST(ZonePresets, IsomorphicFourRowsPresetBasics) {
  auto zones =
      ZonePresets::createPresetZones(ZonePresetId::IsomorphicFourRows);
  ASSERT_EQ(zones.size(), 1u);
  auto z = zones.front();
  ASSERT_TRUE(z);
  EXPECT_EQ(z->layoutStrategy, Zone::LayoutStrategy::Grid);
  EXPECT_EQ(z->gridInterval, 5);
  EXPECT_EQ(z->chordType, ChordUtilities::ChordType::None);
  EXPECT_FALSE(z->inputKeyCodes.empty());
}

