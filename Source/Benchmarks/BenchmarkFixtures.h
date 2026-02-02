#pragma once

#include "../ChordUtilities.h"
#include "../DeviceManager.h"
#include "../ExpressionEngine.h"
#include "../InputProcessor.h"
#include "../MappingTypes.h"
#include "../MidiEngine.h"
#include "../PresetManager.h"
#include "../ScaleLibrary.h"
#include "../SettingsManager.h"
#include "../VoiceManager.h"
#include "../Zone.h"
#include <benchmark/benchmark.h>
#include <vector>

// MockMidiEngine: Records MIDI events for verification without actual output
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
  size_t eventCount() const { return events.size(); }
};

// Base fixture for MIDI processing benchmarks
class MidiBenchmarkFixture : public benchmark::Fixture {
public:
  PresetManager presetMgr;
  DeviceManager deviceMgr;
  ScaleLibrary scaleLib;
  SettingsManager settingsMgr;
  MockMidiEngine mockMidi;
  VoiceManager voiceMgr{mockMidi, settingsMgr};
  InputProcessor proc{voiceMgr, presetMgr, deviceMgr,
                      scaleLib, mockMidi,  settingsMgr};

  void SetUp(benchmark::State &state) override {
    presetMgr.getLayersList().removeAllChildren(nullptr);
    presetMgr.ensureStaticLayers();
    settingsMgr.setMidiModeActive(true);
    proc.initialize();
  }

  void TearDown(benchmark::State &state) override { mockMidi.clear(); }

  // Helper: Add a Note mapping to a specific layer
  void addNoteMapping(int layer, int keyCode, int midiNote, int velocity = 100,
                      int channel = 1) {
    auto mappings = presetMgr.getMappingsListForLayer(layer);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Note", nullptr);
    m.setProperty("data1", midiNote, nullptr);
    m.setProperty("data2", velocity, nullptr);
    m.setProperty("channel", channel, nullptr);
    m.setProperty("layerID", layer, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper: Add a Command mapping
  void addCommandMapping(int layer, int keyCode, int commandId, int data2 = 0) {
    auto mappings = presetMgr.getMappingsListForLayer(layer);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Command", nullptr);
    m.setProperty("data1", commandId, nullptr);
    m.setProperty("data2", data2, nullptr);
    m.setProperty("layerID", layer, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper: Add an Expression (CC) mapping
  void addExpressionCCMapping(int layer, int keyCode, int ccNumber,
                              int channel = 1, bool useCustomEnvelope = false,
                              int attack = 0, int decay = 0, int sustain = 127,
                              int release = 0) {
    auto mappings = presetMgr.getMappingsListForLayer(layer);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Expression", nullptr);
    m.setProperty("adsrTarget", 0, nullptr); // CC
    m.setProperty("data1", ccNumber, nullptr);
    m.setProperty("data2", 127, nullptr); // Peak value
    m.setProperty("channel", channel, nullptr);
    m.setProperty("useCustomEnvelope", useCustomEnvelope, nullptr);
    m.setProperty("adsrAttack", attack, nullptr);
    m.setProperty("adsrDecay", decay, nullptr);
    m.setProperty("adsrSustain", sustain, nullptr);
    m.setProperty("adsrRelease", release, nullptr);
    m.setProperty("layerID", layer, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper: Add an Expression (PitchBend) mapping
  void addExpressionPBMapping(int layer, int keyCode, int channel = 1,
                              bool useCustomEnvelope = false, int attack = 0,
                              int decay = 0, int sustain = 127,
                              int release = 0) {
    auto mappings = presetMgr.getMappingsListForLayer(layer);
    juce::ValueTree m("Mapping");
    m.setProperty("inputKey", keyCode, nullptr);
    m.setProperty("deviceHash",
                  juce::String::toHexString((juce::int64)0).toUpperCase(),
                  nullptr);
    m.setProperty("type", "Expression", nullptr);
    m.setProperty("adsrTarget", 1, nullptr); // PitchBend
    m.setProperty("data2", 16383, nullptr);  // Max bend
    m.setProperty("channel", channel, nullptr);
    m.setProperty("useCustomEnvelope", useCustomEnvelope, nullptr);
    m.setProperty("adsrAttack", attack, nullptr);
    m.setProperty("adsrDecay", decay, nullptr);
    m.setProperty("adsrSustain", sustain, nullptr);
    m.setProperty("adsrRelease", release, nullptr);
    m.setProperty("layerID", layer, nullptr);
    mappings.addChild(m, -1, nullptr);
  }

  // Helper: Create a zone with specific configuration
  std::shared_ptr<Zone> createZone(
      const juce::String &name, int layer, const std::vector<int> &keyCodes,
      ChordUtilities::ChordType chordType,
      PolyphonyMode polyMode = PolyphonyMode::Poly,
      Zone::ReleaseBehavior releaseBehavior = Zone::ReleaseBehavior::Normal) {
    auto zone = std::make_shared<Zone>();
    zone->name = name;
    zone->layerID = layer;
    zone->targetAliasHash = 0;
    zone->inputKeyCodes = keyCodes;
    zone->chordType = chordType;
    zone->scaleName = "Major";
    zone->rootNote = 60;
    zone->playMode = Zone::PlayMode::Direct;
    zone->polyphonyMode = polyMode;
    zone->releaseBehavior = releaseBehavior;
    zone->midiChannel = 1;
    return zone;
  }

  // Helper: Create zone with piano voicing
  std::shared_ptr<Zone> createPianoZone(const juce::String &name, int layer,
                                        const std::vector<int> &keyCodes,
                                        ChordUtilities::ChordType chordType,
                                        Zone::PianoVoicingStyle voicingStyle,
                                        bool addBass = false,
                                        int magnetSemitones = 0) {
    auto zone = createZone(name, layer, keyCodes, chordType);
    zone->instrumentMode = Zone::InstrumentMode::Piano;
    zone->pianoVoicingStyle = voicingStyle;
    zone->addBassNote = addBass;
    zone->voicingMagnetSemitones = magnetSemitones;
    return zone;
  }
};
