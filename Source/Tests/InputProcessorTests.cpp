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
