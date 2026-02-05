#include "../ChordUtilities.h"
#include "../DeviceManager.h"
#include "../GridCompiler.h"
#include "../MappingTypes.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
#include "../Zone.h"
#include "../ZoneManager.h"
#include <cmath>
#include <gtest/gtest.h>

class GridCompilerTest : public ::testing::Test {
protected:
  // Core Dependencies
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  ScaleLibrary scaleLib;
  SettingsManager settingsMgr;
  ZoneManager zoneMgr{scaleLib};

  // Mock Device Hash
  const juce::String aliasName = "TestDevice";
  uintptr_t aliasHash;

  void SetUp() override {
    // Setup a clean state
    presetMgr.getLayersList().removeAllChildren(nullptr);
    presetMgr.ensureStaticLayers();

    // Create a test alias
    deviceMgr.createAlias(aliasName);
    aliasHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
  }

  // Helper to add a manual mapping
  void addMapping(int layerId, int keyCode, uintptr_t deviceH,
                  ActionType type = ActionType::Note) {
    auto mappings = presetMgr.getMappingsListForLayer(layerId);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)deviceH).toUpperCase(),
                  nullptr);
    juce::String typeStr = "Note";
    if (type == ActionType::Expression)
      typeStr = "Expression";
    else if (type == ActionType::Command)
      typeStr = "Command";
    m.setProperty("type", typeStr, nullptr);
    m.setProperty("layerID", layerId, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper to add a Command mapping (e.g. LayerMomentary)
  void addCommandMapping(int layerId, int keyCode, uintptr_t deviceH,
                         int commandId, int data2) {
    auto mappings = presetMgr.getMappingsListForLayer(layerId);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)deviceH).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", commandId, nullptr);
    m.setProperty("data2", data2, nullptr);
    m.setProperty("layerID", layerId, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper to add a Zone
  void addZone(int layerId, int startKey, int count, uintptr_t targetHash) {
    auto zone = zoneMgr.createDefaultZone();
    zone->layerID = layerId;
    zone->targetAliasHash = targetHash;
    zone->inputKeyCodes.clear();
    for (int i = 0; i < count; ++i)
      zone->inputKeyCodes.push_back(startKey + i);
    // Zone is already added to manager by createDefaultZone
  }

  // Helper to add a zone that compiles to a chord (for inheritance tests).
  void addZoneWithChord(int layerId, int keyCode, uintptr_t targetHash) {
    auto zone = std::make_shared<Zone>();
    zone->name = "Triad Zone L" + juce::String(layerId);
    zone->layerID = layerId;
    zone->targetAliasHash = targetHash;
    zone->inputKeyCodes = {keyCode};
    zone->chordType = ChordUtilities::ChordType::Triad;
    zone->scaleName = "Major";
    zone->rootNote = 60;
    zone->instrumentMode = Zone::InstrumentMode::Piano;
    zone->pianoVoicingStyle = Zone::PianoVoicingStyle::Close;
    zoneMgr.addZone(zone);
  }

  // Helper: move one mapping by row index from source layer to target layer
  // (same logic as MappingEditorComponent::moveSelectedMappingsToLayer).
  void moveMappingToLayer(int sourceLayerId, int rowIndex, int targetLayerId) {
    if (sourceLayerId == targetLayerId || targetLayerId < 0 || targetLayerId > 8)
      return;
    auto src = presetMgr.getMappingsListForLayer(sourceLayerId);
    auto tgt = presetMgr.getMappingsListForLayer(targetLayerId);
    if (!src.isValid() || !tgt.isValid() || rowIndex < 0 ||
        rowIndex >= src.getNumChildren())
      return;
    auto child = src.getChild(rowIndex);
    if (!child.isValid())
      return;
    juce::ValueTree copy = child.createCopy();
    copy.setProperty("layerID", targetLayerId, nullptr);
    tgt.addChild(copy, -1, nullptr);
    src.removeChild(child, nullptr);
  }

  // Helper: move multiple mappings by row indices (descending order).
  void moveMappingsToLayer(int sourceLayerId,
                           const juce::Array<int> &rowIndices,
                           int targetLayerId) {
    if (sourceLayerId == targetLayerId || targetLayerId < 0 || targetLayerId > 8)
      return;
    auto src = presetMgr.getMappingsListForLayer(sourceLayerId);
    auto tgt = presetMgr.getMappingsListForLayer(targetLayerId);
    if (!src.isValid() || !tgt.isValid())
      return;
    juce::Array<int> rows = rowIndices;
    rows.sort();
    for (int i = rows.size(); --i >= 0;) {
      int row = rows[i];
      if (row >= 0 && row < src.getNumChildren()) {
        auto child = src.getChild(row);
        if (child.isValid()) {
          juce::ValueTree copy = child.createCopy();
          copy.setProperty("layerID", targetLayerId, nullptr);
          tgt.addChild(copy, -1, nullptr);
          src.removeChild(child, nullptr);
        }
      }
    }
  }

  // Helper to set layer inheritance flags (solo, passthru, private)
  void setLayerSolo(int layerId, bool value = true) {
    auto layer = presetMgr.getLayerNode(layerId);
    if (layer.isValid())
      layer.setProperty("soloLayer", value, nullptr);
  }
  void setLayerPassthru(int layerId, bool value = true) {
    auto layer = presetMgr.getLayerNode(layerId);
    if (layer.isValid())
      layer.setProperty("passthruInheritance", value, nullptr);
  }
  void setLayerPrivate(int layerId, bool value = true) {
    auto layer = presetMgr.getLayerNode(layerId);
    if (layer.isValid())
      layer.setProperty("privateToLayer", value, nullptr);
  }

  // Helper to add a Touchpad mapping (inputAlias "Touchpad")
  void addTouchpadMapping(int layerId, int eventId,
                          const juce::String &typeStr = "Note",
                          const juce::String &releaseBehavior = "Send Note Off",
                          bool followTranspose = true) {
    auto mappings = presetMgr.getMappingsListForLayer(layerId);
    juce::ValueTree m("Mapping");
    m.setProperty("inputAlias", "Touchpad", nullptr);
    m.setProperty("inputTouchpadEvent", eventId, nullptr);
    m.setProperty("type", typeStr, nullptr);
    m.setProperty("layerID", layerId, nullptr);
    m.setProperty("releaseBehavior", releaseBehavior, nullptr);
    m.setProperty("followTranspose", followTranspose, nullptr);
    m.setProperty("channel", 1, nullptr);
    m.setProperty("data1", 60, nullptr);
    m.setProperty("data2", 127, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
};

// Test Case 1: Vertical Inheritance (Layer 0 -> Layer 1)
TEST_F(GridCompilerTest, VerticalInheritance) {
  // Arrange: Map Q (Key 81) on Layer 0 (Global)
  addMapping(0, 81, 0);

  // Act: Compile
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert: Layer 0 Global
  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[81].state, VisualState::Active);

  // Assert: Layer 1 Global
  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l1)[81].displayColor.getAlpha(),
            76); // ~0.3f alpha (checking if it is dimmed)
}

// Test Case 2: Vertical Override (Layer 1 blocks Layer 0)
TEST_F(GridCompilerTest, VerticalOverride) {
  // Arrange
  addMapping(0, 81, 0); // Base: Note
  addMapping(1, 81, 0); // Overlay: Note

  // Act
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert
  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Override);
}

// Test Case 3: Conflict Detection (The Bug Fix)
TEST_F(GridCompilerTest, ConflictDetection) {
  // Arrange: Layer 0
  // 1. Add Mapping on Key 81
  addMapping(0, 81, 0);

  // 2. Add Zone covering Key 81
  addZone(0, 81, 1, 0); // Zone with 1 key (81)

  // Act
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert
  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[81].state, VisualState::Conflict);
  EXPECT_EQ((*l0)[81].displayColor, juce::Colours::red);
}

// Test Case 4: Device Specific Override
TEST_F(GridCompilerTest, DeviceOverridesGlobal) {
  // Arrange
  addMapping(0, 81, 0);         // Global Q
  addMapping(0, 81, aliasHash); // Specific Q

  // Act
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert: Global Grid should be Active
  auto globalGrid = context->visualLookup[0][0];
  EXPECT_EQ((*globalGrid)[81].state, VisualState::Active);

  // Assert: Device Grid should be Override (Local overrides inherited Global)
  auto deviceGrid = context->visualLookup[aliasHash][0];
  EXPECT_EQ((*deviceGrid)[81].state, VisualState::Override);
}

// TEST C: Horizontal Inheritance (Global flows into Device)
TEST_F(GridCompilerTest, DeviceInheritsFromGlobal) {
  // Arrange: Map Q globally on Layer 0
  addMapping(0, 81, 0);

  // Act
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert: The specific device should see this as Inherited
  auto deviceGrid = context->visualLookup[aliasHash][0];
  EXPECT_EQ((*deviceGrid)[81].state, VisualState::Inherited);
}

// TEST D: Horizontal Override (Device masks Global) – device maps Q to CC
TEST_F(GridCompilerTest, DeviceOverridesGlobalWithCC) {
  // Arrange
  addMapping(0, 81, 0); // Global maps Q to Note
  addMapping(0, 81, aliasHash,
             ActionType::Expression); // Device maps Q to Expression

  // Act
  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  // Assert: Device grid should show Override
  auto deviceGrid = context->visualLookup[aliasHash][0];
  EXPECT_EQ((*deviceGrid)[81].state, VisualState::Override);
}

// TEST F: Generic Modifier Expansion
// If I map "Shift" (0x10), it should automatically map LShift (0xA0) and RShift
// (0xA1).
TEST_F(GridCompilerTest, GenericShiftExpandsToSides) {
  addMapping(0, 0x10, 0); // VK_SHIFT

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->visualLookup[0][0];

  EXPECT_EQ((*grid)[0xA0].state, VisualState::Active); // Left Shift
  EXPECT_EQ((*grid)[0xA1].state, VisualState::Active); // Right Shift
}

// TEST G: Generic Modifier Override
// If I map "Shift" (Generic) to CC, but then specifically map "LShift" to Note,
// LShift uses the specific mapping.
TEST_F(GridCompilerTest, SpecificModifierOverridesGeneric) {
  addMapping(0, 0x10, 0, ActionType::Expression); // Generic -> Expression
  addMapping(0, 0xA0, 0, ActionType::Note);       // LShift -> Note

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->visualLookup[0][0];

  // LShift: specific Note (default data1=60 -> "C4")
  EXPECT_EQ((*grid)[0xA0].label, "C4");
  // RShift: inherited generic CC (default data1=60)
  EXPECT_EQ((*grid)[0xA1].label, "Expr: CC");
}

// TEST H: Chord Compilation (Audio Data)
TEST_F(GridCompilerTest, ZoneCompilesToChordPool) {
  auto zone = std::make_shared<Zone>();
  zone->name = "Triad Zone";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zoneMgr.addZone(zone); // addZone calls rebuildZoneCache

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto audioGrid = context->globalGrids[0];

  const auto &slot = (*audioGrid)[81];
  EXPECT_TRUE(slot.isActive);
  EXPECT_GE(slot.chordIndex, 0);

  ASSERT_LT(static_cast<size_t>(slot.chordIndex), context->chordPool.size());
  const auto &chord = context->chordPool[static_cast<size_t>(slot.chordIndex)];
  EXPECT_EQ(chord.size(), 3u); // Triad = 3 notes
}

// Zone useGlobalRoot: when true, rebuildZoneCache uses global root
TEST_F(GridCompilerTest, ZoneUseGlobalRoot_UsesGlobalRootWhenCompiling) {
  scaleLib.loadDefaults();
  zoneMgr.setGlobalRoot(48); // G3
  auto zone = std::make_shared<Zone>();
  zone->name = "GlobalRoot Zone";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81}; // Q
  zone->chordType = ChordUtilities::ChordType::None;
  zone->scaleName = "Major";
  zone->rootNote = 60;        // Ignored when useGlobalRoot true
  zone->useGlobalRoot = true;
  zone->globalRootOctaveOffset = 0;
  zone->layoutStrategy = Zone::LayoutStrategy::Linear;
  zoneMgr.addZone(zone);      // addZone calls rebuildZoneCache with root 48

  auto zones = zoneMgr.getZones();
  ASSERT_EQ(zones.size(), 1u);
  auto notesOpt = zones[0]->getNotesForKey(81, 0, 0);
  ASSERT_TRUE(notesOpt.has_value() && !notesOpt->empty());
  EXPECT_EQ((*notesOpt)[0].pitch, 48)
      << "useGlobalRoot true: getNotesForKey should use global root 48";
}

// Phase 53.4: Layer Commands (e.g. LayerMomentary) must not be inherited.
// Key 10 on Layer 0 is the command; Layer 1 should show Empty for that key.
TEST_F(GridCompilerTest, LayerCommandsAreNotInherited) {
  addCommandMapping(0, 10, 0,
                    static_cast<int>(MIDIQy::CommandID::LayerMomentary), 1);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[10].state, VisualState::Active);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[10].state, VisualState::Empty);
}

// Phase 53.5: LayerToggle must not be inherited (same filter as LayerMomentary)
TEST_F(GridCompilerTest, LayerToggleNotInherited) {
  addCommandMapping(0, 11, 0, static_cast<int>(MIDIQy::CommandID::LayerToggle),
                    1);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[11].state, VisualState::Active);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[11].state, VisualState::Empty);
}

// Layer inheritance: Solo layer – layer shows only its own content, no inherit.
TEST_F(GridCompilerTest, LayerInheritanceSoloLayer) {
  addMapping(0, 81, 0); // Base: key 81
  addMapping(1, 82, 0); // Layer 1: key 82
  setLayerSolo(1);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[81].state, VisualState::Active);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Empty); // not inherited
  EXPECT_EQ((*l1)[82].state, VisualState::Active);

  auto l0Audio = context->globalGrids[0];
  auto l1Audio = context->globalGrids[1];
  EXPECT_TRUE((*l0Audio)[81].isActive);
  EXPECT_FALSE((*l1Audio)[81].isActive);
  EXPECT_TRUE((*l1Audio)[82].isActive);
}

// Layer inheritance: Passthru – next layer inherits from below this layer.
TEST_F(GridCompilerTest, LayerInheritancePassthru) {
  addMapping(0, 81, 0); // Layer 0: key 81
  addMapping(1, 82, 0); // Layer 1: key 82
  setLayerPassthru(1);
  addMapping(2, 83, 0); // Layer 2: key 83

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited); // from layer 0
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);    // layer 1 not in base
  EXPECT_EQ((*l2)[83].state, VisualState::Active);

  auto l2Audio = context->globalGrids[2];
  EXPECT_TRUE((*l2Audio)[81].isActive);
  EXPECT_FALSE((*l2Audio)[82].isActive);
  EXPECT_TRUE((*l2Audio)[83].isActive);
}

// Layer inheritance: Private to layer – higher layers do not inherit this layer.
TEST_F(GridCompilerTest, LayerInheritancePrivateToLayer) {
  addMapping(0, 81, 0); // Layer 0: key 81
  addMapping(1, 82, 0); // Layer 1: key 82
  setLayerPrivate(1);
  addMapping(2, 83, 0); // Layer 2: key 83

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l1)[82].state, VisualState::Active);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited); // from layer 0
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);    // layer 1 private
  EXPECT_EQ((*l2)[83].state, VisualState::Active);

  auto l2Audio = context->globalGrids[2];
  EXPECT_TRUE((*l2Audio)[81].isActive);
  EXPECT_FALSE((*l2Audio)[82].isActive);
  EXPECT_TRUE((*l2Audio)[83].isActive);
}

// Layer inheritance: Default (no flags) unchanged – layer 1 still inherits.
TEST_F(GridCompilerTest, LayerInheritanceDefaultUnchanged) {
  addMapping(0, 81, 0);
  addMapping(1, 82, 0);
  // no solo/passthru/private set

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l1)[82].state, VisualState::Active);
}

// Layer inheritance: Combined solo + passthru – layer 1 is solo and passthru;
// layer 2 inherits from layer 0 (not from layer 1).
TEST_F(GridCompilerTest, LayerInheritanceSoloPlusPassthru) {
  addMapping(0, 81, 0);
  addMapping(1, 82, 0);
  setLayerSolo(1);
  setLayerPassthru(1);
  addMapping(2, 83, 0);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l1)[81].state, VisualState::Empty);
  EXPECT_EQ((*l1)[82].state, VisualState::Active);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited); // from L0
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);   // L1 passthru
  EXPECT_EQ((*l2)[83].state, VisualState::Active);
}

// Layer inheritance: Private + passthru – layer 1 private and passthru; L2
// inherits from L0 only (passthru), so L2 never sees L1's key anyway.
TEST_F(GridCompilerTest, LayerInheritancePrivatePlusPassthru) {
  addMapping(0, 81, 0);
  addMapping(1, 82, 0);
  setLayerPrivate(1);
  setLayerPassthru(1);
  addMapping(2, 83, 0);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);
  EXPECT_EQ((*l2)[83].state, VisualState::Active);
}

// Layer inheritance with zones: Solo layer – zone on L0 (key 81), zone on L1
// (key 82); L1 solo so L1 grid has only key 82.
TEST_F(GridCompilerTest, LayerInheritanceSolo_WithZone) {
  addZoneWithChord(0, 81, 0);
  addZoneWithChord(1, 82, 0);
  setLayerSolo(1);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l0 = context->visualLookup[0][0];
  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l0)[81].state, VisualState::Active);
  EXPECT_EQ((*l1)[81].state, VisualState::Empty);
  EXPECT_EQ((*l1)[82].state, VisualState::Active);

  auto l1Audio = context->globalGrids[1];
  EXPECT_FALSE((*l1Audio)[81].isActive);
  EXPECT_TRUE((*l1Audio)[82].isActive);
}

// Layer inheritance with zones: Private – zone on L1 (key 82); L2 does not
// inherit key 82.
TEST_F(GridCompilerTest, LayerInheritancePrivate_WithZone) {
  addZoneWithChord(0, 81, 0);
  addZoneWithChord(1, 82, 0);
  setLayerPrivate(1);
  addMapping(2, 83, 0);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);
  EXPECT_EQ((*l2)[83].state, VisualState::Active);

  auto l2Audio = context->globalGrids[2];
  EXPECT_TRUE((*l2Audio)[81].isActive);
  EXPECT_FALSE((*l2Audio)[82].isActive);
  EXPECT_TRUE((*l2Audio)[83].isActive);
}

// Layer inheritance with zones: Passthru – zone on L1 (key 82); L2 inherits
// from L0 only, so key 82 empty on L2.
TEST_F(GridCompilerTest, LayerInheritancePassthru_WithZone) {
  addZoneWithChord(0, 81, 0);
  addZoneWithChord(1, 82, 0);
  setLayerPassthru(1);
  addMapping(2, 83, 0);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l2)[82].state, VisualState::Empty);
  EXPECT_EQ((*l2)[83].state, VisualState::Active);
}

// Layer inheritance: Serialization – save and load preset preserves
// soloLayer, passthruInheritance, privateToLayer on layer nodes.
TEST_F(GridCompilerTest, LayerInheritanceProperties_SerializeRoundTrip) {
  presetMgr.ensureStaticLayers();
  auto layer1 = presetMgr.getLayerNode(1);
  ASSERT_TRUE(layer1.isValid());
  layer1.setProperty("soloLayer", true, nullptr);
  layer1.setProperty("passthruInheritance", true, nullptr);
  layer1.setProperty("privateToLayer", true, nullptr);

  juce::File file = juce::File::getSpecialLocation(
                        juce::File::tempDirectory)
                        .getNonexistentChildFile("midiqy_layer_", ".xml", false);
  presetMgr.saveToFile(file);
  presetMgr.loadFromFile(file);
  file.deleteFile();

  auto loaded = presetMgr.getLayerNode(1);
  ASSERT_TRUE(loaded.isValid());
  EXPECT_TRUE((bool)loaded.getProperty("soloLayer", false));
  EXPECT_TRUE((bool)loaded.getProperty("passthruInheritance", false));
  EXPECT_TRUE((bool)loaded.getProperty("privateToLayer", false));
}

// Move to layer: preset state after moving one mapping from layer 0 to layer 1.
TEST_F(GridCompilerTest, MoveMappingsToLayer_PresetState) {
  addMapping(0, 81, 0);
  addMapping(0, 82, 0);
  EXPECT_EQ(presetMgr.getMappingsListForLayer(0).getNumChildren(), 2);
  EXPECT_EQ(presetMgr.getMappingsListForLayer(1).getNumChildren(), 0);

  moveMappingToLayer(0, 1, 1);

  EXPECT_EQ(presetMgr.getMappingsListForLayer(0).getNumChildren(), 1);
  EXPECT_EQ(presetMgr.getMappingsListForLayer(1).getNumChildren(), 1);
  auto moved = presetMgr.getMappingsListForLayer(1).getChild(0);
  ASSERT_TRUE(moved.isValid());
  EXPECT_EQ((int)moved.getProperty("layerID", -1), 1);
  EXPECT_EQ((int)moved.getProperty("inputKey", -1), 82);
  EXPECT_EQ((int)presetMgr.getMappingsListForLayer(0).getChild(0).getProperty("inputKey", -1), 81);
}

// Move to layer: compiled grids reflect moved mappings (key 82 on L1, key 81 on L0).
TEST_F(GridCompilerTest, MoveMappingsToLayer_CompiledGridReflectsMove) {
  addMapping(0, 81, 0);
  addMapping(0, 82, 0);
  moveMappingToLayer(0, 1, 1);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto l0 = context->visualLookup[0][0];
  auto l1 = context->visualLookup[0][1];
  EXPECT_EQ((*l0)[81].state, VisualState::Active);
  EXPECT_EQ((*l0)[82].state, VisualState::Empty);
  EXPECT_EQ((*l1)[81].state, VisualState::Inherited);
  EXPECT_EQ((*l1)[82].state, VisualState::Active);

  auto l0Audio = context->globalGrids[0];
  auto l1Audio = context->globalGrids[1];
  EXPECT_TRUE((*l0Audio)[81].isActive);
  EXPECT_FALSE((*l0Audio)[82].isActive);
  EXPECT_TRUE((*l1Audio)[81].isActive);
  EXPECT_TRUE((*l1Audio)[82].isActive);
}

// Move to layer: move multiple mappings (two rows) from layer 0 to layer 2.
TEST_F(GridCompilerTest, MoveMappingsToLayer_MultipleMappings) {
  addMapping(0, 81, 0);
  addMapping(0, 82, 0);
  addMapping(0, 83, 0);
  moveMappingsToLayer(0, {0, 2}, 2);

  EXPECT_EQ(presetMgr.getMappingsListForLayer(0).getNumChildren(), 1);
  EXPECT_EQ(presetMgr.getMappingsListForLayer(2).getNumChildren(), 2);
  auto layer0 = presetMgr.getMappingsListForLayer(0);
  auto layer2 = presetMgr.getMappingsListForLayer(2);
  EXPECT_EQ((int)layer0.getChild(0).getProperty("inputKey", -1), 82);
  juce::Array<int> keysOn2;
  for (int i = 0; i < layer2.getNumChildren(); ++i)
    keysOn2.add((int)layer2.getChild(i).getProperty("inputKey", -1));
  keysOn2.sort();
  EXPECT_TRUE(keysOn2.contains(81));
  EXPECT_TRUE(keysOn2.contains(83));

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto l2 = context->visualLookup[0][2];
  EXPECT_EQ((*l2)[81].state, VisualState::Active);   // moved to L2
  EXPECT_EQ((*l2)[82].state, VisualState::Inherited); // still on L0, inherited
  EXPECT_EQ((*l2)[83].state, VisualState::Active);   // moved to L2
}

// Phase 53.4: Device view vertical inheritance – Layer 0 mapping on device
// should appear as Inherited on Layer 1 of the same device view.
TEST_F(GridCompilerTest, DeviceVerticalInheritanceIsDimmed) {
  addMapping(0, 20, aliasHash); // Key 20 on Layer 0 for TestDevice only

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  auto devL0 = context->visualLookup[aliasHash][0];
  EXPECT_EQ((*devL0)[20].state, VisualState::Active);

  auto devL1 = context->visualLookup[aliasHash][1];
  EXPECT_EQ((*devL1)[20].state, VisualState::Inherited);
}

// Phase 51.7 / 53.5.1: Device supremacy – Device Layer 0 overrides Global
// Layer 1. Device View Layer 1 should show Device Action as Inherited.
TEST_F(GridCompilerTest, DeviceBaseOverridesGlobalLayer) {
  addMapping(1, 81, 0);         // Global Layer 1 (Q -> Note)
  addMapping(0, 81, aliasHash); // Device Layer 0 (Q -> Note)

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto deviceGrid = context->visualLookup[aliasHash][1]; // View Layer 1

  // Device Layer 0 wins over Global Layer 1; from Layer 0 -> Inherited
  EXPECT_EQ((*deviceGrid)[81].state, VisualState::Inherited);
}

// Phase 56.1: Expression with useCustomEnvelope=false -> Fast Path (0,0,1,0)
TEST_F(GridCompilerTest, ExpressionSimpleCcProducesFastPathAdsr) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 50, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("useCustomEnvelope", false, nullptr);
  m.setProperty("adsrTarget", "CC", nullptr);
  m.setProperty("data1", 7, nullptr); // CC number
  m.setProperty("touchpadValueWhenOn", 64, nullptr);
  m.setProperty("touchpadValueWhenOff", 0, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto audioGrid = context->globalGrids[0];
  const auto &slot = (*audioGrid)[50];

  ASSERT_TRUE(slot.isActive);
  EXPECT_EQ(slot.action.type, ActionType::Expression);
  EXPECT_EQ(slot.action.adsrSettings.ccNumber, 7);
  EXPECT_EQ(slot.action.adsrSettings.valueWhenOn, 64);
  EXPECT_EQ(slot.action.adsrSettings.valueWhenOff, 0);
  EXPECT_EQ(slot.action.data2, 64);
  EXPECT_EQ(slot.action.adsrSettings.attackMs, 0);
  EXPECT_EQ(slot.action.adsrSettings.decayMs, 0);
  EXPECT_EQ(slot.action.adsrSettings.releaseMs, 0);
  EXPECT_FLOAT_EQ(slot.action.adsrSettings.sustainLevel, 1.0f);
}

// Phase 56.1: Expression with useCustomEnvelope=true -> reads ADSR from
// ValueTree
TEST_F(GridCompilerTest, ExpressionCustomEnvelopeReadsAdsr) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 51, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("useCustomEnvelope", true, nullptr);
  m.setProperty("adsrTarget", "CC", nullptr);
  m.setProperty("adsrAttack", 100, nullptr);
  m.setProperty("adsrDecay", 50, nullptr);
  m.setProperty("adsrSustain", 0.6f, nullptr);
  m.setProperty("adsrRelease", 200, nullptr);
  m.setProperty("data1", 1, nullptr);
  m.setProperty("touchpadValueWhenOn", 127, nullptr);
  m.setProperty("touchpadValueWhenOff", 0, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto audioGrid = context->globalGrids[0];
  const auto &slot = (*audioGrid)[51];

  ASSERT_TRUE(slot.isActive);
  EXPECT_EQ(slot.action.type, ActionType::Expression);
  EXPECT_EQ(slot.action.adsrSettings.valueWhenOn, 127);
  EXPECT_EQ(slot.action.adsrSettings.valueWhenOff, 0);
  EXPECT_EQ(slot.action.adsrSettings.attackMs, 100);
  EXPECT_EQ(slot.action.adsrSettings.decayMs, 50);
  EXPECT_FLOAT_EQ(slot.action.adsrSettings.sustainLevel, 0.6f);
  EXPECT_EQ(slot.action.adsrSettings.releaseMs, 200);
}

// Expression: value when on/off compiled from touchpadValueWhenOn/Off (keyboard
// and touchpad)
TEST_F(GridCompilerTest, ExpressionValueWhenOnOffCompiled) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 53, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "CC", nullptr);
  m.setProperty("data1", 11, nullptr);
  m.setProperty("touchpadValueWhenOn", 100, nullptr);
  m.setProperty("touchpadValueWhenOff", 20, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[53];

  EXPECT_EQ(slot.action.adsrSettings.valueWhenOn, 100);
  EXPECT_EQ(slot.action.adsrSettings.valueWhenOff, 20);
  EXPECT_EQ(slot.action.data2, 100);
}

// Phase 56.1: Expression with adsrTarget=PitchBend uses Bend (semitones) =
// data2
TEST_F(GridCompilerTest, ExpressionPitchBendCompilesCorrectly) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 52, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("useCustomEnvelope", false, nullptr);
  m.setProperty("adsrTarget", "PitchBend", nullptr);
  m.setProperty("data2", 2, nullptr); // Bend +2 semitones
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto l0 = context->visualLookup[0][0];
  EXPECT_EQ((*l0)[52].label, "Expr: PB");

  auto audioGrid = context->globalGrids[0];
  const auto &slot = (*audioGrid)[52];
  EXPECT_EQ(slot.action.adsrSettings.target, AdsrTarget::PitchBend);
  EXPECT_EQ(slot.action.data2, 2);
}

// Settings: pitch bend range affects compiled Expression PitchBend data2 clamp
TEST_F(GridCompilerTest, SettingsPitchBendRangeAffectsExpressionBend) {
  settingsMgr.setPitchBendRange(6); // ±6 semitones
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 53, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "PitchBend", nullptr);
  m.setProperty("data2", 4, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  const auto &slot = (*context->globalGrids[0])[53];
  EXPECT_EQ(slot.action.adsrSettings.target, AdsrTarget::PitchBend);
  EXPECT_EQ(slot.action.data2, 4);

  presetMgr.getMappingsListForLayer(0).removeChild(0, nullptr);
  juce::ValueTree m2("Mapping");
  m2.setProperty("inputKey", 54, nullptr);
  m2.setProperty("deviceHash",
                 juce::String::toHexString((juce::int64)0).toUpperCase(),
                 nullptr);
  m2.setProperty("type", "Expression", nullptr);
  m2.setProperty("adsrTarget", "PitchBend", nullptr);
  m2.setProperty("data2", 12, nullptr); // +12, should clamp to 6
  m2.setProperty("layerID", 0, nullptr);
  presetMgr.getMappingsListForLayer(0).addChild(m2, -1, nullptr);
  auto ctx2 =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  EXPECT_EQ((*ctx2->globalGrids[0])[54].action.data2, 6)
      << "Bend semitones should be clamped to pitch bend range 6";
}

TEST_F(GridCompilerTest, NoteReleaseBehaviorCompiles) {
  auto addAndCheck = [this](const char *rbStr, NoteReleaseBehavior expected) {
    presetMgr.getMappingsListForLayer(0).removeAllChildren(nullptr);
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", 50, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 60, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("releaseBehavior", juce::String(rbStr), nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);

    auto ctx =
        GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
    auto grid = ctx->globalGrids[0];
    EXPECT_EQ((*grid)[50].action.releaseBehavior, expected)
        << "releaseBehavior \"" << rbStr << "\"";
  };
  addAndCheck("Send Note Off", NoteReleaseBehavior::SendNoteOff);
  addAndCheck("Sustain until retrigger",
              NoteReleaseBehavior::SustainUntilRetrigger);
  addAndCheck("Always Latch", NoteReleaseBehavior::AlwaysLatch);
}

// SmartScaleBend: lookup is built from global scale + smartStepShift + PB range
TEST_F(GridCompilerTest, SmartScaleBendLookupIsBuilt) {
  scaleLib.loadDefaults(); // Ensure Major scale exists
  zoneMgr.setGlobalScale("Major");
  zoneMgr.setGlobalRoot(60);        // C4
  settingsMgr.setPitchBendRange(2); // 2 semitones = full bend for C->D

  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 52, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "SmartScaleBend", nullptr);
  m.setProperty("smartStepShift", 1,
                nullptr); // +1 scale step (C -> D in Major)
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto audioGrid = context->globalGrids[0];
  const auto &slot = (*audioGrid)[52];

  EXPECT_EQ(slot.action.adsrSettings.target, AdsrTarget::SmartScaleBend);
  ASSERT_EQ(slot.action.smartBendLookup.size(), 128u)
      << "SmartScaleBend lookup must have 128 entries";

  // C4 (60) + 1 scale step in C Major = D4 (62). Semitones = 2. PB range 2 ->
  // full bend up = 16383
  int c4Pb = slot.action.smartBendLookup[60];
  EXPECT_EQ(c4Pb, 16383)
      << "C4 +1 scale step (-> D4) with PB range 2 = full bend up";

  // Center (no bend) for note that is already at target: e.g. bend +0 from any
  // note = 8192 (Actually +0 step: target = same note, semitones = 0 -> 8192.
  // We have step 1 so not this.) D4 (62) + 1 scale step = E4 (64). Semitones
  // = 2. Same full bend.
  int d4Pb = slot.action.smartBendLookup[62];
  EXPECT_EQ(d4Pb, 16383)
      << "D4 +1 scale step (-> E4) with PB range 2 = full bend up";
}

// SmartScaleBend: PB value scales with global PB range (2/6 of range -> ~1/3 of
// full bend)
TEST_F(GridCompilerTest, SmartScaleBendScalesWithPitchBendRange) {
  scaleLib.loadDefaults();
  zoneMgr.setGlobalScale("Major");
  zoneMgr.setGlobalRoot(60);
  settingsMgr.setPitchBendRange(
      6); // 6 semitones: 2 semitones = 2/6 = 1/3 of full bend

  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 52, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "SmartScaleBend", nullptr);
  m.setProperty("smartStepShift", 1, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto audioGrid = context->globalGrids[0];
  const auto &slot = (*audioGrid)[52];

  ASSERT_EQ(slot.action.smartBendLookup.size(), 128u);
  // C4 -> D4 = 2 semitones. PB range 6 -> 2/6 * 8192 = 2730.67 up from center
  // -> 8192+2731 = 10923
  int c4Pb = slot.action.smartBendLookup[60];
  int expected = 8192 + static_cast<int>(std::round(8192.0 * 2.0 / 6.0));
  EXPECT_EQ(c4Pb, expected) << "C4 +1 step with PB range 6 = 1/3 of full bend";
}

// --- Touchpad mapping compilation (recent touchpad feature) ---
TEST_F(GridCompilerTest, TouchpadMappingCompiledIntoContext) {
  addTouchpadMapping(0, TouchpadEvent::Finger1Down, "Note");

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  const auto &entry = context->touchpadMappings.front();
  EXPECT_EQ(entry.layerId, 0);
  EXPECT_EQ(entry.eventId, TouchpadEvent::Finger1Down);
  EXPECT_EQ(entry.action.type, ActionType::Note);
  EXPECT_EQ(entry.action.data1, 60);
  EXPECT_EQ(entry.conversionKind, TouchpadConversionKind::BoolToGate);
}

TEST_F(GridCompilerTest, TouchpadNoteReleaseBehaviorApplied) {
  addTouchpadMapping(0, TouchpadEvent::Finger1Down, "Note",
                     "Sustain until retrigger", true);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  EXPECT_EQ(context->touchpadMappings.front().action.releaseBehavior,
            NoteReleaseBehavior::SustainUntilRetrigger);
}

TEST_F(GridCompilerTest, TouchpadNoteAlwaysLatchApplied) {
  addTouchpadMapping(0, TouchpadEvent::Finger2Down, "Note", "Always Latch",
                     true);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  EXPECT_EQ(context->touchpadMappings.front().action.releaseBehavior,
            NoteReleaseBehavior::AlwaysLatch);
}

TEST_F(GridCompilerTest, TouchpadContinuousEventCompiledAsContinuousToGate) {
  addTouchpadMapping(0, TouchpadEvent::Finger1X, "Note"); // continuous event

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  EXPECT_EQ(context->touchpadMappings.front().conversionKind,
            TouchpadConversionKind::ContinuousToGate);
}

TEST_F(GridCompilerTest, TouchpadPitchPadConfigCompiledForPitchBend) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "PitchBend", nullptr);
  m.setProperty("layerID", 0, nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("touchpadInputMin", 0.0, nullptr);
  m.setProperty("touchpadInputMax", 1.0, nullptr);
  m.setProperty("touchpadOutputMin", -2, nullptr);
  m.setProperty("touchpadOutputMax", 2, nullptr);
  m.setProperty("pitchPadRestingPercent", 15.0, nullptr);
  m.setProperty("pitchPadMode", "Relative", nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);

  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  const auto &entry = context->touchpadMappings.front();
  EXPECT_EQ(entry.conversionKind, TouchpadConversionKind::ContinuousToRange);
  ASSERT_TRUE(entry.conversionParams.pitchPadConfig.has_value());
  const auto &cfg = *entry.conversionParams.pitchPadConfig;
  EXPECT_EQ(cfg.minStep, -2);
  EXPECT_EQ(cfg.maxStep, 2);
  EXPECT_NEAR(cfg.restingSpacePercent, 15.0f, 0.001f);
  EXPECT_EQ(cfg.mode, PitchPadMode::Relative);
  EXPECT_TRUE(entry.action.sendReleaseValue)
      << "Pitch-bend touchpad expression should default to resetting PB on "
         "release";
}

TEST_F(GridCompilerTest, TouchpadPitchPadHonoursResetPitchFlag) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "PitchBend", nullptr);
  m.setProperty("layerID", 0, nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("touchpadInputMin", 0.0, nullptr);
  m.setProperty("touchpadInputMax", 1.0, nullptr);
  m.setProperty("touchpadOutputMin", -2, nullptr);
  m.setProperty("touchpadOutputMax", 2, nullptr);
  m.setProperty("pitchPadRestingPercent", 10.0, nullptr);
  m.setProperty("sendReleaseValue", false, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  ASSERT_EQ(context->touchpadMappings.size(), 1u);
  const auto &entry = context->touchpadMappings.front();
  EXPECT_FALSE(entry.action.sendReleaseValue)
      << "sendReleaseValue should reflect mapping property for touchpad "
         "expression PB";
}

// --- Disabled mapping: not compiled into grid ---
TEST_F(GridCompilerTest, DisabledMappingNotCompiled) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 50, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Note", nullptr);
  m.setProperty("data1", 60, nullptr);
  m.setProperty("data2", 127, nullptr);
  m.setProperty("layerID", 0, nullptr);
  m.setProperty("enabled", false, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->globalGrids[0];
  EXPECT_FALSE((*grid)[50].isActive)
      << "Disabled mapping should not appear in compiled grid";
}

// --- Disabled touchpad mapping: not in touchpadMappings ---
TEST_F(GridCompilerTest, DisabledTouchpadMappingNotInContext) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1Down, nullptr);
  m.setProperty("type", "Note", nullptr);
  m.setProperty("layerID", 0, nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("data1", 60, nullptr);
  m.setProperty("data2", 127, nullptr);
  m.setProperty("enabled", false, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  EXPECT_EQ(context->touchpadMappings.size(), 0u)
      << "Disabled touchpad mapping should not be in context";
}

// --- Transpose command: transposeModify and transposeSemitones compiled ---
TEST_F(GridCompilerTest, TransposeCommandCompilesModifyAndSemitones) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 55, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Command", nullptr);
  m.setProperty("data1", (int)MIDIQy::CommandID::Transpose, nullptr);
  m.setProperty("data2", 0, nullptr);
  m.setProperty("transposeMode", "Global", nullptr);
  m.setProperty("transposeModify", 4, nullptr); // Set
  m.setProperty("transposeSemitones", -5, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->globalGrids[0];
  ASSERT_TRUE((*grid)[55].isActive);
  const auto &action = (*grid)[55].action;
  EXPECT_EQ(action.type, ActionType::Command);
  EXPECT_EQ(action.data1, static_cast<int>(MIDIQy::CommandID::Transpose));
  EXPECT_EQ(action.transposeModify, 4);
  EXPECT_EQ(action.transposeSemitones, -5);
}

// --- Panic command: data2 (panic mode) compiled ---
TEST_F(GridCompilerTest, PanicCommandCompilesData2) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 56, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Command", nullptr);
  m.setProperty("data1", (int)MIDIQy::CommandID::Panic, nullptr);
  m.setProperty("data2", 2, nullptr); // Panic latched only
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->globalGrids[0];
  ASSERT_TRUE((*grid)[56].isActive);
  const auto &action = (*grid)[56].action;
  EXPECT_EQ(action.type, ActionType::Command);
  EXPECT_EQ(action.data1, static_cast<int>(MIDIQy::CommandID::Panic));
  EXPECT_EQ(action.data2, 2);
}

// --- Latch Toggle: releaseLatchedOnLatchToggleOff compiled ---
TEST_F(GridCompilerTest, LatchToggleReleaseLatchedCompiled) {
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 57, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Command", nullptr);
  m.setProperty("data1", (int)MIDIQy::CommandID::LatchToggle, nullptr);
  m.setProperty("data2", 0, nullptr);
  m.setProperty("releaseLatchedOnToggleOff", false, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->globalGrids[0];
  ASSERT_TRUE((*grid)[57].isActive);
  EXPECT_FALSE((*grid)[57].action.releaseLatchedOnLatchToggleOff);
}
