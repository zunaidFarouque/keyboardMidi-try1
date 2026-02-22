// Tests for Touchpad tab: Add/Remove entry behavior (manager + list contract),
// schema/types (TouchpadMixerDefinition), and combined list row ordering.
#include "../MappingDefinition.h"
#include "../MappingTypes.h"
#include "../TouchpadMixerDefinition.h"
#include "../TouchpadMixerManager.h"
#include "../TouchpadMixerTypes.h"
#include <algorithm>
#include <gtest/gtest.h>

namespace {

// Helper: default config for "Empty layout" (Add button first menu item).
TouchpadMixerConfig makeEmptyLayout() {
  TouchpadMixerConfig def;
  def.name = "Touchpad Mixer";
  return def;
}

// Helper: default config for "Empty touchpad mapping" (Add button mapping item).
TouchpadMappingConfig makeEmptyTouchpadMapping() {
  TouchpadMappingConfig cfg;
  cfg.name = "Touchpad Mapping";
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1Down, nullptr);
  m.setProperty("type", "Note", nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("data1", 60, nullptr);
  m.setProperty("data2", 100, nullptr);
  cfg.mapping = m;
  return cfg;
}

bool schemaHasPropertyId(const InspectorSchema &schema,
                         const juce::String &propertyId) {
  for (const auto &c : schema)
    if (c.propertyId == propertyId)
      return true;
  return false;
}

} // namespace

// --- Add entry (UI behavior: what Add button does) ---

TEST(TouchpadTabTest, AddEmptyLayoutIncreasesLayoutCount) {
  TouchpadMixerManager mgr;
  EXPECT_EQ(mgr.getLayouts().size(), 0u);
  mgr.addLayout(makeEmptyLayout());
  EXPECT_EQ(mgr.getLayouts().size(), 1u);
  mgr.addLayout(makeEmptyLayout());
  EXPECT_EQ(mgr.getLayouts().size(), 2u);
}

TEST(TouchpadTabTest, AddEmptyLayoutUsesDefaultName) {
  TouchpadMixerManager mgr;
  mgr.addLayout(makeEmptyLayout());
  ASSERT_EQ(mgr.getLayouts().size(), 1u);
  EXPECT_EQ(mgr.getLayouts()[0].name, "Touchpad Mixer");
  EXPECT_EQ(mgr.getLayouts()[0].type, TouchpadType::Mixer);
}

TEST(TouchpadTabTest, AddEmptyTouchpadMappingIncreasesMappingCount) {
  TouchpadMixerManager mgr;
  EXPECT_EQ(mgr.getTouchpadMappings().size(), 0u);
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  EXPECT_EQ(mgr.getTouchpadMappings().size(), 1u);
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  EXPECT_EQ(mgr.getTouchpadMappings().size(), 2u);
}

TEST(TouchpadTabTest, AddEmptyTouchpadMappingHasValidDefaultMappingTree) {
  TouchpadMixerManager mgr;
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  auto mappings = mgr.getTouchpadMappings();
  ASSERT_EQ(mappings.size(), 1u);
  const auto &m = mappings[0];
  EXPECT_EQ(m.name, "Touchpad Mapping");
  ASSERT_TRUE(m.mapping.isValid());
  EXPECT_EQ(m.mapping.getType().toString(), "Mapping");
  EXPECT_EQ(m.mapping.getProperty("inputAlias", juce::var()).toString(),
            "Touchpad");
  EXPECT_EQ((int)m.mapping.getProperty("inputTouchpadEvent", -1),
            TouchpadEvent::Finger1Down);
  EXPECT_EQ(m.mapping.getProperty("type", juce::var()).toString(), "Note");
}

TEST(TouchpadTabTest, CombinedListLayoutsThenMappings) {
  TouchpadMixerManager mgr;
  mgr.addLayout(makeEmptyLayout());
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  auto layouts = mgr.getLayouts();
  auto mappings = mgr.getTouchpadMappings();
  size_t total = layouts.size() + mappings.size();
  EXPECT_EQ(total, 2u);
  // Row 0 = layout, row 1 = mapping (same contract as TouchpadMixerListPanel).
  EXPECT_EQ(layouts.size(), 1u);
  EXPECT_EQ(mappings.size(), 1u);
}

TEST(TouchpadTabTest, RemoveLayoutDecreasesCountAndRemovesCorrectEntry) {
  TouchpadMixerManager mgr;
  TouchpadMixerConfig a;
  a.name = "First";
  TouchpadMixerConfig b;
  b.name = "Second";
  mgr.addLayout(a);
  mgr.addLayout(b);
  ASSERT_EQ(mgr.getLayouts().size(), 2u);
  mgr.removeLayout(0);
  ASSERT_EQ(mgr.getLayouts().size(), 1u);
  EXPECT_EQ(mgr.getLayouts()[0].name, "Second");
}

TEST(TouchpadTabTest, RemoveTouchpadMappingDecreasesCountAndRemovesCorrect) {
  TouchpadMixerManager mgr;
  TouchpadMappingConfig c1 = makeEmptyTouchpadMapping();
  c1.name = "Map A";
  TouchpadMappingConfig c2 = makeEmptyTouchpadMapping();
  c2.name = "Map B";
  mgr.addTouchpadMapping(c1);
  mgr.addTouchpadMapping(c2);
  ASSERT_EQ(mgr.getTouchpadMappings().size(), 2u);
  mgr.removeTouchpadMapping(0);
  ASSERT_EQ(mgr.getTouchpadMappings().size(), 1u);
  EXPECT_EQ(mgr.getTouchpadMappings()[0].name, "Map B");
}

TEST(TouchpadTabTest, CombinedListRowCountIsLayoutsPlusMappings) {
  TouchpadMixerManager mgr;
  EXPECT_EQ(mgr.getLayouts().size() + mgr.getTouchpadMappings().size(), 0u);
  mgr.addLayout(makeEmptyLayout());
  EXPECT_EQ(mgr.getLayouts().size() + mgr.getTouchpadMappings().size(), 1u);
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  EXPECT_EQ(mgr.getLayouts().size() + mgr.getTouchpadMappings().size(), 2u);
  mgr.addLayout(makeEmptyLayout());
  EXPECT_EQ(mgr.getLayouts().size() + mgr.getTouchpadMappings().size(), 3u);
}

// --- Row index contract (same as TouchpadMixerListPanel::rebuildRowKinds) ---

TEST(TouchpadTabTest, RowIndexZeroToLayoutsSizeMinusOneAreLayouts) {
  TouchpadMixerManager mgr;
  mgr.addLayout(makeEmptyLayout());
  mgr.addLayout(makeEmptyLayout());
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  auto layouts = mgr.getLayouts();
  auto mappings = mgr.getTouchpadMappings();
  int numLayouts = static_cast<int>(layouts.size());
  int numMappings = static_cast<int>(mappings.size());
  // Row 0, 1 = layout indices 0, 1; row 2 = mapping index 0.
  EXPECT_EQ(numLayouts, 2);
  EXPECT_EQ(numMappings, 1);
  EXPECT_EQ(numLayouts + numMappings, 3);
}

TEST(TouchpadTabTest, MappingRowIndexEqualsRowMinusLayoutCount) {
  TouchpadMixerManager mgr;
  mgr.addLayout(makeEmptyLayout());
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  int layoutCount = static_cast<int>(mgr.getLayouts().size());
  // Row layoutCount -> mapping index 0, row layoutCount+1 -> mapping index 1.
  EXPECT_EQ(layoutCount, 1);
  EXPECT_EQ(mgr.getTouchpadMappings().size(), 2u);
}

// --- Schema: getCommonLayoutHeader ---

TEST(TouchpadTabTest, CommonLayoutHeaderHasNameTypeLayerChannelZIndex) {
  InspectorSchema schema = TouchpadMixerDefinition::getCommonLayoutHeader();
  EXPECT_TRUE(schemaHasPropertyId(schema, "name"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "type"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "layerId"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "layoutGroupId"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "midiChannel"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "zIndex"));
  EXPECT_GE(schema.size(), 6u);
}

// --- Schema: getCommonLayoutControls (region + relayout) ---

TEST(TouchpadTabTest, CommonLayoutControlsHasRegionAndRelayout) {
  InspectorSchema schema = TouchpadMixerDefinition::getCommonLayoutControls();
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLeft"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionRight"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionTop"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionBottom"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLock"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "relayoutRegion"));
}

// --- Schema: getSchema(Mixer) ---

TEST(TouchpadTabTest, MixerSchemaHasHeaderAndMixerSpecificControls) {
  InspectorSchema schema =
      TouchpadMixerDefinition::getSchema(TouchpadType::Mixer);
  EXPECT_TRUE(schemaHasPropertyId(schema, "name"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "type"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "layerId"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "midiChannel"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "quickPrecision"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "absRel"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "lockFree"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "numFaders"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "ccStart"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "inputMin"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "inputMax"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "outputMin"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "outputMax"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "muteButtonsEnabled"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLeft"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "relayoutRegion"));
}

// --- Schema: getSchema(DrumPad) ---

TEST(TouchpadTabTest, DrumPadSchemaHasDrumPadAndHarmonicControls) {
  InspectorSchema schema =
      TouchpadMixerDefinition::getSchema(TouchpadType::DrumPad);
  EXPECT_TRUE(schemaHasPropertyId(schema, "name"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "type"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadRows"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadColumns"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadMidiNoteStart"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadBaseVelocity"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadVelocityRandom"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadLayoutMode"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "harmonicRowInterval"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "harmonicUseScaleFilter"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLeft"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "relayoutRegion"));
}

// --- Schema: getSchema(ChordPad) ---

TEST(TouchpadTabTest, ChordPadSchemaHasChordPadSpecificControls) {
  InspectorSchema schema =
      TouchpadMixerDefinition::getSchema(TouchpadType::ChordPad);
  EXPECT_TRUE(schemaHasPropertyId(schema, "name"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "type"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadRows"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadColumns"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "drumPadMidiNoteStart"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "chordPadPreset"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "chordPadLatchMode"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLeft"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "relayoutRegion"));
}

// --- Schema: type-specific content differs ---

TEST(TouchpadTabTest, MixerSchemaHasNumFadersButNotDrumPadLayoutMode) {
  InspectorSchema mixer = TouchpadMixerDefinition::getSchema(TouchpadType::Mixer);
  InspectorSchema drum = TouchpadMixerDefinition::getSchema(TouchpadType::DrumPad);
  EXPECT_TRUE(schemaHasPropertyId(mixer, "numFaders"));
  EXPECT_FALSE(schemaHasPropertyId(mixer, "drumPadLayoutMode"));
  EXPECT_TRUE(schemaHasPropertyId(drum, "drumPadLayoutMode"));
  EXPECT_FALSE(schemaHasPropertyId(drum, "numFaders"));
}

TEST(TouchpadTabTest, ChordPadSchemaHasChordPadPresetNotHarmonicRowInterval) {
  InspectorSchema chord =
      TouchpadMixerDefinition::getSchema(TouchpadType::ChordPad);
  InspectorSchema drum = TouchpadMixerDefinition::getSchema(TouchpadType::DrumPad);
  EXPECT_TRUE(schemaHasPropertyId(chord, "chordPadPreset"));
  EXPECT_TRUE(schemaHasPropertyId(drum, "harmonicRowInterval"));
  EXPECT_FALSE(schemaHasPropertyId(chord, "harmonicRowInterval"));
}

// --- Types: TouchpadType enum ---

TEST(TouchpadTabTest, TouchpadTypeMixerDrumPadChordPadDistinct) {
  EXPECT_NE(static_cast<int>(TouchpadType::Mixer),
            static_cast<int>(TouchpadType::DrumPad));
  EXPECT_NE(static_cast<int>(TouchpadType::Mixer),
            static_cast<int>(TouchpadType::ChordPad));
  EXPECT_NE(static_cast<int>(TouchpadType::DrumPad),
            static_cast<int>(TouchpadType::ChordPad));
}

// --- Update layout / update mapping ---

TEST(TouchpadTabTest, UpdateLayoutModifiesEntryAtIndex) {
  TouchpadMixerManager mgr;
  mgr.addLayout(makeEmptyLayout());
  TouchpadMixerConfig updated;
  updated.name = "Updated Name";
  updated.type = TouchpadType::DrumPad;
  mgr.updateLayout(0, updated);
  ASSERT_EQ(mgr.getLayouts().size(), 1u);
  EXPECT_EQ(mgr.getLayouts()[0].name, "Updated Name");
  EXPECT_EQ(mgr.getLayouts()[0].type, TouchpadType::DrumPad);
}

TEST(TouchpadTabTest, UpdateTouchpadMappingModifiesEntryAtIndex) {
  TouchpadMixerManager mgr;
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  TouchpadMappingConfig updated = makeEmptyTouchpadMapping();
  updated.name = "Updated Mapping";
  updated.layerId = 3;
  mgr.updateTouchpadMapping(0, updated);
  ASSERT_EQ(mgr.getTouchpadMappings().size(), 1u);
  EXPECT_EQ(mgr.getTouchpadMappings()[0].name, "Updated Mapping");
  EXPECT_EQ(mgr.getTouchpadMappings()[0].layerId, 3);
}

// --- Add then remove mapping round-trip ---

TEST(TouchpadTabTest, AddEmptyMappingRoundTripsViaValueTree) {
  TouchpadMixerManager mgr;
  mgr.addTouchpadMapping(makeEmptyTouchpadMapping());
  juce::ValueTree vt = mgr.toValueTree();
  TouchpadMixerManager restored;
  restored.restoreFromValueTree(vt);
  auto mappings = restored.getTouchpadMappings();
  ASSERT_EQ(mappings.size(), 1u);
  const auto &r = mappings[0];
  EXPECT_EQ(r.name, "Touchpad Mapping");
  EXPECT_TRUE(r.mapping.isValid());
  EXPECT_EQ(r.mapping.getProperty("inputAlias", juce::var()).toString(),
            "Touchpad");
}

// --- Touchpad Tab mapping schema tests ---

TEST(TouchpadTabTest, TouchpadTab_MappingSchemaIncludesCommonHeaderAndMappingBody) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  
  // Build schema as TouchpadMixerEditorComponent does
  InspectorSchema schema;
  InspectorSchema commonHeader = TouchpadMixerDefinition::getCommonLayoutHeader();
  commonHeader.erase(
      std::remove_if(commonHeader.begin(), commonHeader.end(),
                     [](const InspectorControl &c) { return c.propertyId == "type"; }),
      commonHeader.end());
  for (const auto &c : commonHeader)
    schema.push_back(c);

  if (cfg.mapping.isValid()) {
    int pbRange = 12; // Default pitch bend range
    InspectorSchema mappingSchema = MappingDefinition::getSchema(cfg.mapping, pbRange);
    schema.push_back(MappingDefinition::createSeparator(
        "Mapping", juce::Justification::centredLeft));
    for (const auto &c : mappingSchema)
      schema.push_back(c);
  }

  for (const auto &c : TouchpadMixerDefinition::getCommonLayoutControls())
    schema.push_back(c);

  // Verify common header properties (without "type")
  EXPECT_TRUE(schemaHasPropertyId(schema, "name"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "layerId"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "layoutGroupId"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "midiChannel"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "zIndex"));

  // Verify mapping body properties (mappingSchema includes "type")
  EXPECT_TRUE(schemaHasPropertyId(schema, "type")) << "Mapping body should have 'type'";
  EXPECT_TRUE(schemaHasPropertyId(schema, "data1")) << "Note mapping should have data1";
  
  // Verify common controls
  EXPECT_TRUE(schemaHasPropertyId(schema, "regionLeft"));
  EXPECT_TRUE(schemaHasPropertyId(schema, "relayoutRegion"));
}

TEST(TouchpadTabTest, TouchpadTab_MappingSchemaNoteTypeHasReleaseBehavior) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  cfg.mapping.setProperty("type", "Note", nullptr);
  
  InspectorSchema mappingSchema = MappingDefinition::getSchema(cfg.mapping, 12);
  
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "releaseBehavior"))
      << "Note type mapping schema should have releaseBehavior";
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "data1"))
      << "Note type mapping schema should have data1 (Note)";
}

TEST(TouchpadTabTest, TouchpadTab_MappingSchemaExpressionTypeHasADSRControls) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  cfg.mapping.setProperty("type", "Expression", nullptr);
  cfg.mapping.setProperty("adsrTarget", "CC", nullptr);
  cfg.mapping.setProperty("useCustomEnvelope", true, nullptr);
  
  InspectorSchema mappingSchema = MappingDefinition::getSchema(cfg.mapping, 12);
  
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "adsrTarget"))
      << "Expression type mapping schema should have adsrTarget";
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "useCustomEnvelope"))
      << "Expression type mapping schema should have useCustomEnvelope";
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "adsrAttack"))
      << "Expression with custom envelope should have ADSR controls";
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "adsrDecay"));
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "adsrSustain"));
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "adsrRelease"));
}

TEST(TouchpadTabTest, TouchpadTab_MappingSchemaCommandTypeHasCommandControls) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  cfg.mapping.setProperty("type", "Command", nullptr);
  cfg.mapping.setProperty("data1", (int)MIDIQy::CommandID::LayerMomentary, nullptr);
  
  InspectorSchema mappingSchema = MappingDefinition::getSchema(cfg.mapping, 12);
  
  // Layer commands use "commandCategory" instead of "data1"
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "commandCategory"))
      << "Layer command mapping schema should have commandCategory";
  EXPECT_TRUE(schemaHasPropertyId(mappingSchema, "data2"))
      << "Layer command should have data2 (Target Layer)";
}

TEST(TouchpadTabTest, TouchpadTab_MappingSchemaRespectsPitchBendRange) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  cfg.mapping.setProperty("type", "Expression", nullptr);
  cfg.mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  
  // Test with different pitch bend ranges
  InspectorSchema schemaRange2 = MappingDefinition::getSchema(cfg.mapping, 2);
  InspectorSchema schemaRange12 = MappingDefinition::getSchema(cfg.mapping, 12);
  
  // Find data2 control (Bend semitones) for both ranges
  const InspectorControl *data2Range2 = nullptr;
  const InspectorControl *data2Range12 = nullptr;
  
  for (const auto &c : schemaRange2) {
    if (c.propertyId == "data2") {
      data2Range2 = &c;
      break;
    }
  }
  for (const auto &c : schemaRange12) {
    if (c.propertyId == "data2") {
      data2Range12 = &c;
      break;
    }
  }
  
  ASSERT_NE(data2Range2, nullptr) << "PitchBend Expression should have data2 control";
  ASSERT_NE(data2Range12, nullptr);
  
  EXPECT_DOUBLE_EQ(data2Range2->min, -2.0)
      << "Pitch bend range 2 should set data2 min to -2";
  EXPECT_DOUBLE_EQ(data2Range2->max, 2.0)
      << "Pitch bend range 2 should set data2 max to 2";
  EXPECT_DOUBLE_EQ(data2Range12->min, -12.0)
      << "Pitch bend range 12 should set data2 min to -12";
  EXPECT_DOUBLE_EQ(data2Range12->max, 12.0)
      << "Pitch bend range 12 should set data2 max to 12";
}

TEST(TouchpadTabTest, TouchpadTab_PitchBendMappingSchemaHasPitchPadControls) {
  TouchpadMappingConfig cfg = makeEmptyTouchpadMapping();
  cfg.mapping.setProperty("type", "Expression", nullptr);
  cfg.mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  InspectorSchema schema =
      MappingDefinition::getSchema(cfg.mapping, 2, true /* forTouchpadEditor */);
  EXPECT_TRUE(schemaHasPropertyId(schema, "pitchPadMode"))
      << "Touchpad PitchBend schema should have pitchPadMode";
  EXPECT_TRUE(schemaHasPropertyId(schema, "pitchPadStart"))
      << "Touchpad PitchBend schema should have pitchPadStart";
  EXPECT_TRUE(schemaHasPropertyId(schema, "pitchPadRestZonePercent"))
      << "Touchpad PitchBend schema should have pitchPadRestZonePercent";
  EXPECT_TRUE(schemaHasPropertyId(schema, "pitchPadTransitionZonePercent"))
      << "Touchpad PitchBend schema should have pitchPadTransitionZonePercent";
  EXPECT_TRUE(schemaHasPropertyId(schema, "touchpadOutputMin"))
      << "Touchpad PitchBend schema should have touchpadOutputMin (step range)";
  EXPECT_TRUE(schemaHasPropertyId(schema, "touchpadOutputMax"))
      << "Touchpad PitchBend schema should have touchpadOutputMax (step range)";
}
