#include "../ChordUtilities.h"
#include "../DeviceManager.h"
#include "../InputProcessor.h"
#include "../MappingTypes.h"
#include "../MidiEngine.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
#include "../VoiceManager.h"
#include "../Zone.h"
#include <gtest/gtest.h>
#include <set>

// Records MIDI note on/off for release behaviour and Note type tests
class MockMidiEngine : public MidiEngine {
public:
  struct Event {
    int channel;
    int note;
    float velocity; // 0.0-1.0 for note-on
    bool isNoteOn;
  };
  std::vector<Event> events;

  void sendNoteOn(int channel, int note, float velocity) override {
    events.push_back({channel, note, velocity, true});
  }
  void sendNoteOff(int channel, int note) override {
    events.push_back({channel, note, 0.0f, false});
  }
  void clear() { events.clear(); }
};

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

// Phase 53.2: Layer Toggle - press toggles layer on/off, persistent (no hold)
TEST_F(InputProcessorTest, LayerToggleSwitching) {
  int keyToggle = 10;
  int keyNote = 20;

  // Layer 0: Key 10 -> Layer Toggle 1
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyToggle, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerToggle, nullptr);
    m.setProperty("data2", 1, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Layer 1: Key 20 -> Note 50
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyNote, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", 50, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  proc.forceRebuildMappings();

  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);

  // Press Toggle -> Layer 1 on
  InputID idToggle{0, keyToggle};
  proc.processEvent(idToggle, true);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // Release (no effect for Toggle)
  proc.processEvent(idToggle, false);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // Press Toggle again -> Layer 1 off
  proc.processEvent(idToggle, true);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);

  // Press Toggle again -> Layer 1 on
  proc.processEvent(idToggle, true);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);
}

// Phase 53.2: Momentary ref-count - two keys holding same layer, release one
// keeps layer active
TEST_F(InputProcessorTest, MomentaryRefCountMultipleKeys) {
  int key1 = 10;
  int key2 = 11;
  int keyNote = 20;

  // Layer 0: Key 10 and Key 11 -> Layer Momentary 1 (both)
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    for (int k : {key1, key2}) {
      juce::ValueTree m("Mapping");
      m.setProperty("inputKey", k, nullptr);
      m.setProperty("deviceHash",
                    juce::String::toHexString((juce::int64)0).toUpperCase(),
                    nullptr);
      m.setProperty("type", "Command", nullptr);
      m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
      m.setProperty("data2", 1, nullptr);
      m.setProperty("layerID", 0, nullptr);
      mappings.addChild(m, -1, nullptr);
    }
  }

  // Layer 1: Key 20 -> Note 60
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

  // Hold Key1 -> Layer 1 active
  proc.processEvent(InputID{0, key1}, true);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // Hold Key2 (both held) -> Layer 1 still active
  proc.processEvent(InputID{0, key2}, true);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // Release Key1 -> Layer 1 still active (Key2 still held)
  proc.processEvent(InputID{0, key1}, false);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

  // Release Key2 -> Layer 1 off
  proc.processEvent(InputID{0, key2}, false);
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);
}

// Momentary layer chain: Handover – release A while holding B keeps Layer 2
TEST_F(InputProcessorTest, MomentaryChain_Handover_StaysInLayer2) {
  int keyA = 10; // Layer 0 -> Momentary Layer 1
  int keyB = 11; // Layer 1 -> Momentary Layer 2

  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyA, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 1, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyB, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 2, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, keyA}, true);  // Hold A -> Layer 1
  proc.processEvent(InputID{0, keyB}, true);  // Hold B -> Layer 2
  proc.processEvent(InputID{0, keyA}, false); // Release A while B held

  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 2)
      << "Handover: Layer 2 should stay active when A is released (B held)";
}

// Momentary layer chain: Free Fall – release B after A drops to Layer 0
TEST_F(InputProcessorTest, MomentaryChain_FreeFall_DropsToLayer0) {
  int keyA = 10;
  int keyB = 11;
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyA, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 1, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyB, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 2, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, keyA}, true);  // Hold A -> Layer 1
  proc.processEvent(InputID{0, keyB}, true);  // Hold B -> Layer 2
  proc.processEvent(InputID{0, keyA}, false); // Release A
  proc.processEvent(InputID{0, keyB}, false); // Release B

  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0)
      << "Free Fall: Releasing B should drop to Layer 0 (not Layer 1)";
}

// Phase 54.2 / 53.9: Studio Mode OFF - device-specific mappings ignored
// (effectiveDevice forced to 0, so only global grids are consulted)
TEST_F(InputProcessorTest, StudioModeOffIgnoresDeviceMappings) {
  // Studio Mode OFF (default)
  settingsMgr.setStudioMode(false);

  uintptr_t devHash = 0x12345;
  deviceMgr.createAlias("TestDevice");
  deviceMgr.assignHardware("TestDevice", devHash);
  uintptr_t aliasHash =
      static_cast<uintptr_t>(std::hash<juce::String>{}("TestDevice"));

  int keyLayer = 10;
  int keyNote = 20;

  // Device-specific: Layer 0 Key 10 -> Momentary Layer 1
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

  // Layer 1: Key 20 -> Note 60 (device-specific)
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyNote, nullptr);
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

  proc.forceRebuildMappings();

  // Send event with device handle - Studio Mode OFF forces effectiveDevice=0
  // Global grids have no device-specific mapping, so Layer command not found
  InputID idLayer{devHash, keyLayer};
  proc.processEvent(idLayer, true);

  // Layer 1 should NOT activate (device mapping ignored)
  EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);

  // Note key on device should also not find mapping (device-specific)
  auto actionOpt = proc.getMappingForInput(InputID{devHash, keyNote});
  EXPECT_FALSE(actionOpt.has_value())
      << "Device-specific note should not be found when Studio Mode is OFF";
}

// Fixture for release behaviour tests (uses MockMidiEngine)
class ReleaseBehaviorTest : public ::testing::Test {
protected:
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  ScaleLibrary scaleLib;
  SettingsManager settingsMgr;
  MockMidiEngine mockMidi;
  VoiceManager voiceMgr{mockMidi, settingsMgr};
  InputProcessor proc{voiceMgr, presetMgr, deviceMgr,
                      scaleLib, mockMidi,  settingsMgr};

  void SetUp() override {
    presetMgr.getLayersList().removeAllChildren(nullptr);
    presetMgr.ensureStaticLayers();
    settingsMgr.setMidiModeActive(true);
    proc.initialize();
    mockMidi.clear();
  }

  void addNoteMapping(int keyCode, int note, juce::String releaseBehavior) {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("channel", 1, nullptr);
    m.setProperty("data1", note, nullptr);
    m.setProperty("data2", 127, nullptr);
    m.setProperty("releaseBehavior", releaseBehavior, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
};

TEST_F(ReleaseBehaviorTest, SendNoteOff_PressRelease_SendsNoteOnThenNoteOff) {
  addNoteMapping(20, 60, "Send Note Off");
  proc.forceRebuildMappings();

  InputID id{0, 20};

  proc.processEvent(id, true); // Press
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_TRUE(mockMidi.events[0].isNoteOn);
  EXPECT_EQ(mockMidi.events[0].channel, 1);
  EXPECT_EQ(mockMidi.events[0].note, 60);

  proc.processEvent(id, false); // Release
  ASSERT_EQ(mockMidi.events.size(), 2u);
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].channel, 1);
  EXPECT_EQ(mockMidi.events[1].note, 60);
}

TEST_F(ReleaseBehaviorTest, Nothing_PressRelease_NoNoteOffOnRelease) {
  addNoteMapping(20, 60, "Nothing");
  proc.forceRebuildMappings();

  InputID id{0, 20};

  proc.processEvent(id, true); // Press
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_TRUE(mockMidi.events[0].isNoteOn);

  proc.processEvent(id, false); // Release - nothing should happen
  EXPECT_EQ(mockMidi.events.size(), 1u) << "No note off should be sent";
}

TEST_F(ReleaseBehaviorTest,
       AlwaysLatch_PressReleasePressRelease_UnlatchesOnSecondPress) {
  addNoteMapping(20, 60, "Always Latch");
  proc.forceRebuildMappings();

  InputID id{0, 20};

  proc.processEvent(id, true); // First press = note on
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_TRUE(mockMidi.events[0].isNoteOn);

  proc.processEvent(id,
                    false); // First release = nothing (voice becomes Latched)
  EXPECT_EQ(mockMidi.events.size(), 1u) << "No note off on first release";

  proc.processEvent(id, true); // Second press = note off (unlatch)
  ASSERT_EQ(mockMidi.events.size(), 2u);
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].note, 60);

  proc.processEvent(id, false); // Second release = nothing
  EXPECT_EQ(mockMidi.events.size(), 2u) << "No extra events on second release";
}

// Fixture for full Note type tests (channel, note, velocity, followTranspose,
// etc.)
class NoteTypeTest : public ::testing::Test {
protected:
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  ScaleLibrary scaleLib;
  SettingsManager settingsMgr;
  MockMidiEngine mockMidi;
  VoiceManager voiceMgr{mockMidi, settingsMgr};
  InputProcessor proc{voiceMgr, presetMgr, deviceMgr,
                      scaleLib, mockMidi,  settingsMgr};

  void SetUp() override {
    presetMgr.getLayersList().removeAllChildren(nullptr);
    presetMgr.ensureStaticLayers();
    settingsMgr.setMidiModeActive(true);
    proc.initialize();
    mockMidi.clear();
  }

  void addNoteMapping(int keyCode, int channel, int note, int velocity,
                      juce::String releaseBehavior = "Send Note Off",
                      bool followTranspose = false, int velRandom = 0) {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("channel", channel, nullptr);
    m.setProperty("data1", note, nullptr);
    m.setProperty("data2", velocity, nullptr);
    m.setProperty("velRandom", velRandom, nullptr);
    m.setProperty("releaseBehavior", releaseBehavior, nullptr);
    m.setProperty("followTranspose", followTranspose, nullptr);
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
};

TEST_F(NoteTypeTest, ChannelAndNoteNumberSentCorrectly) {
  addNoteMapping(30, 5, 72, 100); // Key 30 -> Ch5, G4 (72), vel 100
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 30}, true);
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_EQ(mockMidi.events[0].channel, 5);
  EXPECT_EQ(mockMidi.events[0].note, 72);
  EXPECT_TRUE(mockMidi.events[0].isNoteOn);
}

TEST_F(NoteTypeTest, VelocitySentCorrectly) {
  addNoteMapping(31, 1, 60, 64, "Send Note Off", false, 0); // vel 64, no random
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 31}, true);
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_FLOAT_EQ(mockMidi.events[0].velocity, 64.0f / 127.0f);
}

TEST_F(NoteTypeTest, FollowTransposeAddsToNoteWhenEnabled) {
  proc.getZoneManager().setGlobalTranspose(2, 0);        // +2 semitones
  addNoteMapping(32, 1, 60, 127, "Send Note Off", true); // C4 + 2 = D4 (62)
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 32}, true);
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_EQ(mockMidi.events[0].note, 62);
}

TEST_F(NoteTypeTest, FollowTransposeIgnoredWhenDisabled) {
  proc.getZoneManager().setGlobalTranspose(2, 0);         // +2 semitones
  addNoteMapping(33, 1, 60, 127, "Send Note Off", false); // C4, no transpose
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 33}, true);
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_EQ(mockMidi.events[0].note, 60);
}

// Play mode Direct: chord notes must be sent immediately (strum 0, no timing).
TEST_F(NoteTypeTest, DirectMode_ChordNotesSentImmediately) {
  auto zone = std::make_shared<Zone>();
  zone->name = "Direct Triad";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->playMode = Zone::PlayMode::Direct;
  zone->midiChannel = 1;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  proc.processEvent(InputID{0, 81}, true);

  ASSERT_EQ(mockMidi.events.size(), 3u)
      << "Direct mode must send all chord notes at once (triad=3)";
  for (size_t i = 0; i < mockMidi.events.size(); ++i) {
    EXPECT_TRUE(mockMidi.events[i].isNoteOn)
        << "event " << i << " should be note-on";
    EXPECT_EQ(mockMidi.events[i].channel, 1);
  }
}

// Release mode Sustain: one-shot latch – no note-off on release; next chord
// sends note-off then note-on.
TEST_F(NoteTypeTest,
       SustainMode_ReleaseSendsNoNoteOff_NextChordSendsNoteOffThenNoteOn) {
  auto zone = std::make_shared<Zone>();
  zone->name = "Sustain Triad";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81, 70}; // Q and F
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->playMode = Zone::PlayMode::Direct;
  zone->releaseBehavior = Zone::ReleaseBehavior::Sustain;
  zone->midiChannel = 1;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  proc.processEvent(InputID{0, 81}, true); // Press Q -> C E G
  ASSERT_EQ(mockMidi.events.size(), 3u);
  for (size_t i = 0; i < 3u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn);

  proc.processEvent(InputID{0, 81},
                    false); // Release Q -> no note-off (Sustain)
  EXPECT_EQ(mockMidi.events.size(), 3u)
      << "Sustain: release must not send note-off";

  proc.processEvent(InputID{0, 70},
                    true); // Press F -> note-off for Q's chord, then F's chord
  ASSERT_EQ(mockMidi.events.size(), 9u)
      << "Sustain: 3 on (Q) + 3 off (Q) + 3 on (F)";
  size_t offCount = 0, onCount = 0;
  for (const auto &e : mockMidi.events) {
    if (e.isNoteOn)
      ++onCount;
    else
      ++offCount;
  }
  EXPECT_EQ(offCount, 3u) << "Previous chord's 3 notes must be turned off";
  EXPECT_EQ(onCount, 6u) << "Two chords: 3 note-ons (Q) + 3 note-ons (F)";
}

// Override timer: when enabled, new chord cancels old chord's timer immediately
TEST_F(NoteTypeTest, OverrideTimer_NewChordCancelsOldTimer_OnlyOneTimerAlive) {
  auto zone = std::make_shared<Zone>();
  zone->name = "Override Triad";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81, 70}; // Q and F
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->playMode = Zone::PlayMode::Direct;
  zone->releaseBehavior = Zone::ReleaseBehavior::Normal;
  zone->delayReleaseOn = true;
  zone->releaseDurationMs = 1000; // 1 second delay
  zone->overrideTimer = true;     // Override enabled
  zone->midiChannel = 1;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  // Press Q -> C E G (note-on)
  proc.processEvent(InputID{0, 81}, true);
  ASSERT_EQ(mockMidi.events.size(), 3u);
  for (size_t i = 0; i < 3u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn);

  // Release Q -> starts 1s timer (no immediate note-off)
  proc.processEvent(InputID{0, 81}, false);
  EXPECT_EQ(mockMidi.events.size(), 3u)
      << "Delayed release: no immediate note-off";

  // Press F immediately -> should cancel Q's timer and send note-off for Q,
  // then note-on for F
  proc.processEvent(InputID{0, 70}, true);
  ASSERT_EQ(mockMidi.events.size(), 9u)
      << "Override: 3 on (Q) + 3 off (Q, cancelled) + 3 on (F)";

  // Verify sequence: first 3 are Q on, next 3 are Q off, last 3 are F on
  for (size_t i = 0; i < 3u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn) << "Q chord note-on at " << i;
  for (size_t i = 3; i < 6u; ++i)
    EXPECT_FALSE(mockMidi.events[i].isNoteOn)
        << "Q chord note-off (cancelled) at " << i;
  for (size_t i = 6; i < 9u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn) << "F chord note-on at " << i;
}

// Override timer disabled: old timer still fires even if new chord plays
TEST_F(NoteTypeTest, OverrideTimerOff_OldTimerStillFires_TwoTimersAlive) {
  auto zone = std::make_shared<Zone>();
  zone->name = "No Override Triad";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81, 70}; // Q and F
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->playMode = Zone::PlayMode::Direct;
  zone->releaseBehavior = Zone::ReleaseBehavior::Normal;
  zone->delayReleaseOn = true;
  zone->releaseDurationMs = 50; // 50ms delay (short for testing)
  zone->overrideTimer = false;  // Override disabled
  zone->midiChannel = 1;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  // Press Q -> C E G (note-on)
  proc.processEvent(InputID{0, 81}, true);
  ASSERT_EQ(mockMidi.events.size(), 3u);
  for (size_t i = 0; i < 3u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn);

  // Release Q -> starts 50ms timer (no immediate note-off)
  proc.processEvent(InputID{0, 81}, false);
  EXPECT_EQ(mockMidi.events.size(), 3u)
      << "Delayed release: no immediate note-off";

  // Press F immediately -> note-on for F (Q's timer still alive)
  proc.processEvent(InputID{0, 70}, true);
  ASSERT_EQ(mockMidi.events.size(), 6u)
      << "No override: 3 on (Q) + 3 on (F), Q timer still pending";

  // Verify sequence: first 3 are Q on, last 3 are F on
  for (size_t i = 0; i < 3u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn) << "Q chord note-on at " << i;
  for (size_t i = 3; i < 6u; ++i)
    EXPECT_TRUE(mockMidi.events[i].isNoteOn) << "F chord note-on at " << i;
  // Note: Q's timer will fire after 50ms in real execution, but we can't
  // easily test that here without timer advancement. This test verifies that
  // F's note-on doesn't cancel Q's timer.
}

TEST_F(NoteTypeTest, AllParamsWorkTogether) {
  proc.getZoneManager().setGlobalTranspose(1, 0);          // +1 semitone
  addNoteMapping(34, 8, 83, 90, "Send Note Off", true, 0); // B4 + 1 = C5 (84)
  proc.forceRebuildMappings();

  InputID id{0, 34};
  proc.processEvent(id, true);
  ASSERT_EQ(mockMidi.events.size(), 1u);
  EXPECT_EQ(mockMidi.events[0].channel, 8);
  EXPECT_EQ(mockMidi.events[0].note, 84); // 83 + 1
  EXPECT_NEAR(mockMidi.events[0].velocity, 90.0f / 127.0f, 0.001f);

  proc.processEvent(id, false); // Release sends note off
  ASSERT_EQ(mockMidi.events.size(), 2u);
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].channel, 8);
  EXPECT_EQ(mockMidi.events[1].note, 84);
}

// Momentary layer chain: Phantom Key – B release must not trigger Note(C3)
TEST_F(NoteTypeTest, MomentaryChain_PhantomKey_ReleaseDoesNotTriggerNote) {
  // Layer 0: Key 10 = Momentary(L1), Key 11 = Note(C3)
  // Layer 1: Key 11 = Momentary(L2)
  const int keyA = 10;
  const int keyB = 11;
  const int noteC3 = 48; // MIDI note 48 = C3

  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m1("Mapping");
    m1.setProperty("inputKey", keyA, nullptr);
    m1.setProperty("deviceHash",
                   juce::String::toHexString((juce::int64)0).toUpperCase(),
                   nullptr);
    m1.setProperty("type", "Command", nullptr);
    m1.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m1.setProperty("data2", 1, nullptr);
    m1.setProperty("layerID", 0, nullptr);
    mappings.addChild(m1, -1, nullptr);

    juce::ValueTree m2("Mapping");
    m2.setProperty("inputKey", keyB, nullptr);
    m2.setProperty("deviceHash",
                   juce::String::toHexString((juce::int64)0).toUpperCase(),
                   nullptr);
    m2.setProperty("type", "Note", nullptr);
    m2.setProperty("channel", 1, nullptr);
    m2.setProperty("data1", noteC3, nullptr);
    m2.setProperty("data2", 127, nullptr);
    m2.setProperty("layerID", 0, nullptr);
    mappings.addChild(m2, -1, nullptr);
  }
  {
    auto mappings = presetMgr.getMappingsListForLayer(1);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyB, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
    m.setProperty("data2", 2, nullptr);
    m.setProperty("layerID", 1, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  proc.processEvent(InputID{0, keyA}, true); // Hold A -> Layer 1
  proc.processEvent(InputID{0, keyB},
                    true); // Hold B -> Layer 2 (B = Momentary on L1)
  proc.processEvent(InputID{0, keyA}, false); // Release A (handover)
  proc.processEvent(InputID{0, keyB},
                    false); // Release B – must NOT trigger Note C3

  EXPECT_EQ(mockMidi.events.size(), 0u)
      << "Phantom Key: B release must not trigger Note C3 on Layer 0";
}

// Sustain Toggle: when turned off, send one NoteOff per unique note, not per
// voice
TEST_F(NoteTypeTest, SustainToggleOffSendsOneNoteOffPerUniqueNote) {
  // Sustain Toggle on key 40, Note C4 (60) on key 20, Note D4 (62) on key 21
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    auto addMapping = [&mappings](int key, juce::String type, int d1, int d2) {
      juce::ValueTree m("Mapping");
      m.setProperty("inputKey", key, nullptr);
      m.setProperty("deviceHash",
                    juce::String::toHexString((juce::int64)0).toUpperCase(),
                    nullptr);
      m.setProperty("type", type, nullptr);
      m.setProperty("data1", d1, nullptr);
      m.setProperty("data2", d2, nullptr);
      m.setProperty("layerID", 0, nullptr);
      mappings.addChild(m, -1, nullptr);
    };
    addMapping(40, "Command", 1, 0); // SustainToggle
    addMapping(20, "Note", 60, 127); // C4
    addMapping(21, "Note", 62, 127); // D4
  }
  proc.forceRebuildMappings();

  InputID sustainKey{0, 40}, keyQ{0, 20}, keyW{0, 21};

  proc.processEvent(sustainKey, true); // Sustain ON
  proc.processEvent(sustainKey, false);

  // Q => C4 x4, W => D4 x2 (press+release each time; sustain holds notes)
  for (int i = 0; i < 4; ++i) {
    proc.processEvent(keyQ, true);
    proc.processEvent(keyQ, false);
  }
  for (int i = 0; i < 2; ++i) {
    proc.processEvent(keyW, true);
    proc.processEvent(keyW, false);
  }

  // 6 note-ons, 0 note-offs (sustain holds)
  int noteOnCount = 0;
  for (const auto &e : mockMidi.events)
    if (e.isNoteOn)
      ++noteOnCount;
  EXPECT_EQ(noteOnCount, 6);

  proc.processEvent(sustainKey, true); // Sustain OFF
  proc.processEvent(sustainKey, false);

  // Must send exactly 2 note-offs: one for C4, one for D4
  int noteOffCount = 0;
  std::set<int> noteOffs;
  for (const auto &e : mockMidi.events) {
    if (!e.isNoteOn) {
      ++noteOffCount;
      noteOffs.insert(e.note);
    }
  }
  EXPECT_EQ(noteOffCount, 2) << "Expected one NoteOff per unique note (C4, D4)";
  EXPECT_EQ(noteOffs.size(), 2u);
  EXPECT_TRUE(noteOffs.count(60));
  EXPECT_TRUE(noteOffs.count(62));
}

// Sustain Inverse: default sustain ON; switching to non-Inverse sets sustain
// OFF
TEST_F(NoteTypeTest, SustainInverseDefaultAndConfigChangeCleanup) {
  // Map key 40 to Sustain Inverse (data1=2)
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", 40, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", 2, nullptr); // SustainInverse
    m.setProperty("layerID", 0, nullptr);
    mappings.addChild(m, -1, nullptr);
  }
  proc.forceRebuildMappings(); // calls applySustainDefaultFromPreset
  EXPECT_TRUE(voiceMgr.isSustainActive())
      << "With Sustain Inverse mapped, default sustain should be ON";

  // Change to Sustain Toggle (data1=1) - simulates configurator change
  presetMgr.getMappingsListForLayer(0).getChild(0).setProperty("data1", 1,
                                                               nullptr);
  // valueTreePropertyChanged fires and calls applySustainDefaultFromPreset
  proc.forceRebuildMappings();
  EXPECT_FALSE(voiceMgr.isSustainActive())
      << "With no Sustain Inverse, sustain should be OFF after cleanup";
}

// Latch Toggle with releaseLatchedOnToggleOff: when toggling off, sends NoteOff
// for latched notes
TEST_F(NoteTypeTest, LatchToggleReleaseLatchedOnToggleOff_SendsNoteOff) {
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree latch("Mapping");
    latch.setProperty("inputKey", 40, nullptr);
    latch.setProperty("deviceHash",
                      juce::String::toHexString((juce::int64)0).toUpperCase(),
                      nullptr);
    latch.setProperty("type", "Command", nullptr);
    latch.setProperty("data1", 3, nullptr); // LatchToggle
    latch.setProperty("releaseLatchedOnToggleOff", true, nullptr);
    latch.setProperty("layerID", 0, nullptr);
    mappings.addChild(latch, -1, nullptr);

    juce::ValueTree note("Mapping");
    note.setProperty("inputKey", 20, nullptr);
    note.setProperty("deviceHash",
                     juce::String::toHexString((juce::int64)0).toUpperCase(),
                     nullptr);
    note.setProperty("type", "Note", nullptr);
    note.setProperty("channel", 1, nullptr);
    note.setProperty("data1", 60, nullptr);
    note.setProperty("data2", 127, nullptr);
    note.setProperty("layerID", 0, nullptr);
    mappings.addChild(note, -1, nullptr);
  }
  proc.forceRebuildMappings();

  InputID latchKey{0, 40}, noteKey{0, 20};

  proc.processEvent(latchKey, true); // Latch ON
  proc.processEvent(latchKey, false);

  proc.processEvent(noteKey, true);  // Note on C4
  proc.processEvent(noteKey, false); // Release - note latched (no NoteOff)
  ASSERT_EQ(mockMidi.events.size(), 1u) << "Only note-on so far";

  proc.processEvent(latchKey,
                    true); // Latch OFF (with releaseLatchedOnToggleOff)
  proc.processEvent(latchKey, false);

  ASSERT_EQ(mockMidi.events.size(), 2u)
      << "NoteOff should be sent when latch toggled off";
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].note, 60);
  EXPECT_EQ(mockMidi.events[1].channel, 1);
}

// Panic with dropdown: "Panic all" vs "Panic latched only"
TEST_F(NoteTypeTest, PanicAll_SendsNoteOffForAllNotes) {
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree panic("Mapping");
    panic.setProperty("inputKey", 40, nullptr);
    panic.setProperty("deviceHash",
                      juce::String::toHexString((juce::int64)0).toUpperCase(),
                      nullptr);
    panic.setProperty("type", "Command", nullptr);
    panic.setProperty("data1", 4, nullptr); // Panic
    panic.setProperty("data2", 0, nullptr); // Panic all
    panic.setProperty("layerID", 0, nullptr);
    mappings.addChild(panic, -1, nullptr);

    juce::ValueTree note("Mapping");
    note.setProperty("inputKey", 20, nullptr);
    note.setProperty("deviceHash",
                     juce::String::toHexString((juce::int64)0).toUpperCase(),
                     nullptr);
    note.setProperty("type", "Note", nullptr);
    note.setProperty("channel", 1, nullptr);
    note.setProperty("data1", 60, nullptr);
    note.setProperty("data2", 127, nullptr);
    note.setProperty("layerID", 0, nullptr);
    mappings.addChild(note, -1, nullptr);
  }
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 20}, true); // Note on
  proc.processEvent(InputID{0, 20},
                    false);              // Release - note off (Send Note Off)
  ASSERT_GE(mockMidi.events.size(), 2u); // NoteOn + NoteOff
  mockMidi.clear();

  proc.processEvent(InputID{0, 20}, true); // Note on (playing)
  proc.processEvent(InputID{0, 40}, true); // Panic all
  proc.processEvent(InputID{0, 40}, false);
  ASSERT_EQ(mockMidi.events.size(), 2u) << "NoteOn + NoteOff from panic";
  EXPECT_TRUE(mockMidi.events[0].isNoteOn);
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].note, 60);
}

TEST_F(NoteTypeTest, PanicLatchedOnly_SendsNoteOffOnlyForLatched) {
  voiceMgr.setLatch(true);
  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree panic("Mapping");
    panic.setProperty("inputKey", 40, nullptr);
    panic.setProperty("deviceHash",
                      juce::String::toHexString((juce::int64)0).toUpperCase(),
                      nullptr);
    panic.setProperty("type", "Command", nullptr);
    panic.setProperty("data1", 4, nullptr); // Panic
    panic.setProperty("data2", 1, nullptr); // Panic latched only
    panic.setProperty("layerID", 0, nullptr);
    mappings.addChild(panic, -1, nullptr);

    juce::ValueTree note("Mapping");
    note.setProperty("inputKey", 20, nullptr);
    note.setProperty("deviceHash",
                     juce::String::toHexString((juce::int64)0).toUpperCase(),
                     nullptr);
    note.setProperty("type", "Note", nullptr);
    note.setProperty("channel", 1, nullptr);
    note.setProperty("data1", 60, nullptr);
    note.setProperty("data2", 127, nullptr);
    note.setProperty("layerID", 0, nullptr);
    mappings.addChild(note, -1, nullptr);
  }
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 20}, true);  // Note on
  proc.processEvent(InputID{0, 20}, false); // Release - latched (no NoteOff)
  ASSERT_EQ(mockMidi.events.size(), 1u) << "NoteOn only, note is latched";

  proc.processEvent(InputID{0, 40}, true); // Panic latched only
  proc.processEvent(InputID{0, 40}, false);
  ASSERT_EQ(mockMidi.events.size(), 2u) << "NoteOff from panic latched";
  EXPECT_FALSE(mockMidi.events[1].isNoteOn);
  EXPECT_EQ(mockMidi.events[1].note, 60);
}

// Panic chords: turns off sustain-held chord (Sustain release mode)
TEST_F(NoteTypeTest, PanicChords_SendsNoteOffForSustainChord) {
  auto zone = std::make_shared<Zone>();
  zone->name = "Sustain Triad";
  zone->layerID = 0;
  zone->targetAliasHash = 0;
  zone->inputKeyCodes = {81};
  zone->chordType = ChordUtilities::ChordType::Triad;
  zone->scaleName = "Major";
  zone->rootNote = 60;
  zone->playMode = Zone::PlayMode::Direct;
  zone->releaseBehavior = Zone::ReleaseBehavior::Sustain;
  zone->midiChannel = 1;
  proc.getZoneManager().addZone(zone);

  {
    auto mappings = presetMgr.getMappingsListForLayer(0);
    juce::ValueTree panic("Mapping");
    panic.setProperty("inputKey", 40, nullptr);
    panic.setProperty("deviceHash",
                      juce::String::toHexString((juce::int64)0).toUpperCase(),
                      nullptr);
    panic.setProperty("type", "Command", nullptr);
    panic.setProperty("data1", 4, nullptr); // Panic
    panic.setProperty("data2", 2, nullptr); // Panic chords
    panic.setProperty("layerID", 0, nullptr);
    mappings.addChild(panic, -1, nullptr);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  proc.processEvent(InputID{0, 81}, true); // Press Q -> C E G (3 note-ons)
  proc.processEvent(InputID{0, 81},
                    false); // Release Q -> no note-off (Sustain)
  ASSERT_EQ(mockMidi.events.size(), 3u) << "Sustain: 3 note-ons only";

  proc.processEvent(InputID{0, 40}, true); // Panic chords
  proc.processEvent(InputID{0, 40}, false);
  ASSERT_EQ(mockMidi.events.size(), 6u)
      << "3 note-ons + 3 note-offs from Panic chords";
  EXPECT_FALSE(mockMidi.events[3].isNoteOn);
  EXPECT_FALSE(mockMidi.events[4].isNoteOn);
  EXPECT_FALSE(mockMidi.events[5].isNoteOn);
}

// Transpose command: up1, down1, up12, down12, set; zone selector is
// placeholder
void addTransposeMapping(juce::ValueTree &mappings, int inputKey,
                         int transposeModify, int transposeSemitones = 0) {
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", inputKey, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Command", nullptr);
  m.setProperty("data1", static_cast<int>(OmniKey::CommandID::Transpose),
                nullptr);
  m.setProperty("transposeModify", transposeModify, nullptr);
  m.setProperty("transposeSemitones", transposeSemitones, nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);
}

TEST_F(NoteTypeTest, TransposeUp1Semitone_IncreasesChromatic) {
  proc.getZoneManager().setGlobalTranspose(0, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 0); // modify 0 = up 1 semitone
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 1);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeDown1Semitone_DecreasesChromatic) {
  proc.getZoneManager().setGlobalTranspose(2, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 1); // modify 1 = down 1 semitone
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 1);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeUp1Octave_IncreasesBy12) {
  proc.getZoneManager().setGlobalTranspose(0, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 2); // modify 2 = up 1 octave
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 12);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeDown1Octave_DecreasesBy12) {
  proc.getZoneManager().setGlobalTranspose(12, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 3); // modify 3 = down 1 octave
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 0);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeSet_AppliesSemitonesValue) {
  proc.getZoneManager().setGlobalTranspose(0, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 4, 5); // modify 4 = set, semitones 5
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 5);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeSet_NegativeSemitones) {
  proc.getZoneManager().setGlobalTranspose(0, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 4, -7); // set to -7 semitones
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), -7);
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}

TEST_F(NoteTypeTest, TransposeClampedTo48) {
  proc.getZoneManager().setGlobalTranspose(45, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  addTransposeMapping(mappings, 40, 0); // up 1
  proc.forceRebuildMappings();

  for (int i = 0; i < 10; ++i) {
    proc.processEvent(InputID{0, 40}, true);
    proc.processEvent(InputID{0, 40}, false);
  }
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 48)
      << "Chromatic transpose should be clamped to 48";
}

TEST_F(NoteTypeTest, LegacyGlobalPitchDown_DecreasesChromaticByOne) {
  proc.getZoneManager().setGlobalTranspose(3, 0);
  auto mappings = presetMgr.getMappingsListForLayer(0);
  juce::ValueTree m("Mapping");
  m.setProperty("inputKey", 40, nullptr);
  m.setProperty("deviceHash",
                juce::String::toHexString((juce::int64)0).toUpperCase(),
                nullptr);
  m.setProperty("type", "Command", nullptr);
  m.setProperty("data1", static_cast<int>(OmniKey::CommandID::GlobalPitchDown),
                nullptr);
  m.setProperty("layerID", 0, nullptr);
  mappings.addChild(m, -1, nullptr);
  proc.forceRebuildMappings();

  proc.processEvent(InputID{0, 40}, true);
  proc.processEvent(InputID{0, 40}, false);
  EXPECT_EQ(proc.getZoneManager().getGlobalChromaticTranspose(), 2)
      << "Legacy GlobalPitchDown should act as down 1 semitone";
  EXPECT_EQ(proc.getZoneManager().getGlobalDegreeTranspose(), 0);
}
