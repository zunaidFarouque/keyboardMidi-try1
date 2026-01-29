#include "../ChordUtilities.h"
#include "../DeviceManager.h"
#include "../GridCompiler.h"
#include "../MappingTypes.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
#include "../Zone.h"
#include "../ZoneManager.h"
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
    // We use the hex string format that DeviceManager expects
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)deviceH).toUpperCase(),
                  nullptr);
    m.setProperty("type", (type == ActionType::Note ? "Note" : "CC"), nullptr);
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
  addMapping(0, 81, 0);                         // Global maps Q to Note
  addMapping(0, 81, aliasHash, ActionType::CC); // Device maps Q to CC

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
  addMapping(0, 0x10, 0, ActionType::CC);   // Generic -> CC
  addMapping(0, 0xA0, 0, ActionType::Note); // LShift -> Note

  auto context =
      GridCompiler::compile(presetMgr, deviceMgr, zoneMgr, settingsMgr);
  auto grid = context->visualLookup[0][0];

  // LShift: specific Note (default data1=60 -> "C4")
  EXPECT_EQ((*grid)[0xA0].label, "C4");
  // RShift: inherited generic CC (default data1=60)
  EXPECT_EQ((*grid)[0xA1].label, "CC 60");
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
