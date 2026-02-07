#include "../ChordUtilities.h"
#include "../DeviceManager.h"
#include "../GridCompiler.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../ScaleUtilities.h"
#include "../SettingsManager.h"
#include "../TouchpadMixerManager.h"
#include "../Zone.h"
#include "../ZoneManager.h"
#include <algorithm>
#include <gtest/gtest.h>
#include <set>

// C Major scale intervals
static const std::vector<int> kCMajorIntervals = {0, 2, 4, 5, 7, 9, 11};
static const int kCenterC4 = 60;

// Helper: extract pitches from ChordNote vector
static std::vector<int>
pitchesOf(const std::vector<ChordUtilities::ChordNote> &notes) {
  std::vector<int> p;
  for (const auto &n : notes)
    p.push_back(n.pitch);
  return p;
}

// Helper: assert sorted and in MIDI range
static void expectValidSortedChord(const std::vector<int> &pitches,
                                   size_t minSize = 1) {
  ASSERT_GE(pitches.size(), minSize);
  for (int midi : pitches) {
    EXPECT_GE(midi, 0);
    EXPECT_LE(midi, 127);
  }
  for (size_t i = 1; i < pitches.size(); ++i)
    EXPECT_LE(pitches[i - 1], pitches[i]);
}

// --- Piano: Block (root position) ---
TEST(ChordUtilitiesPiano, BlockTriad_ReturnsRootPositionSorted) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Block, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  EXPECT_EQ(pitches.size(), 3u);
  // C major degree 0: C=60, E=64, G=67
  EXPECT_EQ(pitches[0], 60);
  EXPECT_EQ(pitches[1], 64);
  EXPECT_EQ(pitches[2], 67);
}

TEST(ChordUtilitiesPiano, BlockSeventh_ReturnsFourNotesSorted) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Seventh,
      ChordUtilities::PianoVoicingStyle::Block, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 4u);
  EXPECT_EQ(pitches.size(), 4u);
  EXPECT_EQ(pitches[0], 60);
  EXPECT_EQ(pitches[1], 64);
  EXPECT_EQ(pitches[2], 67);
  EXPECT_EQ(pitches[3], 71); // B
}

TEST(ChordUtilitiesPiano, BlockTriad_Degree1_DReturnsCorrectPitches) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 1, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Block, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  // D minor: D=62, F=65, A=69
  EXPECT_EQ(pitches[0], 62);
  EXPECT_EQ(pitches[1], 65);
  EXPECT_EQ(pitches[2], 69);
}

// --- Piano: Close (Smart Flow - Gravity Well for triads, Alternating Grip for
// 7th) ---
TEST(ChordUtilitiesPiano, CloseTriad_ReturnsThreeNotesClustered) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Close, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  EXPECT_EQ(pitches.size(), 3u);
  // Gravity Well keeps notes near center 60; should still be C E G (possibly
  // different octave)
  std::set<int> pitchClasses;
  for (int p : pitches)
    pitchClasses.insert(p % 12);
  EXPECT_EQ(pitchClasses.size(), 3u);
  EXPECT_TRUE(pitchClasses.count(0)); // C
  EXPECT_TRUE(pitchClasses.count(4)); // E
  EXPECT_TRUE(pitchClasses.count(7)); // G
}

TEST(ChordUtilitiesPiano, CloseSeventh_Degree0_Odd_UsesRootPosition) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Seventh,
      ChordUtilities::PianoVoicingStyle::Close, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 4u);
  EXPECT_EQ(pitches.size(), 4u);
  std::set<int> pc;
  for (int p : pitches)
    pc.insert(p % 12);
  EXPECT_TRUE(pc.count(0));
  EXPECT_TRUE(pc.count(4));
  EXPECT_TRUE(pc.count(7));
  EXPECT_TRUE(pc.count(11)); // B
}

TEST(ChordUtilitiesPiano, CloseSeventh_Degree1_Even_ReturnsFourNotes) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 1, ChordUtilities::ChordType::Seventh,
      ChordUtilities::PianoVoicingStyle::Close, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 4u);
  EXPECT_EQ(pitches.size(), 4u);
  // Dm7: pitch classes D=2, F=5, A=9, C=0 (or octave)
  std::set<int> pc;
  for (int p : pitches)
    pc.insert(p % 12);
  EXPECT_TRUE(pc.count(0));
  EXPECT_TRUE(pc.count(2));
  EXPECT_TRUE(pc.count(5));
  EXPECT_TRUE(pc.count(9));
}

// --- Piano: Open (Drop-2 + Smart Flow) ---
TEST(ChordUtilitiesPiano, OpenTriad_ReturnsSpreadVoicing) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Open, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  EXPECT_EQ(pitches.size(), 3u);
  // Drop-2 on triad drops middle; spread then gravity
  int span = pitches.back() - pitches.front();
  EXPECT_GE(span, 7); // at least a 5th span
}

TEST(ChordUtilitiesPiano, OpenSeventh_ReturnsFourNotesSpread) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Seventh,
      ChordUtilities::PianoVoicingStyle::Open, true);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 4u);
  EXPECT_EQ(pitches.size(), 4u);
  std::set<int> pc;
  for (int p : pitches)
    pc.insert(p % 12);
  EXPECT_EQ(pc.size(), 4u);
}

// --- Piano: None (single note) ---
TEST(ChordUtilitiesPiano, BlockNone_ReturnsSingleNote) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::None,
      ChordUtilities::PianoVoicingStyle::Block, true);
  EXPECT_EQ(notes.size(), 1u);
  EXPECT_EQ(notes[0].pitch, 60);
}

// --- Piano: Magnet (center offset for Close/Open voicing) ---
TEST(ChordUtilitiesPiano, CloseTriad_Magnet0_MatchesExplicitZero) {
  auto notesDefault = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Close, true);
  auto notesExplicit0 = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Close, true, 0);
  auto pit0 = pitchesOf(notesDefault);
  auto pit1 = pitchesOf(notesExplicit0);
  ASSERT_EQ(pit0.size(), pit1.size());
  for (size_t i = 0; i < pit0.size(); ++i)
    EXPECT_EQ(pit0[i], pit1[i]) << "magnet 0 must match current voicing";
}

TEST(ChordUtilitiesPiano, CloseTriad_MagnetPlus1_ValidChord) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Close, true, 1);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  std::set<int> pc;
  for (int p : pitches)
    pc.insert(p % 12);
  EXPECT_TRUE(pc.count(0));
  EXPECT_TRUE(pc.count(4));
  EXPECT_TRUE(pc.count(7));
}

TEST(ChordUtilitiesPiano, CloseTriad_MagnetClampedToRange) {
  auto notes = ChordUtilities::generateChordForPiano(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad,
      ChordUtilities::PianoVoicingStyle::Close, true, 10);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 3u);
  EXPECT_EQ(pitches.size(), 3u);
}

// --- Guitar: Campfire (frets 0-4) ---
// Not all chord tones may fit in a narrow fret window; we get as many as
// playable.
TEST(ChordUtilitiesGuitar, CampfireTriad_ReturnsNotesInFretRange) {
  auto notes = ChordUtilities::generateChordForGuitar(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad, 0, 4);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 1u);
  EXPECT_GE(pitches.size(), 1u);
  EXPECT_LE(pitches.size(), 6u);
  for (int p : pitches) {
    EXPECT_GE(p, 40);
    EXPECT_LE(p, 68);
  }
}

TEST(ChordUtilitiesGuitar, CampfireSeventh_ReturnsNotesInFretRange) {
  auto notes = ChordUtilities::generateChordForGuitar(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Seventh, 0, 4);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 1u);
  EXPECT_GE(pitches.size(), 1u);
  EXPECT_LE(pitches.size(), 6u);
}

// --- Guitar: Rhythm / Virtual Capo (e.g. frets 5-8) ---
TEST(ChordUtilitiesGuitar, RhythmTriad_ReturnsNotesInCapoRange) {
  int fretMin = 5, fretMax = 8;
  auto notes = ChordUtilities::generateChordForGuitar(
      kCenterC4, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad, fretMin,
      fretMax);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 1u);
  EXPECT_GE(pitches.size(), 1u);
  EXPECT_LE(pitches.size(), 6u);
  for (int p : pitches) {
    EXPECT_GE(p, 45);
    EXPECT_LE(p, 72);
  }
}

TEST(ChordUtilitiesGuitar, RhythmSeventh_Degree2_ReturnsValidVoicing) {
  auto notes = ChordUtilities::generateChordForGuitar(
      kCenterC4, kCMajorIntervals, 2, ChordUtilities::ChordType::Seventh, 5, 8);
  auto pitches = pitchesOf(notes);
  expectValidSortedChord(pitches, 1u);
  EXPECT_GE(pitches.size(), 1u);
  EXPECT_LE(pitches.size(), 6u);
}

// --- Guitar: Bass isolation (root on A string -> no low E) ---
TEST(ChordUtilitiesGuitar, CampfireTriadDegree0_RootOnA_NoLowE) {
  // Root note 48 (C3): root=48 on A string fret 3 in Campfire. Root on string 1
  // (A) -> mute string 0 (E). So lowest note should be >= 45 (A open).
  const int rootC3 = 48;
  auto notes = ChordUtilities::generateChordForGuitar(
      rootC3, kCMajorIntervals, 0, ChordUtilities::ChordType::Triad, 0, 4);
  auto pitches = pitchesOf(notes);
  ASSERT_GE(pitches.size(), 3u);
  EXPECT_GE(pitches[0], 45);
}

// --- Zone integration: Piano + Close compiles to chord pool ---
TEST(ChordUtilitiesZoneIntegration, PianoClose_Triad_CompilesToChordPool) {
  ScaleLibrary scaleLib;
  ZoneManager zoneMgr(scaleLib);
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  SettingsManager settingsMgr;
  TouchpadMixerManager touchpadMixerMgr;
  presetMgr.ensureStaticLayers();

  auto zone = std::make_shared<Zone>();
  zone->name = "Piano Close";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->instrumentMode = Zone::InstrumentMode::Piano;
  zone->pianoVoicingStyle = Zone::PianoVoicingStyle::Close;
  zoneMgr.addZone(zone);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, touchpadMixerMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[81];
  ASSERT_TRUE(slot.isActive);
  ASSERT_GE(slot.chordIndex, 0);
  ASSERT_LT(static_cast<size_t>(slot.chordIndex), context->chordPool.size());
  const auto &chord = context->chordPool[static_cast<size_t>(slot.chordIndex)];
  EXPECT_EQ(chord.size(), 3u);
}

// --- Zone integration: Piano + Open compiles ---
TEST(ChordUtilitiesZoneIntegration, PianoOpen_Seventh_CompilesToChordPool) {
  ScaleLibrary scaleLib;
  ZoneManager zoneMgr(scaleLib);
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  SettingsManager settingsMgr;
  TouchpadMixerManager touchpadMixerMgr;
  presetMgr.ensureStaticLayers();

  auto zone = std::make_shared<Zone>();
  zone->name = "Piano Open";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Seventh;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->instrumentMode = Zone::InstrumentMode::Piano;
  zone->pianoVoicingStyle = Zone::PianoVoicingStyle::Open;
  zoneMgr.addZone(zone);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, touchpadMixerMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[81];
  ASSERT_TRUE(slot.isActive);
  ASSERT_GE(slot.chordIndex, 0);
  const auto &chord = context->chordPool[static_cast<size_t>(slot.chordIndex)];
  EXPECT_EQ(chord.size(), 4u);
}

// --- Zone integration: Guitar Campfire compiles ---
TEST(ChordUtilitiesZoneIntegration, GuitarCampfire_Triad_CompilesToChordPool) {
  ScaleLibrary scaleLib;
  ZoneManager zoneMgr(scaleLib);
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  SettingsManager settingsMgr;
  TouchpadMixerManager touchpadMixerMgr;
  presetMgr.ensureStaticLayers();

  auto zone = std::make_shared<Zone>();
  zone->name = "Guitar Campfire";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->instrumentMode = Zone::InstrumentMode::Guitar;
  zone->guitarPlayerPosition = Zone::GuitarPlayerPosition::Campfire;
  zoneMgr.addZone(zone);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, touchpadMixerMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[81];
  ASSERT_TRUE(slot.isActive);
  ASSERT_GE(slot.chordIndex, 0);
  const auto &chord = context->chordPool[static_cast<size_t>(slot.chordIndex)];
  EXPECT_GE(chord.size(), 1u);
  EXPECT_LE(chord.size(), 6u);
}

// --- Zone integration: Guitar Rhythm (Virtual Capo) compiles ---
TEST(ChordUtilitiesZoneIntegration,
     GuitarRhythm_VirtualCapo_CompilesToChordPool) {
  ScaleLibrary scaleLib;
  ZoneManager zoneMgr(scaleLib);
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  SettingsManager settingsMgr;
  TouchpadMixerManager touchpadMixerMgr;
  presetMgr.ensureStaticLayers();

  auto zone = std::make_shared<Zone>();
  zone->name = "Guitar Rhythm";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->instrumentMode = Zone::InstrumentMode::Guitar;
  zone->guitarPlayerPosition = Zone::GuitarPlayerPosition::Rhythm;
  zone->guitarFretAnchor = 5;
  zoneMgr.addZone(zone);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, touchpadMixerMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[81];
  ASSERT_TRUE(slot.isActive);
  ASSERT_GE(slot.chordIndex, 0);
  const auto &chord = context->chordPool[static_cast<size_t>(slot.chordIndex)];
  EXPECT_GE(chord.size(), 1u);
  EXPECT_LE(chord.size(), 6u);
}
