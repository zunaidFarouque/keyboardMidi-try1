#include "../KeyboardLayoutUtils.h"
#include "../ScaleLibrary.h"
#include "../Zone.h"
#include "../ZoneDefinition.h"
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

// Effective root passed to rebuildCache is used for getNotesForKey
TEST(ZoneEffectiveRoot, GetNotesForKeyUsesPassedRoot) {
  ScaleLibrary scaleLib;
  std::vector<int> majorIntervals = scaleLib.getIntervals("Major");
  ASSERT_FALSE(majorIntervals.empty());

  auto zone = std::make_shared<Zone>();
  zone->layoutStrategy = Zone::LayoutStrategy::Linear;
  zone->inputKeyCodes = {kKeyQ};
  zone->scaleName = "Major";
  zone->degreeOffset = 0;
  zone->chordType = ChordUtilities::ChordType::None;
  zone->rebuildCache(majorIntervals, 48); // effective root 48 (G3)
  ASSERT_GT(zone->keyToChordCache.count(kKeyQ), 0u);

  auto notesOpt = zone->getNotesForKey(kKeyQ, 0, 0);
  ASSERT_TRUE(notesOpt.has_value() && !notesOpt->empty());
  EXPECT_EQ((*notesOpt)[0].pitch, 48)
      << "getNotesForKey should use effective root 48 for degree 0";
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

// --- Identity: name, zoneColor, layerID, targetAliasHash, midiChannel ---
TEST(ZoneIdentity, NameSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->name = "My Zone";
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->name, "My Zone");
}

TEST(ZoneIdentity, LayerIdSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->layerID = 3;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->layerID, 3);
}

TEST(ZoneIdentity, MidiChannelSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->midiChannel = 8;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->midiChannel, 8);
}

// --- Tuning: rootNote, scaleName, chromaticOffset, degreeOffset ---
TEST(ZoneTuning, RootNoteSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->rootNote = 48;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->rootNote, 48);
}

TEST(ZoneTuning, ScaleNameSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->scaleName = "Minor";
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->scaleName, "Minor");
}

TEST(ZoneTuning, ChromaticDegreeOffsetSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->chromaticOffset = 2;
  zone->degreeOffset = -1;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->chromaticOffset, 2);
  EXPECT_EQ(loaded->degreeOffset, -1);
}

// --- Velocity ---
TEST(ZoneVelocity, BaseVelocityAndRandomSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->baseVelocity = 90;
  zone->velocityRandom = 10;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->baseVelocity, 90);
  EXPECT_EQ(loaded->velocityRandom, 10);
}

// --- Release behavior ---
TEST(ZoneReleaseBehavior, SerializationRoundTripSustain) {
  auto zone = std::make_shared<Zone>();
  zone->releaseBehavior = Zone::ReleaseBehavior::Sustain;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->releaseBehavior, Zone::ReleaseBehavior::Sustain);
}

TEST(ZoneReleaseBehavior, DefaultIsNormal) {
  auto zone = std::make_shared<Zone>();
  EXPECT_EQ(zone->releaseBehavior, Zone::ReleaseBehavior::Normal);
}

// --- Layout strategy ---
TEST(ZoneLayoutStrategy, LinearAndPianoSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->layoutStrategy = Zone::LayoutStrategy::Linear;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->layoutStrategy, Zone::LayoutStrategy::Linear);

  zone->layoutStrategy = Zone::LayoutStrategy::Piano;
  vt = zone->toValueTree();
  loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->layoutStrategy, Zone::LayoutStrategy::Piano);
}

// --- Chord type ---
TEST(ZoneChordType, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->chordType = ChordUtilities::ChordType::Seventh;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->chordType, ChordUtilities::ChordType::Seventh);
}

// --- Instrument and voicing ---
TEST(ZoneInstrument, PianoVoicingAndGuitarSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->instrumentMode = Zone::InstrumentMode::Guitar;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->instrumentMode, Zone::InstrumentMode::Guitar);

  zone->pianoVoicingStyle = Zone::PianoVoicingStyle::Open;
  vt = zone->toValueTree();
  loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->pianoVoicingStyle, Zone::PianoVoicingStyle::Open);

  zone->guitarPlayerPosition = Zone::GuitarPlayerPosition::Rhythm;
  zone->guitarFretAnchor = 7;
  vt = zone->toValueTree();
  loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->guitarPlayerPosition, Zone::GuitarPlayerPosition::Rhythm);
  EXPECT_EQ(loaded->guitarFretAnchor, 7);
}

// --- Strum pattern and ghost notes ---
TEST(ZoneStrum, PatternAndGhostNotesSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->strumPattern = Zone::StrumPattern::Up;
  zone->strumGhostNotes = true;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->strumPattern, Zone::StrumPattern::Up);
  EXPECT_TRUE(loaded->strumGhostNotes);
}

// --- Add bass ---
TEST(ZoneAddBass, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->addBassNote = true;
  zone->bassOctaveOffset = -2;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->addBassNote);
  EXPECT_EQ(loaded->bassOctaveOffset, -2);
}

// --- Display and global ---
TEST(ZoneDisplay, ShowRomanNumeralsSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->showRomanNumerals = true;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->showRomanNumerals);
}

TEST(ZoneGlobal, UseGlobalScaleAndRootSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->useGlobalScale = true;
  zone->useGlobalRoot = true;
  zone->globalRootOctaveOffset = 1;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_TRUE(loaded->useGlobalScale);
  EXPECT_TRUE(loaded->useGlobalRoot);
  EXPECT_EQ(loaded->globalRootOctaveOffset, 1);
}

// --- Ghost harmony ---
TEST(ZoneGhostHarmony, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->strictGhostHarmony = false;
  zone->ghostVelocityScale = 0.5f;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_FALSE(loaded->strictGhostHarmony);
  EXPECT_FLOAT_EQ(loaded->ghostVelocityScale, 0.5f);
}

// --- Polyphony and glide ---
TEST(ZonePolyphony, ModeSerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Legato;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->polyphonyMode, PolyphonyMode::Legato);
}

TEST(ZoneGlide, SerializationRoundTrip) {
  auto zone = std::make_shared<Zone>();
  zone->glideTimeMs = 100;
  zone->isAdaptiveGlide = true;
  zone->maxGlideTimeMs = 300;
  juce::ValueTree vt = zone->toValueTree();
  auto loaded = Zone::fromValueTree(vt);
  ASSERT_NE(loaded, nullptr);
  EXPECT_EQ(loaded->glideTimeMs, 100);
  EXPECT_TRUE(loaded->isAdaptiveGlide);
  EXPECT_EQ(loaded->maxGlideTimeMs, 300);
}

// --- ZoneDefinition schema visibility ---
static bool schemaHasPropertyKey(const ZoneSchema &schema,
                                 const juce::String &key) {
  for (const auto &c : schema)
    if (c.propertyKey == key)
      return true;
  return false;
}

TEST(ZoneDefinitionSchema, PolyChordGuitarShowsStrumControls) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Poly;
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->instrumentMode = Zone::InstrumentMode::Guitar;
  zone->playMode = Zone::PlayMode::Strum;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "strumSpeedMs"));
  EXPECT_TRUE(schemaHasPropertyKey(schema, "strumPattern"));
  EXPECT_TRUE(schemaHasPropertyKey(schema, "strumGhostNotes"));
}

TEST(ZoneDefinitionSchema, GuitarRhythmShowsFretAnchor) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Poly;
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->instrumentMode = Zone::InstrumentMode::Guitar;
  zone->guitarPlayerPosition = Zone::GuitarPlayerPosition::Rhythm;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "guitarFretAnchor"));
}

TEST(ZoneDefinitionSchema, PolyChordPianoShowsVoicingAndMagnet) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Poly;
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->instrumentMode = Zone::InstrumentMode::Piano;
  zone->pianoVoicingStyle = Zone::PianoVoicingStyle::Close;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "pianoVoicingStyle"));
  EXPECT_TRUE(schemaHasPropertyKey(schema, "voicingMagnetSemitones"));
}

TEST(ZoneDefinitionSchema, LegatoShowsGlideControls) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Legato;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "glideTimeMs"));
  EXPECT_TRUE(schemaHasPropertyKey(schema, "isAdaptiveGlide"));
}

TEST(ZoneDefinitionSchema, LegatoAdaptiveShowsMaxGlideTime) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Legato;
  zone->isAdaptiveGlide = true;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "maxGlideTimeMs"));
}

TEST(ZoneDefinitionSchema, ReleaseNormalChordShowsDelayRelease) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Poly;
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->releaseBehavior = Zone::ReleaseBehavior::Normal;
  zone->delayReleaseOn = true;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "overrideTimer"));
}

TEST(ZoneDefinitionSchema, GlobalRootShowsOctaveOffset) {
  auto zone = std::make_shared<Zone>();
  zone->useGlobalRoot = true;
  ZoneSchema schema = ZoneDefinition::getSchema(zone.get());
  EXPECT_TRUE(schemaHasPropertyKey(schema, "globalRootOctaveOffset"));
}

TEST(ZoneDefinitionSchema, SchemaSignatureChangesWhenVisibilityChanges) {
  auto zone = std::make_shared<Zone>();
  zone->polyphonyMode = PolyphonyMode::Poly;
  zone->chordType = ChordUtilities::ChordType::None;
  juce::String sigNoChord = ZoneDefinition::getSchemaSignature(zone.get());
  zone->chordType = ChordUtilities::ChordType::Triad;
  juce::String sigChord = ZoneDefinition::getSchemaSignature(zone.get());
  EXPECT_NE(sigNoChord, sigChord)
      << "Schema signature should change when chord on (more controls)";
}
