#pragma once
#include "DeviceManager.h"
#include "ExpressionEngine.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "PresetManager.h"
#include "RhythmAnalyzer.h"
#include "VoiceManager.h"
#include "ZoneManager.h"
#include <JuceHeader.h>
#include <set>
#include <unordered_map>
#include <vector>

class ScaleLibrary;

// Phase 40.1: ordering for std::set<InputID>
struct InputIDLess {
  bool operator()(const InputID &a, const InputID &b) const {
    if (a.deviceHandle != b.deviceHandle)
      return a.deviceHandle < b.deviceHandle;
    return a.keyCode < b.keyCode;
  }
};

class MidiEngine;
class SettingsManager;

class InputProcessor : public juce::ChangeBroadcaster,
                       public juce::ValueTree::Listener,
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

  // Phase 41.3: Return copy by value to avoid dangling pointer after unlock
  std::optional<MidiAction> getMappingForInput(InputID input);

  // Simulate input and return action with source description (Phase 39: returns
  // SimulationResult)
  SimulationResult simulateInput(uintptr_t viewDeviceHash, int keyCode);
  // Phase 45.3: simulate for a specific layer context (editing view)
  SimulationResult simulateInput(uintptr_t viewDeviceHash, int keyCode,
                                 int targetLayerId);

  // Phase 50.6: expose compiled context for visualizer (thread-safe)
  // Returns a copy of the shared_ptr so it stays alive while in use.
  std::shared_ptr<const CompiledMapContext> getContext() const;

  // Zone management
  ZoneManager &getZoneManager() { return zoneManager; }

  // Get buffered notes (for visualizer - thread safe)
  std::vector<int> getBufferedNotes();

  // Phase 45: Get names of all active layers (thread-safe)
  juce::StringArray getActiveLayerNames();

  // Phase 50.9: Get highest active layer index (for Dynamic View following)
  int getHighestActiveLayerIndex() const;

  // True if any manual mapping exists for this keyCode (for conflict highlight
  // in visualizer)
  bool hasManualMappingForKey(int keyCode);

  // Lookup mapping type for UI (e.g. visualizer). Returns type if mapping
  // exists for (keyCode, aliasHash), else nullopt. Use aliasHash 0 for
  // Master/Global.
  std::optional<ActionType> getMappingType(int keyCode, uintptr_t aliasHash);

  // Count manual mappings for a specific key/alias (for conflict detection)
  int getManualMappingCountForKey(int keyCode, uintptr_t aliasHash) const;

  // Force rebuild of keyMapping from ValueTree (for reset operations)
  void forceRebuildMappings();

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

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
  mutable juce::CriticalSection stateLock; // Phase 53.7: layer state only

  // Phase 50.5 / 52.1: Grid-based compiled context (audio + visuals). Protected
  // by mapLock.
  std::shared_ptr<const CompiledMapContext> activeContext;

  // Phase 53.2: Layer state – Latched (persistent) vs Momentary (ref count).
  std::vector<bool> layerLatchedState; // 9 elements, from preset / Toggle/Solo
  std::vector<int> layerMomentaryCounts; // 9 elements, ref count for held keys

  // Helper: Layer 0 always active; else active if latched or momentary count >
  // 0
  bool isLayerActive(int layerIdx) const;

  // Phase 40.1: currently held keys for state reconstruction
  std::set<InputID, InputIDLess> currentlyHeldKeys;
  bool updateLayerState(); // returns true if momentary state changed

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
  void rebuildGrid();

  // Sustain default/cleanup: called on init and when sustain-related mappings change
  void applySustainDefaultFromPreset();

  // Phase 52.1: Grid lookup for getMappingForInput / handleAxisEvent (replaces
  // findMapping)
  std::optional<MidiAction> lookupActionInGrid(InputID input) const;

  // Velocity randomization helper
  int calculateVelocity(int base, int range);

  // Phase 50.5: Zone processing helpers (extract complex zone logic)
  void processZoneNote(InputID input, std::shared_ptr<Zone> zone,
                       const MidiAction &action);
  void processZoneChord(InputID input, std::shared_ptr<Zone> zone,
                        const std::vector<MidiAction> &chordActions,
                        const MidiAction &rootAction);

  // Random number generator for velocity humanization
  juce::Random random;
};