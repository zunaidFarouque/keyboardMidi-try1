#include "../DeviceManager.h"
#include "../GridCompiler.h"
#include "../MappingTypes.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
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
