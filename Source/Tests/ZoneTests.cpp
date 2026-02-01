#include "../KeyboardLayoutUtils.h"
#include "../ScaleLibrary.h"
#include "../Zone.h"
#include <gtest/gtest.h>

// Keys that exist in KeyboardLayoutUtils: Q=0x51, A=0x41, W=0x57 (row 1 and 2)
static const int kKeyQ = 0x51;
static const int kKeyA = 0x41;
static const int kKeyW = 0x57;

// Zone with Grid layout: grid interval affects degree = deltaCol + (deltaRow *
// gridInterval)
TEST(ZoneGridLayout, GridIntervalAffectsCache) {
  ScaleLibrary scaleLib;
  std::vector<int> majorIntervals = scaleLib.getIntervals("Major");
  ASSERT_FALSE(majorIntervals.empty());

  auto zone = std::make_shared<Zone>();
  zone->layoutStrategy = Zone::LayoutStrategy::Grid;
  zone->inputKeyCodes = {kKeyQ, kKeyA};
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->degreeOffset = 0;
  zone->chordType = ChordUtilities::ChordType::None;

  zone->gridInterval = 5;
  zone->rebuildCache(majorIntervals, 60);
  ASSERT_TRUE(zone->keyToChordCache.count(kKeyA) > 0);
  std::vector<int> pitchesWith5;
  for (const auto &cn : zone->keyToChordCache.at(kKeyA))
    pitchesWith5.push_back(60 + cn.pitch);

  zone->gridInterval = 7;
  zone->rebuildCache(majorIntervals, 60);
  ASSERT_TRUE(zone->keyToChordCache.count(kKeyA) > 0);
  std::vector<int> pitchesWith7;
  for (const auto &cn : zone->keyToChordCache.at(kKeyA))
    pitchesWith7.push_back(60 + cn.pitch);

  EXPECT_NE(pitchesWith5, pitchesWith7)
      << "Grid interval 5 vs 7 should produce different cached notes for key A";
}

// Zone with Grid layout: keys present in KeyboardLayoutUtils get cache entries
TEST(ZoneGridLayout, KeysInLayoutGetCached) {
  ScaleLibrary scaleLib;
  std::vector<int> majorIntervals = scaleLib.getIntervals("Major");
  ASSERT_FALSE(majorIntervals.empty());

  const auto &layout = KeyboardLayoutUtils::getLayout();
  ASSERT_GT(layout.count(kKeyQ), 0u);
  ASSERT_GT(layout.count(kKeyA), 0u);
  ASSERT_GT(layout.count(kKeyW), 0u);

  auto zone = std::make_shared<Zone>();
  zone->layoutStrategy = Zone::LayoutStrategy::Grid;
  zone->inputKeyCodes = {kKeyQ, kKeyA, kKeyW};
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->degreeOffset = 0;
  zone->chordType = ChordUtilities::ChordType::None;
  zone->gridInterval = 5;

  zone->rebuildCache(majorIntervals, 60);

  EXPECT_EQ(zone->keyToChordCache.size(), 3u)
      << "All three keys (Q, A, W) should be in layout and get cache entries";
  EXPECT_GT(zone->keyToChordCache.count(kKeyQ), 0u);
  EXPECT_GT(zone->keyToChordCache.count(kKeyA), 0u);
  EXPECT_GT(zone->keyToChordCache.count(kKeyW), 0u);

  for (const auto &kv : zone->keyToChordCache) {
    EXPECT_FALSE(kv.second.empty())
        << "Each cached key should have at least one note";
  }
}

// --- Ignore global transpose ---

TEST(ZoneIgnoreGlobalTranspose, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->ignoreGlobalTranspose = true;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->ignoreGlobalTranspose);

  zone->ignoreGlobalTranspose = false;
  vt = zone->toValueTree();
  loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->ignoreGlobalTranspose);
}

TEST(ZoneIgnoreGlobalTranspose, GetNotesForKeyRespectsFlag) {
  ScaleLibrary scaleLib;
  std::vector<int> majorIntervals = scaleLib.getIntervals("Major");
  ASSERT_FALSE(majorIntervals.empty());

  auto zone = std::make_shared<Zone>();
  zone->layoutStrategy = Zone::LayoutStrategy::Linear;
  zone->inputKeyCodes = {kKeyQ};
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->degreeOffset = 0;
  zone->chordType = ChordUtilities::ChordType::None;
  zone->chromaticOffset = 0;
  zone->rebuildCache(majorIntervals, 60);
  ASSERT_GT(zone->keyToChordCache.count(kKeyQ), 0u);

  zone->ignoreGlobalTranspose = true;
  auto notesIgnore = zone->getNotesForKey(kKeyQ, 12, 0);
  ASSERT_TRUE(notesIgnore.has_value() && !notesIgnore->empty());
  // With ignore = true, global chromatic transpose (12) is not applied
  EXPECT_EQ((*notesIgnore)[0].pitch, 60);

  zone->ignoreGlobalTranspose = false;
  auto notesFollow = zone->getNotesForKey(kKeyQ, 12, 0);
  ASSERT_TRUE(notesFollow.has_value() && !notesFollow->empty());
  // With ignore = false, global chromatic transpose (12) is applied
  EXPECT_EQ((*notesFollow)[0].pitch, 72);
}

TEST(ZoneIgnoreGlobalTranspose, MigrationFromIsTransposeLocked) {
  juce::ValueTree vt("Zone");
  vt.setProperty("isTransposeLocked", true, nullptr);
  auto zone = Zone::fromValueTree(vt);
  ASSERT_NE(zone, nullptr);
  EXPECT_TRUE(zone->ignoreGlobalTranspose);

  vt.setProperty("isTransposeLocked", false, nullptr);
  zone = Zone::fromValueTree(vt);
  ASSERT_NE(zone, nullptr);
  EXPECT_FALSE(zone->ignoreGlobalTranspose);
}

// --- Ignore global sustain ---

TEST(ZoneIgnoreGlobalSustain, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->ignoreGlobalSustain = true;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->ignoreGlobalSustain);

  zone->ignoreGlobalSustain = false;
  vt = zone->toValueTree();
  loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->ignoreGlobalSustain);
}

TEST(ZoneIgnoreGlobalSustain, MigrationFromAllowSustain) {
  juce::ValueTree vt("Zone");
  vt.setProperty("allowSustain", true, nullptr);
  auto zone = Zone::fromValueTree(vt);
  ASSERT_NE(zone, nullptr);
  // Old: allowSustain true = follow global → new: ignoreGlobalSustain false
  EXPECT_FALSE(zone->ignoreGlobalSustain);

  vt.setProperty("allowSustain", false, nullptr);
  zone = Zone::fromValueTree(vt);
  ASSERT_NE(zone, nullptr);
  // Old: allowSustain false = ignore global → new: ignoreGlobalSustain true
  EXPECT_TRUE(zone->ignoreGlobalSustain);
}

// --- Play mode (Direct vs Strum Buffer) ---

TEST(ZonePlayMode, SerializationRoundTripDirect) {
  auto zone = std::make_shared<Zone>();
  zone->playMode = Zone::PlayMode::Direct;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->playMode, Zone::PlayMode::Direct);
}

TEST(ZonePlayMode, SerializationRoundTripStrum) {
  auto zone = std::make_shared<Zone>();
  zone->playMode = Zone::PlayMode::Strum;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->playMode, Zone::PlayMode::Strum);
}

TEST(ZonePlayMode, DefaultIsDirect) {
  auto zone = std::make_shared<Zone>();
  EXPECT_EQ(zone->playMode, Zone::PlayMode::Direct);
}

// --- Strum timing variation (with and without) ---

TEST(ZoneStrumTimingVariation, SerializationRoundTripVariationOff) {
  auto zone = std::make_shared<Zone>();
  zone->strumTimingVariationOn = false;
  zone->strumTimingVariationMs = 0;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->strumTimingVariationOn);
  EXPECT_EQ(loaded->strumTimingVariationMs, 0);
}

TEST(ZoneStrumTimingVariation, SerializationRoundTripVariationOn) {
  auto zone = std::make_shared<Zone>();
  zone->strumTimingVariationOn = true;
  zone->strumTimingVariationMs = 25;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->strumTimingVariationOn);
  EXPECT_EQ(loaded->strumTimingVariationMs, 25);
}

TEST(ZoneStrumTimingVariation, DefaultIsOffAndZero) {
  auto zone = std::make_shared<Zone>();
  EXPECT_FALSE(zone->strumTimingVariationOn);
  EXPECT_EQ(zone->strumTimingVariationMs, 0);
}

TEST(ZoneStrumSpeed, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->strumSpeedMs = 80;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->strumSpeedMs, 80);
}

// --- Delay release (Normal release only: checkbox + slider) ---

TEST(ZoneDelayRelease, SerializationRoundTripOff) {
  auto zone = std::make_shared<Zone>();
  zone->delayReleaseOn = false;
  zone->releaseDurationMs = 500;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->delayReleaseOn);
  EXPECT_EQ(loaded->releaseDurationMs, 500);
}

TEST(ZoneDelayRelease, SerializationRoundTripOn) {
  auto zone = std::make_shared<Zone>();
  zone->delayReleaseOn = true;
  zone->releaseDurationMs = 1200;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->delayReleaseOn);
  EXPECT_EQ(loaded->releaseDurationMs, 1200);
}

TEST(ZoneDelayRelease, DefaultIsOff) {
  auto zone = std::make_shared<Zone>();
  EXPECT_FALSE(zone->delayReleaseOn);
}

// --- Override timer (cancel previous chord timer) ---

TEST(ZoneOverrideTimer, SerializationRoundTripOff) {
  auto zone = std::make_shared<Zone>();
  zone->overrideTimer = false;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->overrideTimer);
}

TEST(ZoneOverrideTimer, SerializationRoundTripOn) {
  auto zone = std::make_shared<Zone>();
  zone->overrideTimer = true;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->overrideTimer);
}

TEST(ZoneOverrideTimer, DefaultIsOff) {
  auto zone = std::make_shared<Zone>();
  EXPECT_FALSE(zone->overrideTimer);
}

// --- Voicing magnet (Piano Close/Open: center offset -6..+6) ---
TEST(ZoneVoicingMagnet, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->voicingMagnetSemitones = 3;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->voicingMagnetSemitones, 3);
}

TEST(ZoneVoicingMagnet, DefaultIsZero) {
  auto zone = std::make_shared<Zone>();
  EXPECT_EQ(zone->voicingMagnetSemitones, 0);
}

TEST(ZoneVoicingMagnet, NegativeValueRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->voicingMagnetSemitones = -2;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->voicingMagnetSemitones, -2);
}
