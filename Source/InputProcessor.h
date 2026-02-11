#pragma once
#include "DeviceManager.h"
#include "ExpressionEngine.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "PresetManager.h"
#include "RhythmAnalyzer.h"
#include "TouchpadMixerManager.h"
#include "TouchpadTypes.h"
#include "VoiceManager.h"
#include "ZoneManager.h"
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <optional>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

class ScaleLibrary;

// Custom hash for touchpad mixer tuple keys (O(1) unordered_map lookups)
struct Tuple2Hash {
  size_t operator()(const std::tuple<uintptr_t, int> &t) const {
    return std::hash<uintptr_t>{}(std::get<0>(t)) ^
           (static_cast<size_t>(std::get<1>(t)) << 16);
  }
};
struct Tuple3Hash {
  size_t operator()(const std::tuple<uintptr_t, int, int> &t) const {
    return std::hash<uintptr_t>{}(std::get<0>(t)) ^
           (static_cast<size_t>(std::get<1>(t)) << 16) ^
           (static_cast<size_t>(std::get<2>(t)) << 24);
  }
};
class Zone;

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
                 MidiEngine &midiEng, SettingsManager &settingsMgr,
                 TouchpadMixerManager &touchpadMixerMgr);
  ~InputProcessor() override;

  // The main entry point for key events
  void processEvent(InputID input, bool isDown);

  // Handle continuous axis events (scroll, pointer X/Y)
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value);

  // Handle touchpad contact updates (for alias "Touchpad" mappings)
  void processTouchpadContacts(uintptr_t deviceHandle,
                               const std::vector<TouchpadContact> &contacts);

  // Check if preset has pointer mappings (for smart cursor locking)
  bool hasPointerMappings();

  // True if compiled context has any touchpad layouts (mixer or drum pad; so
  // MainComponent can pass touchpad contacts even when device is not in
  // "Touchpad" alias).
  bool hasTouchpadLayouts() const;

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

  // Run one ExpressionEngine timer tick (for benchmarks only).
  void runExpressionEngineOneTick();

  // Get buffered notes (for visualizer - thread safe)
  std::vector<int> getBufferedNotes();

  // Phase 45: Get names of all active layers (thread-safe)
  juce::StringArray getActiveLayerNames();

  // Phase 50.9: Get highest active layer index (for Dynamic View following)
  int getHighestActiveLayerIndex() const;

  // Effective touchpad layout group solo for a given layer. If a global solo
  // group is active, that always wins; otherwise the per-layer solo is used.
  int getEffectiveSoloLayoutGroupForLayer(int layerIdx) const;

  // Clear touchpad solo states for layers that have scope=1 (forget) and are now inactive.
  // Call this whenever layer state changes.
  void clearForgetScopeSolosForInactiveLayers();

  // Thread-safe: return relative pitch-pad anchor X (normalized [0,1]) for the
  // given device/layer/event if set; nullopt if no touch has occurred yet.
  std::optional<float> getPitchPadRelativeAnchorNormX(uintptr_t deviceHandle,
                                                      int layerId,
                                                      int eventId) const;

  // Touchpad mixer: last sent CC value per fader for visualizer (thread-safe).
  // Returns numFaders values; 0 where none sent yet.
  std::vector<int> getTouchpadMixerStripCCValues(uintptr_t deviceHandle,
                                                 int stripIndex,
                                                 int numFaders) const;

  // Touchpad mixer: mute state per fader for visualizer (thread-safe).
  std::vector<bool> getTouchpadMixerStripMuteState(uintptr_t deviceHandle,
                                                   int stripIndex,
                                                   int numFaders) const;

  // Touchpad mixer: value to display per fader (unmuted value when muted, else
  // last sent CC). Thread-safe.
  std::vector<int> getTouchpadMixerStripDisplayValues(uintptr_t deviceHandle,
                                                      int stripIndex,
                                                      int numFaders) const;

  // Combined getter: displayValues + muted in one lock (for visualizer).
  struct TouchpadMixerStripState {
    std::vector<int> displayValues;
    std::vector<bool> muted;
  };
  TouchpadMixerStripState getTouchpadMixerStripState(uintptr_t deviceHandle,
                                                     int stripIndex,
                                                     int numFaders) const;

  // Region lock: effective positions for ghosts. Returns (contactId, normX,
  // normY) for locked contacts whose raw position is outside their region.
  // Visualizer draws ghost at (normX, normY).
  struct EffectiveContactPosition {
    int contactId = 0;
    float normX = 0.0f, normY = 0.0f;
  };
  std::vector<EffectiveContactPosition> getEffectiveContactPositions(
      uintptr_t deviceHandle,
      const std::vector<TouchpadContact> &contacts) const;

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
  TouchpadMixerManager &touchpadMixerManager;
  ExpressionEngine expressionEngine;
  SettingsManager &settingsManager;

  // Thread Safety
  juce::ReadWriteLock mapLock;
  juce::ReadWriteLock bufferLock;
  juce::ReadWriteLock
      mixerStateLock; // Touchpad mixer state only (reduces contention)
  mutable juce::CriticalSection stateLock; // Phase 53.7: layer state only

  // Phase 50.5 / 52.1: Grid-based compiled context (audio + visuals). Protected
  // by mapLock.
  std::shared_ptr<const CompiledMapContext> activeContext;

  // Phase 53.2: Layer state – Latched (persistent) vs Momentary (ref count).
  std::vector<bool> layerLatchedState; // 9 elements, from preset / Toggle/Solo
  std::vector<int> layerMomentaryCounts; // 9 elements, ref count for held keys

  // Touchpad layout group solo state:
  // - Global solo applies across all layers.
  // - Per-layer solo applies only to that layer (behaviour on layer change is
  //   controlled by commands; see touchpadSoloScope in MidiAction).
  int touchpadSoloLayoutGroupGlobal = 0;
  std::array<int, 9> touchpadSoloLayoutGroupPerLayer{{0, 0, 0, 0, 0, 0, 0, 0, 0}};
  // Track which per-layer solos have scope=1 (forget on layer change)
  std::array<bool, 9> touchpadSoloScopeForgetPerLayer{{false, false, false, false, false, false, false, false, false}};

  // Momentary layer chain: track which keys hold which layer (for Phantom Key)
  std::unordered_map<InputID, int>
      momentaryLayerHolds; // InputID -> target layer 0-8

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

  // Sustain release mode: one-shot latch – last chord source (keyCode < 0 =
  // none)
  InputID lastSustainChordSource{0, -1};
  Zone *lastSustainZone = nullptr;

  // Zone delayed release tracking: zone -> InputID (for override timer)
  std::unordered_map<Zone *, InputID> zoneActiveTimers;

  // Current CC values for relative inputs (scroll)
  std::unordered_map<InputID, float> currentCCValues;

  // Touchpad continuous CC/PB: last sent value per (device, layer, eventId,
  // channel, ccNumber/-1 for PB) to avoid spamming identical values when held.
  std::map<std::tuple<uintptr_t, int, int, int, int>, int>
      lastTouchpadContinuousValues;

  // Track last triggered note for SmartScaleBend
  int lastTriggeredNote = 60; // Default to middle C

  // Relative-mode pitch-pad state: per-gesture anchor X and anchor step, keyed
  // by (deviceHandle, layerId, eventId, channel) so multiple mappings for the
  // same touchpad event do not interfere. Protected by anchorLock.
  mutable juce::CriticalSection anchorLock;
  std::map<std::tuple<uintptr_t, int, int, int>, float> pitchPadRelativeAnchorT;
  std::map<std::tuple<uintptr_t, int, int, int>, float>
      pitchPadRelativeAnchorStep;

  // Rhythm analyzer for adaptive glide (Phase 26.1)
  RhythmAnalyzer rhythmAnalyzer;

  // Touchpad: previous state per device for edge detection (Finger Down/Up)
  struct TouchpadPrevState {
    bool tip1 = false;
    bool tip2 = false;
    float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
  };
  std::unordered_map<uintptr_t, TouchpadPrevState> touchpadPrevState;
  // Continuous->Note: track whether note is currently "on" per (device,
  // layerId, eventId) to send note off
  std::set<std::tuple<uintptr_t, int, int>> touchpadNoteOnSent;
  // Touchpad BoolToCC Expression: active envelopes to release when finger lifts
  std::set<std::tuple<uintptr_t, int, int>> touchpadExpressionActive;

  // Touchpad mixer: per-strip per-contact prev state for edge detection
  struct TouchpadContactPrev {
    bool tipDown = false;
    float x = 0.0f, y = 0.0f;
  };
  std::unordered_map<std::tuple<uintptr_t, int, int>, TouchpadContactPrev,
                    Tuple3Hash>
      touchpadMixerContactPrev;

  // Region lock: (deviceHandle, contactId) -> (TouchpadType, stripIndex)
  std::unordered_map<std::tuple<uintptr_t, int>, std::pair<TouchpadType, size_t>,
                    Tuple2Hash>
      contactLayoutLock;

  // Touchpad mixer strips: lock mode (device, stripIndex) -> locked fader index
  // (-1 = none). unordered_map for O(1) lookups.
  std::unordered_map<std::tuple<uintptr_t, int>, int, Tuple2Hash>
      touchpadMixerLockedFader;
  // Touchpad mixer: per-strip applier (finger2) down state for edge detection.
  // Needed so Precision mode can detect finger2 down/up even when we have <2
  // active contacts (where the main mixer processing early-outs).
  std::unordered_map<std::tuple<uintptr_t, int>, bool, Tuple2Hash>
      touchpadMixerApplierDownPrev;
  std::unordered_map<std::tuple<uintptr_t, int, int>, float, Tuple3Hash>
      touchpadMixerRelativeValue;
  // Relative mode: anchor Y (effectiveYClamped) when gesture started or entered fader
  std::unordered_map<std::tuple<uintptr_t, int, int>, float, Tuple3Hash>
      touchpadMixerRelativeAnchor;
  // (device, stripIdx) -> last fader index for free-mode switch detection (-1 = none)
  std::unordered_map<std::tuple<uintptr_t, int>, int, Tuple2Hash>
      touchpadMixerLastFaderIndex;
  std::unordered_map<std::tuple<uintptr_t, int, int>, bool, Tuple3Hash>
      touchpadMixerMuteState;
  std::unordered_map<std::tuple<uintptr_t, int, int>, int, Tuple3Hash>
      lastTouchpadMixerCCValues;
  std::unordered_map<std::tuple<uintptr_t, int, int>, int, Tuple3Hash>
      touchpadMixerValueBeforeMute;

  // Drum pad / Harmonic grid: active notes per (deviceHandle, stripIdx,
  // contactId). Value stores InputID and padIndex so we can send note off when
  // finger moves outside or to a different pad.
  struct DrumPadKeyHash {
    size_t operator()(const std::tuple<uintptr_t, int, int> &t) const {
      return Tuple3Hash{}(t);
    }
  };
  struct DrumPadActiveEntry {
    InputID inputId;
    int padIndex;
  };
  std::unordered_map<std::tuple<uintptr_t, int, int>, DrumPadActiveEntry,
                    DrumPadKeyHash>
      drumPadActiveNotes;

  // Chord Pad: active chords per (deviceHandle, stripIdx, contactId) in
  // momentary mode. Each entry represents an InputID that currently holds a
  // chord on voiceManager; handleKeyUp releases the entire chord.
  std::unordered_map<std::tuple<uintptr_t, int, int>, DrumPadActiveEntry,
                     DrumPadKeyHash>
      chordPadActiveChords;

  // Chord Pad latch: per-pad latched chords (deviceHandle, stripIdx, padIndex).
  // Used when latch mode is enabled; toggling a pad on/off corresponds to
  // noteOn / handleKeyUp for the stored InputID.
  std::unordered_map<std::tuple<uintptr_t, int, int>, DrumPadActiveEntry,
                     DrumPadKeyHash>
      chordPadLatchedPads;
  // Last seen latchMode per compiled Chord Pad strip index (for clearing
  // latched chords when latch is turned off).
  std::unordered_map<int, bool> chordPadLastLatchMode;

  // Throttle sendChangeMessage for mixer (~60 Hz)
  std::atomic<int64_t> lastMixerChangeNotifyMs{0};

  // ValueTree Callbacks
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &child) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &child, int) override;
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

  // Helpers
  void rebuildGrid();

  // Sustain default/cleanup: called on init and when sustain-related mappings
  // change
  void applySustainDefaultFromPreset();

  // Phase 52.1: Grid lookup for getMappingForInput / handleAxisEvent (replaces
  // findMapping)
  std::optional<MidiAction> lookupActionInGrid(InputID input) const;

  // Velocity randomization helper
  int calculateVelocity(int base, int range);

  // Shared manual-note trigger (keyboard and touchpad). Respects
  // releaseBehavior and velocity random.
  // allowLatchFromAction: if false, alwaysLatch is forced false (e.g. for
  // touchpad Finger Up events where Latch only applies to Down).
  void triggerManualNoteOn(InputID id, const MidiAction &act,
                           bool allowLatchFromAction = true);
  void triggerManualNoteRelease(InputID id, const MidiAction &act);

  // Phase 50.5: Zone processing helpers (extract complex zone logic)
  void processZoneNote(InputID input, std::shared_ptr<Zone> zone,
                       const MidiAction &action);
  void processZoneChord(InputID input, std::shared_ptr<Zone> zone,
                        const std::vector<MidiAction> &chordActions,
                        const MidiAction &rootAction);

  // Random number generator for velocity humanization
  juce::Random random;

  // Grant tests access to internal touchpad state for behavioural verification.
  friend class TouchpadPitchPadTest;
};