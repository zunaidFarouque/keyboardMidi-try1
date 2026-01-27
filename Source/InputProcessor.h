#pragma once
#include "DeviceManager.h"
#include "ExpressionEngine.h"
#include "MappingTypes.h"
#include "PresetManager.h"
#include "VoiceManager.h"
#include "ZoneManager.h"
#include "RhythmAnalyzer.h"
#include <JuceHeader.h>

class ScaleLibrary;
class MidiEngine;
class SettingsManager;

class InputProcessor : public juce::ValueTree::Listener,
                       public juce::ChangeListener {
public:
  InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr,
                 DeviceManager &deviceMgr, ScaleLibrary &scaleLib,
                 MidiEngine &midiEng, SettingsManager &settingsMgr);
  ~InputProcessor() override;

  // The main entry point for key events
  void processEvent(InputID input, bool isDown);

  // Handle continuous axis events (scroll, pointer X/Y)
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value);

  // Check if preset has pointer mappings (for smart cursor locking)
  bool hasPointerMappings();

  // NEW: Helper for the Logger to peek at what will happen
  const MidiAction *getMappingForInput(InputID input);

  // Simulate input and return action with source description
  std::pair<std::optional<MidiAction>, juce::String>
  simulateInput(uintptr_t deviceHandle, int keyCode);

  // Zone management
  ZoneManager &getZoneManager() { return zoneManager; }

  // Get buffered notes (for visualizer - thread safe)
  std::vector<int> getBufferedNotes();

  // True if any manual mapping exists for this keyCode (for conflict highlight
  // in visualizer)
  bool hasManualMappingForKey(int keyCode);

  // Lookup mapping type for UI (e.g. visualizer). Returns type if mapping
  // exists for (keyCode, aliasHash), else nullopt. Use aliasHash 0 for
  // Master/Global.
  std::optional<ActionType> getMappingType(int keyCode, uintptr_t aliasHash);

  // Force rebuild of keyMapping from ValueTree (for reset operations)
  void forceRebuildMappings();

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  VoiceManager &voiceManager;
  PresetManager &presetManager;
  DeviceManager &deviceManager;
  ScaleLibrary &scaleLibrary;
  ZoneManager zoneManager;
  ExpressionEngine expressionEngine;
  SettingsManager &settingsManager;

  // Thread Safety
  juce::ReadWriteLock mapLock;
  juce::ReadWriteLock bufferLock;

  // The Fast Lookup Cache
  std::unordered_map<InputID, MidiAction> keyMapping;

  // Note buffer for Strum mode (for visualizer; strum is triggered on key
  // press)
  std::vector<int> noteBuffer;
  int bufferedStrumSpeedMs = 50;

  // Last zone key that triggered a strum (for cancel-on-new-chord)
  InputID lastStrumSource{0, 0};

  // Current CC values for relative inputs (scroll)
  std::unordered_map<InputID, float> currentCCValues;

  // Track last triggered note for SmartScaleBend
  int lastTriggeredNote = 60; // Default to middle C

  // Rhythm analyzer for adaptive glide (Phase 26.1)
  RhythmAnalyzer rhythmAnalyzer;

  // ValueTree Callbacks
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &child) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &child, int) override;
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

  // Helpers
  void rebuildMapFromTree();
  void addMappingFromTree(juce::ValueTree mappingNode);
  void removeMappingFromTree(juce::ValueTree mappingNode);
  const MidiAction *findMapping(const InputID &input);

  // Shared lookup logic (used by both processEvent and simulateInput)
  std::pair<std::optional<MidiAction>, juce::String>
  lookupAction(uintptr_t deviceHandle, int keyCode);

  // Resolve zone using same InputID as lookupAction (alias hash or 0 for "Any")
  std::shared_ptr<Zone> getZoneForInputResolved(InputID input);

  // Velocity randomization helper
  int calculateVelocity(int base, int range);

  // Random number generator for velocity humanization
  juce::Random random;
};