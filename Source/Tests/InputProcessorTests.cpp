#include "../DeviceManager.h"
#include "../InputProcessor.h"
#include "../MappingTypes.h"
#include "../MidiEngine.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
#include "../VoiceManager.h"
#include <gtest/gtest.h>

class InputProcessorTest : public ::testing::Test {
protected:
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  ScaleLibrary scaleLib;
  SettingsManager settingsMgr;
  MidiEngine midiEng;
  VoiceManager voiceMgr{midiEng, settingsMgr};
  InputProcessor proc{voiceMgr, presetMgr, deviceMgr,
                      scaleLib, midiEng,   settingsMgr};

  void SetUp() override {
    presetMgr.getLayersList().removeAllChildren(nullptr);
    presetMgr.ensureStaticLayers();
    settingsMgr.setMidiModeActive(true);
    proc.initialize();
  }
};

TEST_F(InputProcessorTest, LayerMomentarySwitching) {
  // 1. Setup: Map Key 10 (Enter) on Layer 0 to "Layer Momentary 1"
  //    (Command ID 10 = LayerMomentary, Data2 = 1)
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", 10, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", 10, nullptr); // CommandID::LayerMomentary
    m.setProperty("data2", 1, nullptr);  // Target Layer 1
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // 2. Setup: Map Key 20 (Q) on Layer 1 to "Note 50"
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", 20, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 50, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // 3. Compile (public API; rebuildGrid is private)
  proc.forceRebuildMappings();

  // 4. Initial State Check
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);

  // 5. Act: Press Layer Button (Key 10)
  InputID layerBtn{0, 10};           // Global
  proc.processEvent(layerBtn, true); // Down

  // 6. Assert: Layer 1 should be active
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // 7. Act: Release Layer Button
  proc.processEvent(layerBtn, false); // Up

  // 8. Assert: Back to Layer 0
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);
}

// Phase 53.8: Hold Layer key and play a note on that layer (real-world
// scenario)
TEST_F(InputProcessorTest, HoldLayerAndPlayNote) {
  // --- ARRANGE ---
  int keyLayer = 10; // Key used for Momentary Layer 1
  int keyNote = 20;  // Key used for Note (e.g. S)

  // 1. Layer 0: Key A -> Momentary Layer 1
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyLayer, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 1, nullptr); // Target Layer 1
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // 2. Layer 1: Key S -> Note 60 (C4)
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyNote, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 60, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  proc.forceRebuildMappings();

  // --- ACT 1: Hold Layer Key ---
  InputID idLayer{0, keyLayer};
  proc.processEvent(idLayer, true); // Down

  // Check: Layer 1 should now be active (Command executed, state updated)
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // --- ACT 2: Press Note Key (while layer key is held) ---
  InputID idNote{0, keyNote};
  proc.processEvent(idNote, true); // Down

  // --- ASSERT ---
  // Routing: With Layer 1 active, keyNote should resolve to Note 60.
  // If getMappingForInput returns the note action, routing is correct.
  auto actionOpt = proc.getMappingForInput(idNote);
  ASSERT_TRUE(actionOpt.has_value())
      << "Note key should have mapping on Layer 1";
  EXPECT_EQ(actionOpt->type, ActionType::Note);
  EXPECT_EQ(actionOpt->data1, 60);
}

// Phase 53.9: Device-specific layer switch then play (real-world scenario)
TEST_F(InputProcessorTest, DeviceSpecificLayerSwitching) {
  // Studio Mode must be ON so InputProcessor uses device handle for lookup
  settingsMgr.setStudioMode(true);

  // --- ARRANGE ---
  uintptr_t devHash = 0x12345;
  deviceMgr.createAlias("TestDevice");
  deviceMgr.assignHardware("TestDevice", devHash);

  uintptr_t aliasHash =
      static_cast<uintptr_t>(std::hash<juce::String>{}("TestDevice"));

  int keyLayer = 10;
  int keyNoteLocal = 20;
  int keyNoteGlobal = 30;

  // 1. Layer 0 (Device Specific): Key A -> Momentary Layer 1
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyLayer, nullptr);
    m.setProperty(
        "deviceHash",
        juce::String::toHexString((juce::int64)aliasHash).toUpperCase(),
        nullptr);
    m.setProperty("inputAlias", "TestDevice", nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 1, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // 2. Layer 1 (Device Specific): Key S -> Note 60
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyNoteLocal, nullptr);
    m.setProperty(
        "deviceHash",
        juce::String::toHexString((juce::int64)aliasHash).toUpperCase(),
        nullptr);
    m.setProperty("inputAlias", "TestDevice", nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 60, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // 3. Layer 1 (Global): Key D -> Note 62 (device should inherit)
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyNoteGlobal, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("inputAlias", "", nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 62, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  proc.forceRebuildMappings();

  // --- ACT 1: Hold Layer Key on Device ---
  InputID idLayer{devHash, keyLayer};
  proc.processEvent(idLayer, true);

  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  auto names = proc.getActiveLayerNames();
  EXPECT_TRUE(names.contains("Layer 1 (Hold)"));

  // --- ACT 2: Verify device grid and mappings ---
  auto ctx = proc.getContext();
  ASSERT_TRUE(ctx != nullptr);
  // deviceGrids keyed by hardware ID (GridCompiler stores under both alias hash
  // and hardware IDs)
  ASSERT_TRUE(ctx->deviceGrids.count(devHash) > 0)
      << "Device grids must exist for hardware ID";
  auto gridL1 = ctx->deviceGrids.at(devHash)[1];
  ASSERT_TRUE(gridL1 != nullptr);

  EXPECT_TRUE((*gridL1)[(size_t)keyNoteLocal].isActive);  // Local mapping
  EXPECT_TRUE((*gridL1)[(size_t)keyNoteGlobal].isActive); // Inherited Global
}
