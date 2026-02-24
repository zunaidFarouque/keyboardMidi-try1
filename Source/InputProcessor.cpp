#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "PitchPadUtilities.h"
#include "ScaleLibrary.h"
#include "ScaleUtilities.h"
#include "SettingsManager.h"
#include "TouchpadMixerTypes.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <tuple>

// Phase 39.9: Modifier key aliasing (Left/Right -> Generic).
// Visualizer/layout uses VK_LSHIFT/VK_RSHIFT etc, while older mappings may
// store VK_SHIFT/VK_CONTROL/VK_MENU. This helper allows fallback lookups.
static int getGenericKey(int specificKey) {
  switch (specificKey) {
  case 0xA0:     // VK_LSHIFT
  case 0xA1:     // VK_RSHIFT
    return 0x10; // VK_SHIFT
  case 0xA2:     // VK_LCONTROL
  case 0xA3:     // VK_RCONTROL
    return 0x11; // VK_CONTROL
  case 0xA4:     // VK_LMENU
  case 0xA5:     // VK_RMENU
    return 0x12; // VK_MENU
  default:
    return 0;
  }
}

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr,
                               DeviceManager &deviceMgr, ScaleLibrary &scaleLib,
                               MidiEngine &midiEng,
                               SettingsManager &settingsMgr,
                               TouchpadMixerManager &touchpadMixerMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr),
      deviceManager(deviceMgr), zoneManager(scaleLib), scaleLibrary(scaleLib),
      touchpadMixerManager(touchpadMixerMgr), expressionEngine(midiEng),
      settingsManager(settingsMgr) {
  // Phase 53.2: 9 layers; momentary = ref count, latched = persistent
  layerLatchedState.resize(9);
  layerMomentaryCounts.resize(9);
  for (int i = 0; i < 9; ++i) {
    layerLatchedState[(size_t)i] = false;
    layerMomentaryCounts[(size_t)i] = 0;
    touchpadSoloLayoutGroupPerLayer[(size_t)i] = 0;
    touchpadSoloScopeForgetPerLayer[(size_t)i] = false;
  }
  touchpadSoloLayoutGroupGlobal = 0;
  activeContext = std::make_shared<CompiledMapContext>();
}

bool InputProcessor::isLayerActive(int layerIdx) const {
  if (layerIdx == 0)
    return true; // Base always active
  if (layerIdx < 0 || layerIdx >= 9)
    return false;
  return layerLatchedState[(size_t)layerIdx] ||
         (layerMomentaryCounts[(size_t)layerIdx] > 0);
}

int InputProcessor::getEffectiveSoloLayoutGroupForLayer(int layerIdx) const {
  juce::ScopedLock sl(stateLock);
  if (touchpadSoloLayoutGroupGlobal > 0)
    return touchpadSoloLayoutGroupGlobal;
  if (layerIdx < 0 || layerIdx >= 9)
    return 0;
  return touchpadSoloLayoutGroupPerLayer[(size_t)layerIdx];
}

// Shared helpers for touchpad mapping region logic. These centralize the
// notion of a mapping "drive point" (which contact/coordinate controls the
// mapping for a given eventId) and the region hit-test so all conversion kinds
// use consistent rules.
static bool touchpadMappingHasRegion(const TouchpadMappingEntry &entry) {
  return (entry.regionLeft != 0.0f || entry.regionTop != 0.0f ||
          entry.regionRight != 1.0f || entry.regionBottom != 1.0f);
}

static std::pair<float, float>
touchpadMappingDrivePoint(const TouchpadMappingEntry &entry, float x1,
                          float y1, float x2, float y2, float avgX,
                          float avgY) {
  switch (entry.eventId) {
  case TouchpadEvent::Finger1Down:
  case TouchpadEvent::Finger1Up:
  case TouchpadEvent::Finger1X:
  case TouchpadEvent::Finger1Y:
    return {x1, y1};
  case TouchpadEvent::Finger2Down:
  case TouchpadEvent::Finger2Up:
  case TouchpadEvent::Finger2X:
  case TouchpadEvent::Finger2Y:
    return {x2, y2};
  case TouchpadEvent::Finger1And2Dist:
  case TouchpadEvent::Finger1And2AvgX:
  case TouchpadEvent::Finger1And2AvgY:
    return {avgX, avgY};
  default:
    break;
  }
  return {0.0f, 0.0f};
}

static bool touchpadMappingContainsDrivePoint(
    const TouchpadMappingEntry &entry, float x1, float y1, float x2, float y2,
    float avgX, float avgY) {
  if (!touchpadMappingHasRegion(entry))
    return true;
  auto drive = touchpadMappingDrivePoint(entry, x1, y1, x2, y2, avgX, avgY);
  const float testX = drive.first;
  const float testY = drive.second;
  return (testX >= entry.regionLeft && testX < entry.regionRight &&
          testY >= entry.regionTop && testY < entry.regionBottom);
}

void InputProcessor::clearForgetScopeSolosForInactiveLayers() {
  juce::ScopedLock sl(stateLock);
  for (int i = 0; i < 9; ++i) {
    if (i == 0)
      continue; // Base layer is always active
    if (!isLayerActive(i) && touchpadSoloScopeForgetPerLayer[(size_t)i] &&
        touchpadSoloLayoutGroupPerLayer[(size_t)i] > 0) {
      touchpadSoloLayoutGroupPerLayer[(size_t)i] = 0;
      touchpadSoloScopeForgetPerLayer[(size_t)i] = false;
    }
  }
}

void InputProcessor::initialize() {
  presetManager.getRootNode().addListener(this);
  presetManager.getLayersList().addListener(this);
  presetManager.addChangeListener(this);
  deviceManager.addChangeListener(this);
  settingsManager.addChangeListener(this);
  zoneManager.addChangeListener(this);
  touchpadMixerManager.addChangeListener(this);
  rebuildGrid();
  applySustainDefaultFromPreset();
}

void InputProcessor::runExpressionEngineOneTick() {
  expressionEngine.processOneTick();
}

InputProcessor::~InputProcessor() {
  stopTimer(); // Touch glide release timer
  // Remove listeners
  presetManager.getRootNode().removeListener(this);
  presetManager.getLayersList().removeListener(
      this); // Phase 41: Remove layer listener
  presetManager.removeChangeListener(this);
  deviceManager.removeChangeListener(this);
  settingsManager.removeChangeListener(this);
  zoneManager.removeChangeListener(this);
  touchpadMixerManager.removeChangeListener(this);
}

void InputProcessor::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &presetManager || source == &deviceManager ||
      source == &settingsManager || source == &zoneManager ||
      source == &touchpadMixerManager) {
    rebuildGrid();
    if (source == &presetManager)
      applySustainDefaultFromPreset(); // Preset load: apply sustain default
  }
}

// Sustain: if any Sustain Inverse (data1=2) is mapped, default sustain = ON;
// otherwise default = OFF and clear sustain state
static bool hasSustainInverseMapped(PresetManager &presetMgr) {
  const int sustainInverse =
      static_cast<int>(MIDIQy::CommandID::SustainInverse);
  for (int layer = 0; layer < 9; ++layer) {
    auto list = presetMgr.getEnabledMappingsForLayer(layer);
    for (const auto &m : list) {
      if (m.isValid() &&
          m.getProperty("type", juce::String())
              .toString()
              .equalsIgnoreCase("Command") &&
          static_cast<int>(m.getProperty("data1", -1)) == sustainInverse)
        return true;
    }
  }
  return false;
}

void InputProcessor::applySustainDefaultFromPreset() {
  if (hasSustainInverseMapped(presetManager))
    voiceManager.setSustain(true); // Default ON when Sustain Inverse exists
  else
    voiceManager.setSustain(false); // OFF and clear when no Inverse
}

void InputProcessor::rebuildGrid() {
  ++rebuildCount_;
  auto newContext =
      GridCompiler::compile(presetManager, deviceManager, zoneManager,
                            touchpadMixerManager, settingsManager);
  {
    juce::ScopedWriteLock sl(mapLock);
    activeContext = std::move(newContext);
  }
  touchpadNoteOnSent.clear();
  touchpadPrevState.clear();
  touchpadPitchGlideState.clear();
  stopTouchGlideTimerIfIdle();
  {
    juce::ScopedWriteLock wl(mixerStateLock);
    touchpadMixerContactPrev.clear();
    contactLayoutLock.clear();
    touchpadMixerLockedFader.clear();
    touchpadMixerApplierDownPrev.clear();
    touchpadMixerRelativeValue.clear();
    touchpadMixerRelativeAnchor.clear();
    touchpadMixerLastFaderIndex.clear();
    touchpadMixerMuteState.clear();
    lastTouchpadMixerCCValues.clear();
    touchpadMixerValueBeforeMute.clear();
    drumPadActiveNotes.clear();
    chordPadActiveChords.clear();
    chordPadLatchedPads.clear();
  }
  contactMappingLock.clear();
  touchpadMappingPrevState.clear();
  // SlideToCC / EncoderCC state: clear so mappings work after rebuild (no stale
  // lastTouchpadSlideCCValues preventing first send).
  lastTouchpadSlideCCValues.clear();
  touchpadSlideRelativeValue.clear();
  touchpadSlideRelativeAnchor.clear();
  touchpadSlideLockedContact.clear();
  touchpadSlideContactPrev.clear();
  touchpadSlideApplierDownPrev.clear();
  touchpadSlideReturnState.clear();
  // Phase 53.7: Layer state under stateLock only
  {
    juce::ScopedLock sl(stateLock);
    momentaryLayerHolds.clear(); // Momentary chain: stale after grid rebuild
    for (int i = 0; i < 9; ++i) {
      juce::ValueTree layerNode = presetManager.getLayerNode(i);
      if (layerNode.isValid())
        layerLatchedState[(size_t)i] =
            (bool)layerNode.getProperty("isActive", i == 0);
      else
        layerLatchedState[(size_t)i] = (i == 0);
      touchpadSoloLayoutGroupPerLayer[(size_t)i] = 0;
      touchpadSoloScopeForgetPerLayer[(size_t)i] = false;
    }
    touchpadSoloLayoutGroupGlobal = 0;
  }
  sendChangeMessage();
}

// Phase 52.1: Grid lookup (replaces findMapping). Returns action if slot is
// active. Phase 53.7: Snapshot layer state under stateLock to avoid data race.
std::optional<MidiAction>
InputProcessor::lookupActionInGrid(InputID input) const {
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }

  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return std::nullopt;

  uintptr_t effectiveDevice = input.deviceHandle;
  if (!settingsManager.isStudioMode())
    effectiveDevice = 0;

  const int keyCode = input.keyCode;
  if (keyCode < 0 || keyCode > 0xFF)
    return std::nullopt;

  const int genericKey = getGenericKey(keyCode);

  for (int i = 8; i >= 0; --i) {
    if (!activeLayersSnapshot[(size_t)i])
      continue;

    std::shared_ptr<const AudioGrid> grid;
    if (effectiveDevice != 0) {
      auto it = ctx->deviceGrids.find(effectiveDevice);
      if (it != ctx->deviceGrids.end() && it->second[(size_t)i])
        grid = it->second[(size_t)i];
    }
    if (!grid)
      grid = ctx->globalGrids[(size_t)i];
    if (!grid)
      continue;

    const auto &slot = (*grid)[(size_t)keyCode];
    if (slot.isActive)
      return slot.action;
    if (genericKey != 0) {
      const auto &genSlot = (*grid)[(size_t)genericKey];
      if (genSlot.isActive)
        return genSlot.action;
    }
  }
  return std::nullopt;
}

// Helper to convert alias name to hash (used by processEvent for zone lookup)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  const juce::String trimmed = aliasName.trim();
  if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("Any / Master") ||
      trimmed.equalsIgnoreCase("Global (All Devices)") ||
      trimmed.equalsIgnoreCase("Global") ||
      trimmed.equalsIgnoreCase("Unassigned"))
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(trimmed));
}

void InputProcessor::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  if (presetManager.getIsLoading())
    return;

  // Phase 41 / 52.1: Layer or mapping tree changed; compiler owns data.
  if (parentTree.hasType("Layers") || childWhichHasBeenAdded.hasType("Layer") ||
      childWhichHasBeenAdded.hasType("Mappings")) {
    rebuildGrid();
    return;
  }

  if (parentTree.hasType("Mappings")) {
    auto layerNode = parentTree.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      rebuildGrid();
      return;
    }
  }

  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    rebuildGrid();
  }
}

void InputProcessor::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {
  if (presetManager.getIsLoading())
    return;

  if (parentTree.hasType("Layers") ||
      childWhichHasBeenRemoved.hasType("Mappings")) {
    rebuildGrid();
    return;
  }

  if (parentTree.hasType("Mappings")) {
    auto layerNode = parentTree.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      rebuildGrid();
      return;
    }
  }

  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    rebuildGrid();
  }
}

void InputProcessor::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  if (presetManager.getIsLoading())
    return;

  // Phase 52.1: Layer property changes — update layer state only (grid
  // unchanged).
  if (treeWhosePropertyHasChanged.hasType("Layer")) {
    if (property == juce::Identifier("name") ||
        property == juce::Identifier("isActive")) {
      juce::ScopedWriteLock lock(mapLock);
      int layerId = treeWhosePropertyHasChanged.getProperty("id", -1);
      if (layerId >= 0 && layerId < 9) {
        layerLatchedState[(size_t)layerId] =
            (bool)treeWhosePropertyHasChanged.getProperty("isActive",
                                                          layerId == 0);
      }
      clearForgetScopeSolosForInactiveLayers();
      sendChangeMessage();
      return;
    }
  }

  auto mappingsNode = presetManager.getMappingsNode();
  auto parent = treeWhosePropertyHasChanged.getParent();

  // Phase 41: Check if this is a mapping in a layer
  bool isLayerMapping = false;
  if (treeWhosePropertyHasChanged.hasType("Mapping")) {
    auto layerNode = parent.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      isLayerMapping = true;
    }
  }

  // Sustain cleanup: when a Command mapping's type or data1 changes, apply
  // default
  if (treeWhosePropertyHasChanged.hasType("Mapping") &&
      (property == juce::Identifier("type") ||
       (property == juce::Identifier("data1") &&
        treeWhosePropertyHasChanged.getProperty("type", juce::String())
            .toString()
            .equalsIgnoreCase("Command")))) {
    applySustainDefaultFromPreset();
  }

  // Optimization: Only rebuild if relevant properties changed (affects compiled
  // grid / touchpad mappings)
  if (property == juce::Identifier("enabled") ||
      property == juce::Identifier("inputKey") ||
      property == juce::Identifier("layerID") ||
      property == juce::Identifier("deviceHash") ||
      property == juce::Identifier("inputAlias") ||
      property == juce::Identifier("inputTouchpadEvent") ||
      property == juce::Identifier("type") ||
      property == juce::Identifier("channel") ||
      property == juce::Identifier("data1") ||
      property == juce::Identifier("data2") ||
      property == juce::Identifier("velRandom") ||
      property == juce::Identifier("releaseBehavior") ||
      property == juce::Identifier("followTranspose") ||
      property == juce::Identifier("touchpadThreshold") ||
      property == juce::Identifier("touchpadTriggerAbove") ||
      property == juce::Identifier("touchpadValueWhenOn") ||
      property == juce::Identifier("touchpadValueWhenOff") ||
      property == juce::Identifier("touchpadInputMin") ||
      property == juce::Identifier("touchpadInputMax") ||
      property == juce::Identifier("touchpadOutputMin") ||
      property == juce::Identifier("touchpadOutputMax") ||
      property == juce::Identifier("pitchPadMode") ||
      property == juce::Identifier("pitchPadStart") ||
      property == juce::Identifier("pitchPadUseCustomRange") ||
      property == juce::Identifier("pitchPadTouchGlideMs") ||
      property == juce::Identifier("pitchPadRestingPercent") ||
      property == juce::Identifier("pitchPadRestZonePercent") ||
      property == juce::Identifier("pitchPadTransitionZonePercent") ||
      property == juce::Identifier("pitchPadCustomStart") ||
      property == juce::Identifier("adsrTarget") ||
      property == juce::Identifier("smartStepShift") ||
      property == juce::Identifier("smartScaleFollowGlobal") ||
      property == juce::Identifier("smartScaleName") ||
      property == juce::Identifier("pbRange") ||
      property == juce::Identifier("pbShift") ||
      property == juce::Identifier("adsrAttack") ||
      property == juce::Identifier("adsrDecay") ||
      property == juce::Identifier("adsrSustain") ||
      property == juce::Identifier("adsrRelease") ||
      property == juce::Identifier("useCustomEnvelope") ||
      property == juce::Identifier("sendReleaseValue") ||
      property == juce::Identifier("releaseValue") ||
      property == juce::Identifier("releaseLatchedOnToggleOff") ||
      property == juce::Identifier("touchpadLayoutGroupId") ||
      property == juce::Identifier("touchpadSoloScope")) {
    if (isLayerMapping || parent.isEquivalentTo(mappingsNode) ||
        treeWhosePropertyHasChanged.isEquivalentTo(mappingsNode)) {
      // SAFER: Rebuild everything.
      // The logic to "Remove Old -> Add New" is impossible here because
      // we don't know the Old values anymore (the Tree is already updated).
      juce::ScopedWriteLock lock(mapLock);
      rebuildGrid(); // Phase 50.5: Only rebuild grid
    }
  }
}

std::optional<MidiAction> InputProcessor::getMappingForInput(InputID input) {
  return lookupActionInGrid(input);
}

int InputProcessor::calculateVelocity(int base, int range) {
  if (range <= 0)
    return base;

  // Generate random delta in range [-range, +range]
  int delta = random.nextInt(range * 2 + 1) - range;

  // Clamp result between 1 and 127
  return juce::jlimit(1, 127, base + delta);
}

void InputProcessor::triggerManualNoteOn(InputID id, const MidiAction &act,
                                         bool allowLatchFromAction) {
  int vel = calculateVelocity(act.data2, act.velocityRandom);
  bool alwaysLatch = allowLatchFromAction &&
                     (act.releaseBehavior == NoteReleaseBehavior::AlwaysLatch);
  bool sustainUntilRetrigger =
      (act.releaseBehavior == NoteReleaseBehavior::SustainUntilRetrigger);
  voiceManager.noteOn(id, act.data1, vel, act.channel, true, 0,
                      PolyphonyMode::Poly, 50, alwaysLatch,
                      sustainUntilRetrigger);
  lastTriggeredNote = act.data1;
}

void InputProcessor::triggerManualNoteRelease(InputID id,
                                              const MidiAction &act) {
  if (act.releaseBehavior == NoteReleaseBehavior::SustainUntilRetrigger)
    return;
  voiceManager.handleKeyUp(id);
}

// Phase 53.2: Momentary is updated in processEvent via ref count; no recompute.
bool InputProcessor::updateLayerState() { return false; }

void InputProcessor::processEvent(InputID input, bool isDown) {
  // Gate: If MIDI mode is not active, don't generate MIDI
  if (!settingsManager.isMidiModeActive()) {
    return;
  }

  // Phase 40.1: Track held keys and recompute momentary layers.
  InputID held = input;
  if (!settingsManager.isStudioMode())
    held.deviceHandle = 0;

  {
    juce::ScopedWriteLock wl(mapLock);
    if (isDown)
      currentlyHeldKeys.insert(held);
    else
      currentlyHeldKeys.erase(held);
  }

  // Phase 53.7: Snapshot active layers under state lock (no lock inside loop)
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }

  // Momentary layer chain: key-up of a key holding a layer (Phantom Key fix)
  if (!isDown) {
    juce::ScopedLock sl(stateLock);
    auto it = momentaryLayerHolds.find(held);
    if (it != momentaryLayerHolds.end()) {
      int target = juce::jlimit(0, 8, it->second);
      if (layerMomentaryCounts[(size_t)target] > 0)
        layerMomentaryCounts[(size_t)target]--;
      momentaryLayerHolds.erase(it);
      sendChangeMessage();
      return;
    }
  }

  {
    juce::ScopedReadLock rl(mapLock);
    auto ctx = activeContext;
    if (!ctx)
      return;

    uintptr_t effectiveDevice = input.deviceHandle;
    if (!settingsManager.isStudioMode())
      effectiveDevice = 0;

    const int keyCode = input.keyCode;
    if (keyCode < 0 || keyCode > 0xFF)
      return;

    for (int layerIdx = 8; layerIdx >= 0; --layerIdx) {
      if (!activeLayersSnapshot[(size_t)layerIdx])
        continue;

      // Lookup: Try device-specific grid first, then global fallback
      std::shared_ptr<const AudioGrid> grid;
      if (effectiveDevice != 0) {
        auto it = ctx->deviceGrids.find(effectiveDevice);
        if (it != ctx->deviceGrids.end() && it->second[(size_t)layerIdx]) {
          grid = it->second[(size_t)layerIdx];
        }
      }
      if (!grid) {
        grid = ctx->globalGrids[(size_t)layerIdx];
      }

      if (!grid)
        continue;

      // Fetch slot
      const auto &slot = (*grid)[(size_t)keyCode];
      if (!slot.isActive)
        continue;

      // Hit! Process this action
      const auto &midiAction = slot.action;

      // Check if this is a zone (by checking ZoneManager - zones need special
      // handling)
      std::shared_ptr<Zone> zone;
      if (midiAction.type == ActionType::Note) {
        // Resolve alias for zone lookup
        juce::String aliasName =
            deviceManager.getAliasForHardware(effectiveDevice);
        uintptr_t aliasHash = 0;
        if (aliasName != "Unassigned" && !aliasName.isEmpty()) {
          aliasHash = aliasNameToHash(aliasName);
        }
        zone =
            zoneManager.getZoneForInput(InputID{aliasHash, keyCode}, layerIdx);
        if (!zone) {
          zone = zoneManager.getZoneForInput(InputID{0, keyCode}, layerIdx);
        }
      }

      if (!isDown) {
        // Key-up handling
        if (midiAction.type == ActionType::Command) {
          int cmd = midiAction.data1;
          if (cmd == static_cast<int>(MIDIQy::CommandID::LayerMomentary)) {
            int target = juce::jlimit(0, 8, midiAction.data2);
            {
              juce::ScopedLock sl(stateLock);
              if (layerMomentaryCounts[(size_t)target] > 0)
                layerMomentaryCounts[(size_t)target]--;
            }
            clearForgetScopeSolosForInactiveLayers();
            sendChangeMessage();
            return;
          }
          if (cmd == static_cast<int>(MIDIQy::CommandID::SustainMomentary))
            voiceManager.setSustain(false);
          else if (cmd == static_cast<int>(MIDIQy::CommandID::SustainInverse))
            voiceManager.setSustain(true);
          else if (cmd == static_cast<int>(
                                 MIDIQy::CommandID::
                                     TouchpadLayoutGroupSoloMomentary)) {
            // Release for temporary solo: restore previous solo state for scope.
            int scope = midiAction.touchpadSoloScope;
            int currentLayer = getHighestActiveLayerIndex();
            juce::ScopedLock sl(stateLock);
            if (scope == 0) {
              touchpadSoloLayoutGroupGlobal = 0;
            } else if (scope == 1 || scope == 2) {
              if (currentLayer >= 0 && currentLayer < 9) {
                touchpadSoloLayoutGroupPerLayer[(size_t)currentLayer] = 0;
                touchpadSoloScopeForgetPerLayer[(size_t)currentLayer] = false;
              }
            }
            sendChangeMessage();
          }
        }
        if (midiAction.type == ActionType::Expression) {
          expressionEngine.releaseEnvelope(input);
          // Phase 56.1: Release value for Fast Path (or simple CC-style)
          if (midiAction.sendReleaseValue) {
            if (midiAction.adsrSettings.target == AdsrTarget::CC)
              voiceManager.sendCC(midiAction.channel,
                                  midiAction.adsrSettings.ccNumber,
                                  midiAction.releaseValue);
            else if (midiAction.adsrSettings.target == AdsrTarget::PitchBend ||
                     midiAction.adsrSettings.target ==
                         AdsrTarget::SmartScaleBend)
              voiceManager.sendPitchBend(midiAction.channel, 8192);
          }
          return;
        }
        // Phase 55.4: Note release behavior
        if (midiAction.type == ActionType::Note &&
            midiAction.releaseBehavior ==
                NoteReleaseBehavior::SustainUntilRetrigger) {
          return; // Do nothing on release
        }
        // Sustain mode: one-shot latch – do not send note-off on release
        if (zone && zone->releaseBehavior == Zone::ReleaseBehavior::Sustain) {
          return; // Notes stay on until next chord
        }
        if (zone && zone->playMode == Zone::PlayMode::Strum) {
          juce::ScopedWriteLock lock(bufferLock);
          noteBuffer.clear();
          bufferedStrumSpeedMs = 50;
        }
        // Normal release: instant or delayed
        if (zone && zone->releaseBehavior == Zone::ReleaseBehavior::Normal) {
          if (zone->delayReleaseOn)
            voiceManager.handleKeyUp(input, zone->releaseDurationMs, false);
          else
            voiceManager.handleKeyUp(input);
        } else if (!zone) {
          triggerManualNoteRelease(input, midiAction);
        }
        return; // Stop searching lower layers
      }

      // Key-down handling
      if (midiAction.type == ActionType::Expression) {
        int peakValue = midiAction.data2;
        if (midiAction.adsrSettings.target == AdsrTarget::PitchBend) {
          double stepsPerSemitone = settingsManager.getStepsPerSemitone();
          peakValue =
              static_cast<int>(8192.0 + (midiAction.data2 * stepsPerSemitone));
          peakValue = juce::jlimit(0, 16383, peakValue);
        } else if (midiAction.adsrSettings.target ==
                   AdsrTarget::SmartScaleBend) {
          if (!midiAction.smartBendLookup.empty() && lastTriggeredNote >= 0 &&
              lastTriggeredNote < 128) {
            peakValue = midiAction.smartBendLookup[lastTriggeredNote];
          } else {
            peakValue = 8192;
          }
        }
        expressionEngine.triggerEnvelope(input, midiAction.channel,
                                         midiAction.adsrSettings, peakValue);
        return;
      }

      if (midiAction.type == ActionType::Command) {
        int cmd = midiAction.data1;
        int layerId = midiAction.data2;
        if (cmd == static_cast<int>(MIDIQy::CommandID::LayerMomentary)) {
          int target = juce::jlimit(0, 8, midiAction.data2);
          {
            juce::ScopedLock sl(stateLock);
            layerMomentaryCounts[(size_t)target]++;
            momentaryLayerHolds[held] = target;
          }
          clearForgetScopeSolosForInactiveLayers();
          sendChangeMessage();
          return;
        }
        if (cmd == static_cast<int>(MIDIQy::CommandID::LayerToggle)) {
          if (isDown) {
            int target = juce::jlimit(0, 8, layerId);
            {
              juce::ScopedLock sl(stateLock);
              layerLatchedState[(size_t)target] =
                  !layerLatchedState[(size_t)target];
            }
            clearForgetScopeSolosForInactiveLayers();
            sendChangeMessage();
            return;
          }
          return;
        }
        if (cmd == static_cast<int>(MIDIQy::CommandID::SustainMomentary))
          voiceManager.setSustain(true);
        else if (cmd == static_cast<int>(MIDIQy::CommandID::SustainToggle))
          voiceManager.setSustain(!voiceManager.isSustainActive());
        else if (cmd == static_cast<int>(MIDIQy::CommandID::SustainInverse))
          voiceManager.setSustain(false);
        else if (cmd == static_cast<int>(MIDIQy::CommandID::LatchToggle)) {
          bool wasActive = voiceManager.isLatchActive();
          voiceManager.setLatch(!wasActive);
          if (wasActive && !voiceManager.isLatchActive() &&
              midiAction.releaseLatchedOnLatchToggleOff)
            voiceManager.panicLatch();
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::Panic)) {
          if (midiAction.data2 == 1)
            voiceManager.panicLatch();
          else if (midiAction.data2 == 2) {
            // Panic chords: turn off sustain-held chord (Sustain release mode)
            if (lastSustainChordSource.keyCode >= 0) {
              voiceManager.handleKeyUp(lastSustainChordSource);
              lastSustainChordSource = InputID{0, -1};
              lastSustainZone = nullptr;
            }
          } else
            voiceManager.panic();
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::PanicLatch)) {
          voiceManager.panicLatch(); // Backward compat: old Panic Latch mapping
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary)) {
          if (isDown) {
            int groupId = midiAction.touchpadLayoutGroupId;
            int scope = midiAction.touchpadSoloScope;
            int currentLayer = getHighestActiveLayerIndex();
            juce::ScopedLock sl(stateLock);
            if (scope == 0) {
              touchpadSoloLayoutGroupGlobal = groupId;
            } else if (scope == 1 || scope == 2) {
              if (currentLayer >= 0 && currentLayer < 9) {
                touchpadSoloLayoutGroupPerLayer[(size_t)currentLayer] = groupId;
                touchpadSoloScopeForgetPerLayer[(size_t)currentLayer] = (scope == 1);
              }
            }
            sendChangeMessage();
          }
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle)) {
          if (isDown) {
            int groupId = midiAction.touchpadLayoutGroupId;
            int scope = midiAction.touchpadSoloScope;
            int currentLayer = getHighestActiveLayerIndex();
            juce::ScopedLock sl(stateLock);
            if (scope == 0) {
              touchpadSoloLayoutGroupGlobal =
                  (touchpadSoloLayoutGroupGlobal == groupId) ? 0 : groupId;
            } else if (scope == 1 || scope == 2) {
              if (currentLayer >= 0 && currentLayer < 9) {
                int &slotRef =
                    touchpadSoloLayoutGroupPerLayer[(size_t)currentLayer];
                bool wasActive = (slotRef == groupId);
                slotRef = (slotRef == groupId) ? 0 : groupId;
                touchpadSoloScopeForgetPerLayer[(size_t)currentLayer] =
                    (!wasActive && scope == 1);
              }
            }
            sendChangeMessage();
          }
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet)) {
          if (isDown) {
            int groupId = midiAction.touchpadLayoutGroupId;
            int scope = midiAction.touchpadSoloScope;
            int currentLayer = getHighestActiveLayerIndex();
            juce::ScopedLock sl(stateLock);
            if (scope == 0) {
              touchpadSoloLayoutGroupGlobal = groupId;
            } else if (scope == 1 || scope == 2) {
              if (currentLayer >= 0 && currentLayer < 9) {
                touchpadSoloLayoutGroupPerLayer[(size_t)currentLayer] = groupId;
                touchpadSoloScopeForgetPerLayer[(size_t)currentLayer] = (scope == 1);
              }
            }
            sendChangeMessage();
          }
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloClear)) {
          if (isDown) {
            int scope = midiAction.touchpadSoloScope;
            int currentLayer = getHighestActiveLayerIndex();
            juce::ScopedLock sl(stateLock);
            if (scope == 0) {
              touchpadSoloLayoutGroupGlobal = 0;
            } else if (scope == 1 || scope == 2) {
              if (currentLayer >= 0 && currentLayer < 9) {
                touchpadSoloLayoutGroupPerLayer[(size_t)currentLayer] = 0;
                touchpadSoloScopeForgetPerLayer[(size_t)currentLayer] = false;
              }
            }
            sendChangeMessage();
          }
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::Transpose) ||
                   cmd ==
                       static_cast<int>(MIDIQy::CommandID::GlobalPitchDown)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          int modify = midiAction.transposeModify;
          if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalPitchDown))
            modify = 1; // Legacy
          switch (modify) {
          case 0:
            chrom += 1;
            break; // Up 1 semitone
          case 1:
            chrom -= 1;
            break; // Down 1 semitone
          case 2:
            chrom += 12;
            break; // Up 1 octave
          case 3:
            chrom -= 12;
            break; // Down 1 octave
          case 4:
            chrom = midiAction.transposeSemitones;
            break; // Set
          default:
            break;
          }
          chrom = juce::jlimit(-48, 48, chrom);
          zoneManager.setGlobalTranspose(chrom, deg);
          // transposeLocal: placeholder – local/zone selector not implemented
          // yet
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalModeUp)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom, deg + 1);
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalModeDown)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom, deg - 1);
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootUp)) {
          int root = zoneManager.getGlobalRootNote();
          zoneManager.setGlobalRoot(juce::jlimit(0, 127, root + 1));
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootDown)) {
          int root = zoneManager.getGlobalRootNote();
          zoneManager.setGlobalRoot(juce::jlimit(0, 127, root - 1));
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootSet)) {
          zoneManager.setGlobalRoot(juce::jlimit(0, 127, midiAction.rootNote));
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::GlobalScaleNext)) {
          juce::StringArray names = scaleLibrary.getScaleNames();
          if (!names.isEmpty()) {
            juce::String current = zoneManager.getGlobalScaleName();
            int idx = names.indexOf(current);
            if (idx < 0)
              idx = 0;
            idx = (idx + 1) % names.size();
            zoneManager.setGlobalScale(names[idx]);
          }
        } else if (cmd ==
                   static_cast<int>(MIDIQy::CommandID::GlobalScalePrev)) {
          juce::StringArray names = scaleLibrary.getScaleNames();
          if (!names.isEmpty()) {
            juce::String current = zoneManager.getGlobalScaleName();
            int idx = names.indexOf(current);
            if (idx < 0)
              idx = 0;
            idx = (idx + names.size() - 1) % names.size();
            zoneManager.setGlobalScale(names[idx]);
          }
        } else if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalScaleSet)) {
          juce::StringArray names = scaleLibrary.getScaleNames();
          int idx = juce::jlimit(0, names.size() - 1, midiAction.scaleIndex);
          if (idx >= 0 && idx < names.size())
            zoneManager.setGlobalScale(names[idx]);
        }
        return;
      }

      if (midiAction.type == ActionType::Note) {
        // Handle chords from chordPool if present
        if (slot.chordIndex >= 0 &&
            slot.chordIndex < (int)ctx->chordPool.size()) {
          const auto &chordActions = ctx->chordPool[(size_t)slot.chordIndex];
          if (zone) {
            // Zone with chord: use zone's special behavior
            processZoneChord(input, zone, chordActions, midiAction);
          } else {
            // Manual mapping chord: simple playback
            std::vector<int> notes;
            std::vector<int> velocities;
            notes.reserve(chordActions.size());
            velocities.reserve(chordActions.size());
            for (const auto &a : chordActions) {
              notes.push_back(a.data1);
              velocities.push_back(
                  calculateVelocity(a.data2, a.velocityRandom));
            }
            voiceManager.noteOn(input, notes, velocities, midiAction.channel, 0,
                                true, 0, PolyphonyMode::Poly, 50);
            if (!notes.empty())
              lastTriggeredNote = notes.front();
          }
        } else {
          // Single note
          if (zone) {
            // Zone single note: use zone's special behavior
            processZoneNote(input, zone, midiAction);
          } else {
            // Manual mapping: simple playback (shared with touchpad path)
            triggerManualNoteOn(input, midiAction);
          }
        }
        return; // Stop searching lower layers
      }
    }
  } // release read lock
  // No match found in any active layer
}

std::vector<int> InputProcessor::getBufferedNotes() {
  juce::ScopedReadLock lock(bufferLock);
  return noteBuffer;
}

std::shared_ptr<const CompiledMapContext> InputProcessor::getContext() const {
  juce::ScopedReadLock rl(mapLock);
  return activeContext;
}

// Phase 50.5: Process a single note from a zone (with zone-specific behavior)
void InputProcessor::processZoneNote(InputID input, std::shared_ptr<Zone> zone,
                                     const MidiAction &action) {
  if (!zone)
    return;

  auto chordNotes = zone->getNotesForKey(
      input.keyCode, zoneManager.getGlobalChromaticTranspose(),
      zoneManager.getGlobalDegreeTranspose());
  bool allowSustain = !zone->ignoreGlobalSustain;

  if (chordNotes.has_value() && !chordNotes->empty()) {
    // Per-note velocities from base + velocity random (velocity random slider
    // controls variation)
    std::vector<int> finalNotes;
    std::vector<int> finalVelocities;
    finalNotes.reserve(chordNotes->size());
    finalVelocities.reserve(chordNotes->size());

    for (const auto &cn : *chordNotes) {
      finalNotes.push_back(cn.pitch);
      int vel = calculateVelocity(zone->baseVelocity, zone->velocityRandom);
      if (cn.isGhost) {
        int ghostVel = static_cast<int>(vel * zone->ghostVelocityScale);
        finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
      } else {
        finalVelocities.push_back(vel);
      }
    }
    if (zone->strumGhostNotes && finalVelocities.size() > 2) {
      for (size_t i = 1; i < finalVelocities.size() - 1; ++i)
        finalVelocities[i] =
            juce::jlimit(1, 127, static_cast<int>(finalVelocities[i] * 0.85f));
    }
    // Direct: send instantly (strum 0, no timing variation). Strum: use slider
    // values.
    int strumMsForCall =
        (zone->playMode == Zone::PlayMode::Direct)
            ? 0
            : ((zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50);
    int humanizeMs = (zone->playMode == Zone::PlayMode::Strum &&
                      zone->strumSpeedMs > 0 && zone->strumTimingVariationOn)
                         ? zone->strumTimingVariationMs
                         : 0;

    // Sustain mode: one-shot latch – turn off previous chord before playing new
    if (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain) {
      if (lastSustainZone == zone.get() && lastSustainChordSource.keyCode >= 0)
        voiceManager.handleKeyUp(lastSustainChordSource);
      lastSustainChordSource = input;
      lastSustainZone = zone.get();
    }

    // Normal mode with delay release and override timer: cancel old timer for
    // this zone
    if (zone->releaseBehavior == Zone::ReleaseBehavior::Normal &&
        zone->delayReleaseOn && zone->overrideTimer) {
      auto it = zoneActiveTimers.find(zone.get());
      if (it != zoneActiveTimers.end()) {
        // Cancel old timer and send immediate note-off for old input
        voiceManager.cancelPendingRelease(it->second);
        zoneActiveTimers.erase(it);
      }
      // Register this new input as the active timer for this zone
      zoneActiveTimers[zone.get()] = input;
    }

    int releaseMs = (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain)
                        ? 0
                        : (zone->delayReleaseOn ? zone->releaseDurationMs : 0);

    if (zone->playMode == Zone::PlayMode::Direct) {
      int glideSpeed = zone->glideTimeMs;
      if (zone->isAdaptiveGlide &&
          zone->polyphonyMode == PolyphonyMode::Legato) {
        rhythmAnalyzer.logTap();
        glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                     zone->maxGlideTimeMs);
      }

      if (finalNotes.size() > 1) {
        voiceManager.noteOn(input, finalNotes, finalVelocities, action.channel,
                            0, allowSustain, releaseMs, zone->polyphonyMode,
                            glideSpeed, static_cast<int>(zone->strumPattern),
                            0);
        if (!finalNotes.empty())
          lastTriggeredNote = finalNotes.front();
      } else {
        voiceManager.noteOn(input, finalNotes.front(), finalVelocities.front(),
                            action.channel, allowSustain, releaseMs,
                            zone->polyphonyMode, glideSpeed);
        lastTriggeredNote = finalNotes.front();
      }
    } else if (zone->playMode == Zone::PlayMode::Strum) {
      int glideSpeed = zone->glideTimeMs;
      if (zone->isAdaptiveGlide &&
          zone->polyphonyMode == PolyphonyMode::Legato) {
        rhythmAnalyzer.logTap();
        glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                     zone->maxGlideTimeMs);
      }

      voiceManager.handleKeyUp(lastStrumSource);
      voiceManager.noteOn(input, finalNotes, finalVelocities, action.channel,
                          strumMsForCall, allowSustain, 0, zone->polyphonyMode,
                          glideSpeed, static_cast<int>(zone->strumPattern),
                          humanizeMs);
      lastStrumSource = input;
      if (!finalNotes.empty())
        lastTriggeredNote = finalNotes.front();
      {
        juce::ScopedWriteLock bufferWriteLock(bufferLock);
        noteBuffer = finalNotes;
        bufferedStrumSpeedMs = strumMsForCall;
      }
    }
  } else {
    // Fallback: use action velocity (still respect zone polyphony mode)
    int vel = calculateVelocity(action.data2, action.velocityRandom);
    int glideSpeed = zone->glideTimeMs;
    if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
      rhythmAnalyzer.logTap();
      glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                   zone->maxGlideTimeMs);
    }
    voiceManager.noteOn(input, action.data1, vel, action.channel, allowSustain,
                        0, zone->polyphonyMode, glideSpeed);
    lastTriggeredNote = action.data1;
  }
}

// Phase 50.5: Process a chord from chordPool with zone-specific behavior
void InputProcessor::processZoneChord(
    InputID input, std::shared_ptr<Zone> zone,
    const std::vector<MidiAction> &chordActions, const MidiAction &rootAction) {
  if (!zone)
    return;

  // Reconstruct chord notes with ghost note info from zone
  auto chordNotes = zone->getNotesForKey(
      input.keyCode, zoneManager.getGlobalChromaticTranspose(),
      zoneManager.getGlobalDegreeTranspose());
  bool allowSustain = !zone->ignoreGlobalSustain;

  if (!chordNotes.has_value() || chordNotes->empty()) {
    // Fallback: use chordActions directly
    std::vector<int> notes;
    std::vector<int> velocities;
    notes.reserve(chordActions.size());
    velocities.reserve(chordActions.size());
    for (const auto &a : chordActions) {
      notes.push_back(a.data1);
      velocities.push_back(calculateVelocity(a.data2, a.velocityRandom));
    }
    voiceManager.noteOn(input, notes, velocities, rootAction.channel, 0,
                        allowSustain, 0, zone->polyphonyMode, 50);
    if (!notes.empty())
      lastTriggeredNote = notes.front();
    return;
  }

  // Per-note velocities from base + velocity random
  std::vector<int> finalNotes;
  std::vector<int> finalVelocities;
  finalNotes.reserve(chordNotes->size());
  finalVelocities.reserve(chordNotes->size());

  for (const auto &cn : *chordNotes) {
    finalNotes.push_back(cn.pitch);
    int vel = calculateVelocity(zone->baseVelocity, zone->velocityRandom);
    if (cn.isGhost) {
      int ghostVel = static_cast<int>(vel * zone->ghostVelocityScale);
      finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
    } else {
      finalVelocities.push_back(vel);
    }
  }
  if (zone->strumGhostNotes && finalVelocities.size() > 2) {
    for (size_t i = 1; i < finalVelocities.size() - 1; ++i)
      finalVelocities[i] =
          juce::jlimit(1, 127, static_cast<int>(finalVelocities[i] * 0.85f));
  }
  // Direct: send instantly (strum 0, no timing variation). Strum: use slider
  // values.
  int strumMsForCall =
      (zone->playMode == Zone::PlayMode::Direct)
          ? 0
          : ((zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50);
  int humanizeMs = (zone->playMode == Zone::PlayMode::Strum &&
                    zone->strumSpeedMs > 0 && zone->strumTimingVariationOn)
                       ? zone->strumTimingVariationMs
                       : 0;

  // Sustain mode: one-shot latch – turn off previous chord before playing new
  if (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain) {
    if (lastSustainZone == zone.get() && lastSustainChordSource.keyCode >= 0)
      voiceManager.handleKeyUp(lastSustainChordSource);
    lastSustainChordSource = input;
    lastSustainZone = zone.get();
  }

  // Normal mode with delay release and override timer: cancel old timer for
  // this zone
  if (zone->releaseBehavior == Zone::ReleaseBehavior::Normal &&
      zone->delayReleaseOn && zone->overrideTimer) {
    auto it = zoneActiveTimers.find(zone.get());
    if (it != zoneActiveTimers.end()) {
      // Cancel old timer and send immediate note-off for old input
      voiceManager.cancelPendingRelease(it->second);
      zoneActiveTimers.erase(it);
    }
    // Register this new input as the active timer for this zone
    zoneActiveTimers[zone.get()] = input;
  }

  int releaseMsChord =
      (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain)
          ? 0
          : (zone->delayReleaseOn ? zone->releaseDurationMs : 0);

  if (zone->playMode == Zone::PlayMode::Direct) {
    int glideSpeed = zone->glideTimeMs;
    if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
      rhythmAnalyzer.logTap();
      glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                   zone->maxGlideTimeMs);
    }
    voiceManager.noteOn(input, finalNotes, finalVelocities, rootAction.channel,
                        0, allowSustain, releaseMsChord, zone->polyphonyMode,
                        glideSpeed, static_cast<int>(zone->strumPattern), 0);
    if (!finalNotes.empty())
      lastTriggeredNote = finalNotes.front();
  } else if (zone->playMode == Zone::PlayMode::Strum) {
    int glideSpeed = zone->glideTimeMs;
    if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
      rhythmAnalyzer.logTap();
      glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                   zone->maxGlideTimeMs);
    }
    voiceManager.handleKeyUp(lastStrumSource);
    voiceManager.noteOn(input, finalNotes, finalVelocities, rootAction.channel,
                        strumMsForCall, allowSustain, 0, zone->polyphonyMode,
                        glideSpeed, static_cast<int>(zone->strumPattern),
                        humanizeMs);
    lastStrumSource = input;
    if (!finalNotes.empty())
      lastTriggeredNote = finalNotes.front();
    {
      juce::ScopedWriteLock bufferWriteLock(bufferLock);
      noteBuffer = finalNotes;
      bufferedStrumSpeedMs = strumMsForCall;
    }
  }
}

juce::StringArray InputProcessor::getActiveLayerNames() {
  juce::StringArray result;
  juce::ScopedLock lock(stateLock);
  for (int i = 0; i < 9; ++i) {
    bool active = (i == 0) || layerLatchedState[(size_t)i] ||
                  (layerMomentaryCounts[(size_t)i] > 0);
    if (!active)
      continue;
    juce::ValueTree layerNode = presetManager.getLayerNode(i);
    juce::String name =
        layerNode.isValid()
            ? layerNode.getProperty("name", "Layer " + juce::String(i))
                  .toString()
            : ("Layer " + juce::String(i));
    if (layerMomentaryCounts[(size_t)i] > 0)
      name += " (Hold)";
    else if (i > 0)
      name += " (On)";
    result.add(name);
  }
  return result;
}

int InputProcessor::getHighestActiveLayerIndex() const {
  juce::ScopedLock lock(stateLock);
  for (int i = 8; i >= 0; --i) {
    if (i == 0 || layerLatchedState[(size_t)i] ||
        (layerMomentaryCounts[(size_t)i] > 0))
      return i;
  }
  return 0;
}

std::optional<float>
InputProcessor::getPitchPadRelativeAnchorNormX(uintptr_t deviceHandle,
                                               int layerId, int eventId) const {
  juce::ScopedLock lock(anchorLock);
  for (const auto &pair : pitchPadRelativeAnchorT) {
    const auto &key = pair.first;
    if (std::get<0>(key) == deviceHandle && std::get<1>(key) == layerId &&
        std::get<2>(key) == eventId)
      return pair.second;
  }
  return std::nullopt;
}

std::vector<int> InputProcessor::getTouchpadMixerStripCCValues(
    uintptr_t deviceHandle, int stripIndex, int numFaders) const {
  std::vector<int> out(static_cast<size_t>(juce::jmax(0, numFaders)), 0);
  juce::ScopedReadLock rl(mixerStateLock);
  for (int i = 0; i < numFaders; ++i) {
    auto key = std::make_tuple(deviceHandle, stripIndex, i);
    auto it = lastTouchpadMixerCCValues.find(key);
    if (it != lastTouchpadMixerCCValues.end())
      out[(size_t)i] = it->second;
  }
  return out;
}

std::vector<bool> InputProcessor::getTouchpadMixerStripMuteState(
    uintptr_t deviceHandle, int stripIndex, int numFaders) const {
  std::vector<bool> out(static_cast<size_t>(juce::jmax(0, numFaders)), false);
  juce::ScopedReadLock rl(mixerStateLock);
  for (int i = 0; i < numFaders; ++i) {
    auto key = std::make_tuple(deviceHandle, stripIndex, i);
    auto it = touchpadMixerMuteState.find(key);
    if (it != touchpadMixerMuteState.end())
      out[(size_t)i] = it->second;
  }
  return out;
}

std::vector<int> InputProcessor::getTouchpadMixerStripDisplayValues(
    uintptr_t deviceHandle, int stripIndex, int numFaders) const {
  return getTouchpadMixerStripState(deviceHandle, stripIndex, numFaders)
      .displayValues;
}

InputProcessor::TouchpadMixerStripState
InputProcessor::getTouchpadMixerStripState(uintptr_t deviceHandle,
                                           int stripIndex,
                                           int numFaders) const {
  TouchpadMixerStripState out;
  out.displayValues.resize(static_cast<size_t>(juce::jmax(0, numFaders)), 0);
  out.muted.resize(static_cast<size_t>(juce::jmax(0, numFaders)), false);
  juce::ScopedReadLock rl(mixerStateLock);
  for (int i = 0; i < numFaders; ++i) {
    auto key = std::make_tuple(deviceHandle, stripIndex, i);
    bool isMuted = false;
    {
      auto itMute = touchpadMixerMuteState.find(key);
      if (itMute != touchpadMixerMuteState.end())
        isMuted = itMute->second;
    }
    out.muted[(size_t)i] = isMuted;
    if (isMuted) {
      auto itPre = touchpadMixerValueBeforeMute.find(key);
      if (itPre != touchpadMixerValueBeforeMute.end())
        out.displayValues[(size_t)i] = itPre->second;
    } else {
      auto itLast = lastTouchpadMixerCCValues.find(key);
      if (itLast != lastTouchpadMixerCCValues.end())
        out.displayValues[(size_t)i] = itLast->second;
    }
  }
  return out;
}

std::vector<InputProcessor::EffectiveContactPosition>
InputProcessor::getEffectiveContactPositions(
    uintptr_t deviceHandle,
    const std::vector<TouchpadContact> &contacts) const {
  std::vector<EffectiveContactPosition> result;
  std::shared_ptr<const CompiledMapContext> ctx;
  {
    juce::ScopedReadLock rl(mapLock);
    ctx = activeContext;
  }
  if (!ctx)
    return result;

  juce::ScopedReadLock rl(mixerStateLock);
  for (const auto &c : contacts) {
    if (!c.tipDown)
      continue;
    auto lockKey = std::make_tuple(deviceHandle, c.contactId);
    auto itLock = contactLayoutLock.find(lockKey);
    if (itLock == contactLayoutLock.end())
      continue;

    TouchpadType type = itLock->second.first;
    size_t idx = itLock->second.second;
    float left, top, right, bottom;
    if (type == TouchpadType::Mixer && idx < ctx->touchpadMixerStrips.size()) {
      const auto &s = ctx->touchpadMixerStrips[idx];
      left = s.regionLeft;
      top = s.regionTop;
      right = s.regionRight;
      bottom = s.regionBottom;
    } else if (type == TouchpadType::DrumPad &&
               idx < ctx->touchpadDrumPadStrips.size()) {
      const auto &s = ctx->touchpadDrumPadStrips[idx];
      left = s.regionLeft;
      top = s.regionTop;
      right = s.regionRight;
      bottom = s.regionBottom;
    } else if (type == TouchpadType::ChordPad &&
               idx < ctx->touchpadChordPads.size()) {
      const auto &s = ctx->touchpadChordPads[idx];
      left = s.regionLeft;
      top = s.regionTop;
      right = s.regionRight;
      bottom = s.regionBottom;
    } else {
      continue;
    }

    float ex = std::clamp(c.normX, left, right);
    float ey = std::clamp(c.normY, top, bottom);
    if (ex != c.normX || ey != c.normY)
      result.push_back({c.contactId, ex, ey});
  }
  return result;
}

bool InputProcessor::hasManualMappingForKey(int keyCode) {
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return false;

  for (int i = 8; i >= 0; --i) {
    if (!activeLayersSnapshot[(size_t)i])
      continue;

    auto grid = ctx->globalGrids[(size_t)i];
    if (grid && keyCode >= 0 && keyCode < (int)grid->size()) {
      const auto &slot = (*grid)[(size_t)keyCode];
      if (slot.isActive) {
        // Check if it's a zone by looking at visual grid
        auto visIt = ctx->visualLookup.find(0);
        if (visIt != ctx->visualLookup.end() && i < (int)visIt->second.size() &&
            visIt->second[(size_t)i]) {
          const auto &visSlot = (*visIt->second[(size_t)i])[(size_t)keyCode];
          if (visSlot.state != VisualState::Empty &&
              !visSlot.sourceName.startsWith("Zone: ")) {
            return true; // Manual mapping found
          }
        }
      }
    }
  }
  return false;
}

std::optional<ActionType> InputProcessor::getMappingType(int keyCode,
                                                         uintptr_t aliasHash) {
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return std::nullopt;

  for (int i = 8; i >= 0; --i) {
    if (!activeLayersSnapshot[(size_t)i])
      continue;

    // Check visual grid for this alias/layer to find manual mappings
    auto visIt = ctx->visualLookup.find(aliasHash);
    if (visIt == ctx->visualLookup.end() || aliasHash != 0) {
      visIt = ctx->visualLookup.find(0); // Fallback to global
    }
    if (visIt != ctx->visualLookup.end() && i < (int)visIt->second.size() &&
        visIt->second[(size_t)i]) {
      const auto &visGrid = visIt->second[(size_t)i];
      if (keyCode >= 0 && keyCode < (int)visGrid->size()) {
        const auto &visSlot = (*visGrid)[(size_t)keyCode];
        if (visSlot.state != VisualState::Empty &&
            !visSlot.sourceName.startsWith("Zone: ")) {
          // Found manual mapping - get type from audio grid
          auto grid = ctx->globalGrids[(size_t)i];
          if (grid && keyCode >= 0 && keyCode < (int)grid->size()) {
            const auto &slot = (*grid)[(size_t)keyCode];
            if (slot.isActive)
              return slot.action.type;
          }
        }
      }
    }
  }
  return std::nullopt;
}

int InputProcessor::getManualMappingCountForKey(int keyCode,
                                                uintptr_t aliasHash) const {
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return 0;

  int count = 0;
  for (int i = 0; i < 9; ++i) {
    auto visIt = ctx->visualLookup.find(aliasHash);
    if (visIt == ctx->visualLookup.end() || aliasHash != 0) {
      visIt = ctx->visualLookup.find(0); // Fallback to global
    }
    if (visIt != ctx->visualLookup.end() && i < (int)visIt->second.size() &&
        visIt->second[(size_t)i]) {
      const auto &visGrid = visIt->second[(size_t)i];
      if (keyCode >= 0 && keyCode < (int)visGrid->size()) {
        const auto &visSlot = (*visGrid)[(size_t)keyCode];
        if (visSlot.state != VisualState::Empty &&
            !visSlot.sourceName.startsWith("Zone: ")) {
          count++;
        }
      }
    }
  }
  return count;
}

void InputProcessor::forceRebuildMappings() {
  rebuildGrid();
  applySustainDefaultFromPreset();
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash,
                                               int keyCode) {
  SimulationResult result;
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return result;

  if (!settingsManager.isStudioMode())
    viewDeviceHash = 0;

  int genericKey = getGenericKey(keyCode);
  if (keyCode < 0 || keyCode > 0xFF)
    return result;

  auto visIt = ctx->visualLookup.find(viewDeviceHash);
  if (visIt == ctx->visualLookup.end() || viewDeviceHash != 0)
    visIt = ctx->visualLookup.find(0);
  if (visIt == ctx->visualLookup.end())
    return result;

  const auto &layerVec = visIt->second;

  for (int i = 8; i >= 0; --i) {
    if (!activeLayersSnapshot[(size_t)i])
      continue;

    if (i >= (int)layerVec.size() || !layerVec[(size_t)i])
      continue;

    const auto &visGrid = layerVec[(size_t)i];
    if (keyCode >= (int)visGrid->size())
      continue;

    const auto &visSlot = (*visGrid)[(size_t)keyCode];
    if (visSlot.state == VisualState::Empty) {
      // Try generic key fallback
      if (genericKey != 0 && genericKey < (int)visGrid->size()) {
        const auto &genSlot = (*visGrid)[(size_t)genericKey];
        if (genSlot.state != VisualState::Empty) {
          // Get action from audio grid
          auto audioGrid = ctx->globalGrids[(size_t)i];
          if (audioGrid && genericKey < (int)audioGrid->size()) {
            const auto &audioSlot = (*audioGrid)[(size_t)genericKey];
            if (audioSlot.isActive) {
              result.action = audioSlot.action;
              result.isZone = genSlot.sourceName.startsWith("Zone: ");
              result.sourceName = genSlot.sourceName;
              result.state = genSlot.state;
              result.updateLegacyFields();
              return result;
            }
          }
        }
      }
      continue;
    }

    // Found a match - get action from audio grid
    auto audioGrid = ctx->globalGrids[(size_t)i];
    if (audioGrid && keyCode < (int)audioGrid->size()) {
      const auto &audioSlot = (*audioGrid)[(size_t)keyCode];
      if (audioSlot.isActive) {
        result.action = audioSlot.action;
        result.isZone = visSlot.sourceName.startsWith("Zone: ");
        result.sourceName = visSlot.sourceName;
        result.state = visSlot.state;
        result.updateLegacyFields();
        return result;
      }
    }
  }

  return result;
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash,
                                               int keyCode, int targetLayerId) {
  SimulationResult result;
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return result;

  if (!settingsManager.isStudioMode())
    viewDeviceHash = 0;

  if (targetLayerId < 0 || targetLayerId >= 9)
    targetLayerId = 8;

  int genericKey = getGenericKey(keyCode);
  if (keyCode < 0 || keyCode > 0xFF)
    return result;

  // Find the visual grid for this alias
  auto visIt = ctx->visualLookup.find(viewDeviceHash);
  if (visIt == ctx->visualLookup.end() || viewDeviceHash != 0) {
    visIt = ctx->visualLookup.find(0); // Fallback to global
  }

  if (visIt == ctx->visualLookup.end())
    return result;

  const auto &layerVec = visIt->second;

  // Phase 50.5: Search from targetLayerId down to 0 using visual grid
  for (int i = targetLayerId; i >= 0; --i) {
    if (i >= (int)layerVec.size() || !layerVec[(size_t)i])
      continue;

    const auto &visGrid = layerVec[(size_t)i];
    if (keyCode >= (int)visGrid->size())
      continue;

    const auto &visSlot = (*visGrid)[(size_t)keyCode];
    if (visSlot.state == VisualState::Empty) {
      // Try generic key fallback
      if (genericKey != 0 && genericKey < (int)visGrid->size()) {
        const auto &genSlot = (*visGrid)[(size_t)genericKey];
        if (genSlot.state != VisualState::Empty) {
          // Get action from audio grid
          auto audioGrid = ctx->globalGrids[(size_t)i];
          if (audioGrid && genericKey < (int)audioGrid->size()) {
            const auto &audioSlot = (*audioGrid)[(size_t)genericKey];
            if (audioSlot.isActive) {
              result.action = audioSlot.action;
              result.isZone = genSlot.sourceName.startsWith("Zone: ");
              result.sourceName = genSlot.sourceName;
              result.state = (i == targetLayerId ? VisualState::Active
                                                 : VisualState::Inherited);
              result.updateLegacyFields();
              return result;
            }
          }
        }
      }
      continue;
    }

    // Found a match - get action from audio grid
    auto audioGrid = ctx->globalGrids[(size_t)i];
    if (audioGrid && keyCode < (int)audioGrid->size()) {
      const auto &audioSlot = (*audioGrid)[(size_t)keyCode];
      if (audioSlot.isActive) {
        result.action = audioSlot.action;
        result.isZone = visSlot.sourceName.startsWith("Zone: ");
        result.sourceName = visSlot.sourceName;
        result.state =
            (i == targetLayerId ? VisualState::Active : VisualState::Inherited);
        result.updateLegacyFields();
        return result;
      }
    }
  }

  return result;
}

void InputProcessor::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                     float value) {
  InputID input = {deviceHandle, inputCode};
  auto opt = lookupActionInGrid(input);
  if (!opt || opt->type != ActionType::Expression ||
      opt->adsrSettings.target != AdsrTarget::CC)
    return;
  const MidiAction &action = *opt;

  float currentVal = 0.0f;
  // Note: Scroll is now handled as discrete key events (ScrollUp/ScrollDown)
  // so this method only handles pointer X/Y axis events
  bool isRelative = false; // Pointer events are absolute

  if (isRelative) {
    juce::ScopedWriteLock lock(mapLock);
    auto it = currentCCValues.find(input);
    if (it != currentCCValues.end())
      currentVal = it->second;
    const float sensitivity = 1.0f;
    currentVal = std::clamp(currentVal + (value * sensitivity), 0.0f, 127.0f);
    currentCCValues[input] = currentVal;
  } else {
    // Absolute (Pointer): map 0.0-1.0 to 0-127
    currentVal = value * 127.0f;
  }

  int ccValue =
      static_cast<int>(std::round(std::clamp(currentVal, 0.0f, 127.0f)));

  voiceManager.sendCC(action.channel, action.adsrSettings.ccNumber, ccValue);
}

bool InputProcessor::hasPointerMappings() {
  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }
  juce::ScopedReadLock lock(mapLock);
  auto ctx = activeContext;
  if (!ctx)
    return false;

  // Check for touchpad mappings (Finger1X, Finger1Y, etc.)
  for (const auto &entry : ctx->touchpadMappings) {
    if (activeLayersSnapshot[(size_t)entry.layerId]) {
      // Found at least one touchpad mapping in an active layer
      return true;
    }
  }

  // Check for touchpad mixer layouts (CC faders)
  for (const auto &strip : ctx->touchpadMixerStrips) {
    if (activeLayersSnapshot[(size_t)strip.layerId]) {
      return true;
    }
  }

  // Check for touchpad drum pad / harmonic grid layouts
  for (const auto &strip : ctx->touchpadDrumPadStrips) {
    if (activeLayersSnapshot[(size_t)strip.layerId]) {
      return true;
    }
  }

  // Check for Chord Pad layouts
  for (const auto &cp : ctx->touchpadChordPads) {
    if (activeLayersSnapshot[(size_t)cp.layerId]) {
      return true;
    }
  }

  // Also check for legacy PointerX/PointerY mappings in grids
  for (int i = 8; i >= 0; --i) {
    if (!activeLayersSnapshot[(size_t)i])
      continue;

    auto grid = ctx->globalGrids[(size_t)i];
    if (!grid)
      continue;

    // Check for PointerX (0x2000) and PointerY (0x2001)
    if ((0x2000 < (int)grid->size() && (*grid)[0x2000].isActive) ||
        (0x2001 < (int)grid->size() && (*grid)[0x2001].isActive)) {
      return true;
    }
  }
  return false;
}

bool InputProcessor::hasTouchpadLayouts() const {
  juce::ScopedReadLock rl(mapLock);
  return activeContext &&
         (!activeContext->touchpadMixerStrips.empty() ||
          !activeContext->touchpadDrumPadStrips.empty() ||
          !activeContext->touchpadChordPads.empty());
}

void InputProcessor::processTouchpadContacts(
    uintptr_t deviceHandle, const std::vector<TouchpadContact> &contacts) {
  if (!settingsManager.isMidiModeActive())
    return;

  std::array<bool, 9> activeLayersSnapshot{};
  {
    juce::ScopedLock sl(stateLock);
    for (int i = 0; i < 9; ++i)
      activeLayersSnapshot[(size_t)i] = (i == 0) ||
                                        layerLatchedState[(size_t)i] ||
                                        (layerMomentaryCounts[(size_t)i] > 0);
  }

  std::shared_ptr<const CompiledMapContext> ctx;
  {
    juce::ScopedReadLock rl(mapLock);
    ctx = activeContext;
  }
  if (!ctx)
    return;

  TouchpadPrevState &prev = touchpadPrevState[deviceHandle];
  bool tip1 = false, tip2 = false;
  float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
  // Finger 1/2 are taken by array index (contacts[0], contacts[1]) so that
  // continuous mappings (e.g. PitchBend on Finger1X) receive updated positions
  // every frame regardless of driver contactId. The accumulator keeps contact
  // order stable, so the same physical finger stays at the same index.
  int idxFinger1 = -1;
  int idxFinger2 = -1;
  if (contacts.size() >= 1) {
    idxFinger1 = 0;
    tip1 = contacts[0].tipDown;
    x1 = contacts[0].normX;
    y1 = contacts[0].normY;
  }
  if (contacts.size() >= 2) {
    idxFinger2 = 1;
    tip2 = contacts[1].tipDown;
    x2 = contacts[1].normX;
    y2 = contacts[1].normY;
  }

  float dist = 0.0f, avgX = x1, avgY = y1;
  if (tip1 && tip2) {
    float dx = x2 - x1, dy = y2 - y1;
    dist = std::sqrt(dx * dx + dy * dy);
    avgX = (x1 + x2) * 0.5f;
    avgY = (y1 + y2) * 0.5f;
  }

  bool finger1Down = tip1 && !prev.tip1;
  bool finger1Up = !tip1 && prev.tip1;
  bool finger2Down = tip2 && !prev.tip2;
  bool finger2Up = !tip2 && prev.tip2;

  // Helper: find first layout in touchpadLayoutOrder whose region contains
  // (nx,ny)
  auto findLayoutForPoint =
      [&](float nx,
          float ny) -> std::optional<std::pair<TouchpadType, size_t>> {
    for (size_t i = 0; i < ctx->touchpadLayoutOrder.size(); ++i) {
      const auto &ref = ctx->touchpadLayoutOrder[i];
      if (ref.type == TouchpadType::Mixer &&
          ref.index < ctx->touchpadMixerStrips.size()) {
        const auto &s = ctx->touchpadMixerStrips[ref.index];
        if (!activeLayersSnapshot[(size_t)s.layerId])
          continue;
        int soloGroup =
            getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, s.layerId));
        // Hide grouped layouts when no solo is active, or hide non-matching layouts when solo is active
        if ((soloGroup == 0 && s.layoutGroupId != 0) || (soloGroup > 0 && s.layoutGroupId != soloGroup))
          continue;
        if (nx >= s.regionLeft && nx < s.regionRight && ny >= s.regionTop &&
            ny < s.regionBottom)
          return {{TouchpadType::Mixer, ref.index}};
      } else if (ref.type == TouchpadType::DrumPad &&
                 ref.index < ctx->touchpadDrumPadStrips.size()) {
        const auto &s = ctx->touchpadDrumPadStrips[ref.index];
        if (!activeLayersSnapshot[(size_t)s.layerId] || s.numPads <= 0)
          continue;
        int soloGroup =
            getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, s.layerId));
        // Hide grouped layouts when no solo is active, or hide non-matching layouts when solo is active
        if ((soloGroup == 0 && s.layoutGroupId != 0) || (soloGroup > 0 && s.layoutGroupId != soloGroup))
          continue;
        if (nx >= s.regionLeft && nx < s.regionRight && ny >= s.regionTop &&
            ny < s.regionBottom)
          return {{TouchpadType::DrumPad, ref.index}};
      } else if (ref.type == TouchpadType::ChordPad &&
                 ref.index < ctx->touchpadChordPads.size()) {
        const auto &s = ctx->touchpadChordPads[ref.index];
        if (!activeLayersSnapshot[(size_t)s.layerId])
          continue;
        int soloGroup =
            getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, s.layerId));
        // Hide grouped layouts when no solo is active, or hide non-matching layouts when solo is active
        if ((soloGroup == 0 && s.layoutGroupId != 0) || (soloGroup > 0 && s.layoutGroupId != soloGroup))
          continue;
        if (nx >= s.regionLeft && nx < s.regionRight && ny >= s.regionTop &&
            ny < s.regionBottom)
          return {{TouchpadType::ChordPad, ref.index}};
      }
    }
    return std::nullopt;
  };

  // Helper: does layout have region lock enabled?
  auto layoutHasRegionLock = [&](TouchpadType type, size_t idx) -> bool {
    if (type == TouchpadType::Mixer && idx < ctx->touchpadMixerStrips.size())
      return ctx->touchpadMixerStrips[idx].regionLock;
    if (type == TouchpadType::DrumPad &&
        idx < ctx->touchpadDrumPadStrips.size())
      return ctx->touchpadDrumPadStrips[idx].regionLock;
    if (type == TouchpadType::ChordPad &&
        idx < ctx->touchpadChordPads.size())
      return ctx->touchpadChordPads[idx].regionLock;
    return false;
  };

  // Region lock: add on first touch (in layout with regionLock), remove on
  // lift. Also build layoutPerContact (use locked layout when in
  // contactLayoutLock).
  std::vector<std::optional<std::pair<TouchpadType, size_t>>> layoutPerContact;
  layoutPerContact.reserve(contacts.size());
  {
    juce::ScopedWriteLock wl(mixerStateLock);
    for (size_t i = 0; i < contacts.size(); ++i) {
      const auto &c = contacts[i];
      auto lockKey = std::make_tuple(deviceHandle, c.contactId);
      if (c.tipDown) {
        if (contactLayoutLock.find(lockKey) == contactLayoutLock.end()) {
          auto layout = findLayoutForPoint(c.normX, c.normY);
          if (layout && layoutHasRegionLock(layout->first, layout->second))
            contactLayoutLock[lockKey] = *layout;
        }
      } else {
        contactLayoutLock.erase(lockKey);
      }
      auto itLock = contactLayoutLock.find(lockKey);
      if (itLock != contactLayoutLock.end())
        layoutPerContact.push_back(itLock->second);
      else
        layoutPerContact.push_back(findLayoutForPoint(c.normX, c.normY));
    }
  }

  // When finger is in any layout region (mixer or drum), skip Finger1Down/
  // Finger2Down Note mappings so the layout owns that finger.
  // Clear per-contact mapping lock when a contact lifts (same semantics as
  // contactLayoutLock for layouts).
  for (size_t i = 0; i < contacts.size(); ++i) {
    if (!contacts[i].tipDown)
      contactMappingLock.erase(
          std::make_tuple(deviceHandle, contacts[i].contactId));
  }

  // Per-mapping contact list and local finger state (like layouts: each
  // mapping counts only contacts in its region or locked to it).
  struct MappingLocalState {
    std::vector<std::pair<size_t, const TouchpadContact *>> inRegion;
    float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, avgX = 0.0f, avgY = 0.0f,
        dist = 0.0f;
    bool tip1 = false, tip2 = false;
    size_t idxContact1 = SIZE_MAX, idxContact2 = SIZE_MAX;
  };
  auto buildMappingContactList =
      [&](const TouchpadMappingEntry &entry, size_t mapIdx) -> MappingLocalState {
    MappingLocalState out;
    const bool fullPad = !touchpadMappingHasRegion(entry);
    for (size_t i = 0; i < contacts.size(); ++i) {
      const auto &c = contacts[i];
      bool inRegion;
      if (fullPad) {
        inRegion = true; // Full pad: every contact belongs to this mapping
      } else {
        // Explicit region: inclusive bounds so 1.0 is inside
        inRegion =
            (c.normX >= entry.regionLeft && c.normX <= entry.regionRight &&
             c.normY >= entry.regionTop && c.normY <= entry.regionBottom);
      }
      auto lockKey = std::make_tuple(deviceHandle, c.contactId);
      bool lockedToThis = false;
      {
        auto it = contactMappingLock.find(lockKey);
        lockedToThis =
            (it != contactMappingLock.end() && it->second == mapIdx);
      }
      if (inRegion || lockedToThis) {
        out.inRegion.push_back({i, &c});
        if (inRegion && entry.regionLock && c.tipDown)
          contactMappingLock[lockKey] = mapIdx;
      }
    }
    std::sort(out.inRegion.begin(), out.inRegion.end(),
              [](const auto &a, const auto &b) {
                return a.second->contactId < b.second->contactId;
              });
    if (out.inRegion.size() >= 1) {
      const auto *c1 = out.inRegion[0].second;
      out.idxContact1 = out.inRegion[0].first;
      out.tip1 = c1->tipDown;
      float nx1 = c1->normX, ny1 = c1->normY;
      if (entry.regionLock) {
        auto it = contactMappingLock.find(
            std::make_tuple(deviceHandle, c1->contactId));
        if (it != contactMappingLock.end() && it->second == mapIdx) {
          nx1 = std::clamp(nx1, entry.regionLeft, entry.regionRight);
          ny1 = std::clamp(ny1, entry.regionTop, entry.regionBottom);
        }
      }
      out.x1 = nx1;
      out.y1 = ny1;
      out.avgX = nx1;
      out.avgY = ny1;
    }
    if (out.inRegion.size() >= 2) {
      const auto *c2 = out.inRegion[1].second;
      out.idxContact2 = out.inRegion[1].first;
      out.tip2 = c2->tipDown;
      float nx2 = c2->normX, ny2 = c2->normY;
      if (entry.regionLock) {
        auto it = contactMappingLock.find(
            std::make_tuple(deviceHandle, c2->contactId));
        if (it != contactMappingLock.end() && it->second == mapIdx) {
          nx2 = std::clamp(nx2, entry.regionLeft, entry.regionRight);
          ny2 = std::clamp(ny2, entry.regionTop, entry.regionBottom);
        }
      }
      out.x2 = nx2;
      out.y2 = ny2;
      out.avgX = (out.x1 + out.x2) * 0.5f;
      out.avgY = (out.y1 + out.y2) * 0.5f;
      float dx = out.x2 - out.x1, dy = out.y2 - out.y1;
      out.dist = std::sqrt(dx * dx + dy * dy);
    }
    return out;
  };

  if (!ctx->touchpadMappings.empty()) {
    for (size_t mapIdx = 0; mapIdx < ctx->touchpadMappings.size(); ++mapIdx) {
      const auto &entry = ctx->touchpadMappings[mapIdx];
      if (!activeLayersSnapshot[(size_t)entry.layerId])
        continue;
      if (entry.conversionKind == TouchpadConversionKind::EncoderCC)
        continue;

      // Layout group solo: apply the same visibility rules as touchpad layouts
      int soloGroup =
          getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, entry.layerId));
      if ((soloGroup == 0 && entry.layoutGroupId != 0) ||
          (soloGroup > 0 && entry.layoutGroupId != soloGroup))
        continue;

      MappingLocalState local = buildMappingContactList(entry, mapIdx);
      // Skip if mapping has a region and no contacts belong to it
      if (touchpadMappingHasRegion(entry) && local.inRegion.empty())
        continue;

      // Per-mapping finger 1/2 and edge detection
      auto mapPrevKey = std::make_tuple(deviceHandle, static_cast<int>(mapIdx));
      bool prevTip1 = false, prevTip2 = false;
      {
        auto itPrev = touchpadMappingPrevState.find(mapPrevKey);
        if (itPrev != touchpadMappingPrevState.end()) {
          prevTip1 = itPrev->second.tip1;
          prevTip2 = itPrev->second.tip2;
        }
      }
      bool localFinger1Down = local.tip1 && !prevTip1;
      bool localFinger1Up = !local.tip1 && prevTip1;
      bool localFinger2Down = local.tip2 && !prevTip2;
      bool localFinger2Up = !local.tip2 && prevTip2;
      touchpadMappingPrevState[mapPrevKey] = {local.tip1, local.tip2};

      bool layoutConsumesLocalFinger1 =
          local.tip1 &&
          (local.idxContact1 < layoutPerContact.size() &&
           layoutPerContact[local.idxContact1].has_value());
      bool layoutConsumesLocalFinger2 =
          local.tip2 &&
          (local.idxContact2 < layoutPerContact.size() &&
           layoutPerContact[local.idxContact2].has_value());

      bool boolVal = false;
      float continuousVal = 0.0f;
      switch (entry.eventId) {
      case TouchpadEvent::Finger1Down:
        boolVal = localFinger1Down;
        break;
      case TouchpadEvent::Finger1Up:
        boolVal = localFinger1Up;
        break;
      case TouchpadEvent::Finger1X:
        continuousVal = local.x1;
        break;
      case TouchpadEvent::Finger1Y:
        continuousVal = local.y1;
        break;
      case TouchpadEvent::Finger2Down:
        boolVal = localFinger2Down;
        break;
      case TouchpadEvent::Finger2Up:
        boolVal = localFinger2Up;
        break;
      case TouchpadEvent::Finger2X:
        continuousVal = local.x2;
        break;
      case TouchpadEvent::Finger2Y:
        continuousVal = local.y2;
        break;
      case TouchpadEvent::Finger1And2Dist:
        continuousVal = local.dist;
        break;
      case TouchpadEvent::Finger1And2AvgX:
        continuousVal = local.avgX;
        break;
      case TouchpadEvent::Finger1And2AvgY:
        continuousVal = local.avgY;
        break;
      default:
        continue;
      }

      const auto &act = entry.action;
      const auto &p = entry.conversionParams;
      auto key = std::make_tuple(deviceHandle, entry.layerId, entry.eventId);

      switch (entry.conversionKind) {
      case TouchpadConversionKind::BoolToGate:
        if (act.type == ActionType::Note) {
          bool isDownEvent = (entry.eventId == TouchpadEvent::Finger1Down ||
                              entry.eventId == TouchpadEvent::Finger2Down);
          bool layoutConsumes =
              (entry.eventId == TouchpadEvent::Finger1Down &&
               layoutConsumesLocalFinger1) ||
              (entry.eventId == TouchpadEvent::Finger2Down &&
               layoutConsumesLocalFinger2);
          if (isDownEvent && layoutConsumes)
            continue; // Layout (mixer/drum) owns finger; skip fixed-note mapping

          // Track note state: check if note is currently active
          bool noteIsActive =
              (touchpadNoteOnSent.find(key) != touchpadNoteOnSent.end());

          bool releaseThisFrame =
              (entry.eventId == TouchpadEvent::Finger1Down && localFinger1Up) ||
              (entry.eventId == TouchpadEvent::Finger2Down && localFinger2Up);
          InputID touchpadInput{deviceHandle, 0};
          
          // Check hold behavior: if "Ignore, send note off immediately", send note off right after note on
          bool shouldSendNoteOffImmediately = 
              (act.touchpadHoldBehavior == TouchpadHoldBehavior::IgnoreSendNoteOffImmediately);
          
          if (isDownEvent && boolVal) {
            // Finger down: trigger note on (only if not already active)
            if (!noteIsActive) {
              triggerManualNoteOn(touchpadInput, act,
                                  true); // Latch applies to Down
              touchpadNoteOnSent.insert(key);
              
              // If hold behavior is "Ignore, send note off immediately", send note off right away
              if (shouldSendNoteOffImmediately) {
                triggerManualNoteRelease(touchpadInput, act);
                touchpadNoteOnSent.erase(key);
              }
            }
          } else if (releaseThisFrame && noteIsActive) {
            // Finger released: trigger note off only if note was active
            // Only send note off if hold behavior is NOT "Ignore" (since we already sent it)
            if (!shouldSendNoteOffImmediately) {
              triggerManualNoteRelease(touchpadInput, act);
            }
            touchpadNoteOnSent.erase(key);
          } else if ((entry.eventId == TouchpadEvent::Finger1Up || entry.eventId == TouchpadEvent::Finger2Up) && boolVal) {
            // Finger1Up/Finger2Up: trigger note when finger lifts (one-shot);
            // no Send Note Off, Latch only applies to Down so pass false
            // Only fire for explicit Finger1Up/Finger2Up mappings, not as side effect of Finger1Down release
            triggerManualNoteOn(touchpadInput, act, false);
          }
        } else if (act.type == ActionType::Command && boolVal) {
          int cmd = act.data1;
          if (cmd == static_cast<int>(MIDIQy::CommandID::SustainMomentary))
            voiceManager.setSustain(true);
          else if (cmd == static_cast<int>(MIDIQy::CommandID::SustainToggle))
            voiceManager.setSustain(!voiceManager.isSustainActive());
          else if (cmd == static_cast<int>(MIDIQy::CommandID::Panic))
            voiceManager.panic();
          else if (cmd == static_cast<int>(MIDIQy::CommandID::PanicLatch))
            voiceManager.panicLatch();
        }
        break;
      case TouchpadConversionKind::BoolToCC:
        if (act.type == ActionType::Expression) {
          const bool isCcTarget = (act.adsrSettings.target == AdsrTarget::CC);
          const bool isLatchMode =
              (p.ccReleaseBehavior == CcReleaseBehavior::AlwaysLatch);

          if (!isLatchMode) {
            // Existing behaviour: trigger Expression envelope and rely on
            // touchpadExpressionActive / releaseEnvelope to send the release
            // value when the finger lifts.
            if (boolVal) {
              InputID touchpadExprInput{deviceHandle,
                                        static_cast<int>(entry.eventId)};
              int peakValue;
              if (act.adsrSettings.target == AdsrTarget::CC) {
                peakValue = act.adsrSettings.valueWhenOn;
              } else if (act.adsrSettings.target == AdsrTarget::PitchBend) {
                double stepsPerSemitone =
                    settingsManager.getStepsPerSemitone();
                peakValue = static_cast<int>(
                    8192.0 + (act.data2 * stepsPerSemitone));
                peakValue = juce::jlimit(0, 16383, peakValue);
              } else {
                if (!act.smartBendLookup.empty() && lastTriggeredNote >= 0 &&
                    lastTriggeredNote < 128)
                  peakValue = act.smartBendLookup[lastTriggeredNote];
                else
                  peakValue = 8192;
              }
              expressionEngine.triggerEnvelope(touchpadExprInput, act.channel,
                                               act.adsrSettings, peakValue);
              touchpadExpressionActive.insert(
                  std::make_tuple(deviceHandle, entry.layerId, entry.eventId));
            }
          } else if (isCcTarget) {
            // Latch mode for CC Position: first press sends valueWhenOn,
            // second press sends valueWhenOff, releases do nothing.
            if (boolVal) {
              auto latchKey =
                  std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                                  act.channel, act.adsrSettings.ccNumber);
              const bool currentlyLatched =
                  touchpadCcLatchedOn.find(latchKey) !=
                  touchpadCcLatchedOn.end();

              const int valueToSend = currentlyLatched
                                          ? act.adsrSettings.valueWhenOff
                                          : act.adsrSettings.valueWhenOn;

              voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber,
                                  valueToSend);

              if (currentlyLatched)
                touchpadCcLatchedOn.erase(latchKey);
              else
                touchpadCcLatchedOn.insert(latchKey);
            }
          }
        }
        break;
      case TouchpadConversionKind::ContinuousToGate: {
        bool above = continuousVal >= p.threshold;
        bool trigger = p.triggerAbove ? above : !above;
        InputID touchpadInput{deviceHandle, 0};
        if (trigger) {
          if (touchpadNoteOnSent.find(key) == touchpadNoteOnSent.end()) {
            touchpadNoteOnSent.insert(key);
            triggerManualNoteOn(touchpadInput, act);
          }
        } else {
          if (touchpadNoteOnSent.erase(key))
            triggerManualNoteRelease(touchpadInput, act);
        }
        break;
      }
      case TouchpadConversionKind::ContinuousToRange:
        if (act.type == ActionType::Expression) {
          // Determine whether this continuous event is currently active based
          // on this mapping's local finger state.
          bool eventActive = true;
          switch (entry.eventId) {
          case TouchpadEvent::Finger1X:
          case TouchpadEvent::Finger1Y:
          case TouchpadEvent::Finger1And2AvgX:
          case TouchpadEvent::Finger1And2AvgY:
            eventActive = local.tip1;
            break;
          case TouchpadEvent::Finger2X:
          case TouchpadEvent::Finger2Y:
            eventActive = local.tip2;
            break;
          case TouchpadEvent::Finger1And2Dist:
            eventActive = local.tip1 && local.tip2;
            break;
          default:
            break;
          }

          const int ccNumberOrMinusOne =
              (act.adsrSettings.target == AdsrTarget::CC)
                  ? act.adsrSettings.ccNumber
                  : -1;
          auto keyCont =
              std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                              act.channel, ccNumberOrMinusOne);

          // If the event is no longer active and we previously sent a value,
          // send the configured release value (for CC) or center pitch (for PB)
          // when requested, then clear the last-value entry so this happens
          // only once.
          if (!eventActive) {
            auto itLast = lastTouchpadContinuousValues.find(keyCont);
            if (itLast != lastTouchpadContinuousValues.end()) {
              const bool isPB =
                  (act.adsrSettings.target == AdsrTarget::PitchBend ||
                   act.adsrSettings.target == AdsrTarget::SmartScaleBend);
              if (isPB && entry.touchGlideMs > 0 && act.sendReleaseValue) {
                // Touch glide: transition to center over duration; timer will send.
                auto &g = touchpadPitchGlideState[keyCont];
                g.phase = TouchGlidePhase::GlidingToCenter;
                g.startValue = itLast->second;
                g.targetValue = 8192;
                g.startTimeMs = static_cast<uint32_t>(juce::Time::getMillisecondCounter());
                g.durationMs = entry.touchGlideMs;
                g.lastSentValue = itLast->second;
                lastTouchpadContinuousValues.erase(itLast);
                startTouchGlideTimerIfNeeded();
              } else {
                if (act.sendReleaseValue) {
                  if (act.adsrSettings.target == AdsrTarget::CC) {
                    voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber,
                                        act.releaseValue);
                  } else if (act.adsrSettings.target == AdsrTarget::PitchBend ||
                             act.adsrSettings.target ==
                                 AdsrTarget::SmartScaleBend) {
                    voiceManager.sendPitchBend(act.channel, 8192);
                  }
                }
                lastTouchpadContinuousValues.erase(itLast);
              }
            }
            break;
          }

          float t = (continuousVal - p.inputMin) * p.invInputRange;
          t = std::clamp(t, 0.0f, 1.0f);

          float outVal = static_cast<float>(p.outputMin) +
                         static_cast<float>(p.outputMax - p.outputMin) * t;
          float stepOffset = 0.0f;

          // If a pitch-pad config is present and this is a pitch-based target,
          // use the shared pitch-pad layout to derive the discrete step from X
          // instead of direct linear min/max mapping.
          if (p.pitchPadConfig.has_value() &&
              (act.adsrSettings.target == AdsrTarget::PitchBend ||
               act.adsrSettings.target == AdsrTarget::SmartScaleBend)) {
            const PitchPadConfig &cfg = *p.pitchPadConfig;
            const PitchPadLayout &layout = p.cachedPitchPadLayout
                                               ? *p.cachedPitchPadLayout
                                               : buildPitchPadLayout(cfg);

            auto relKey = std::make_tuple(deviceHandle, entry.layerId,
                                          entry.eventId, act.channel);

            if (cfg.mode == PitchPadMode::Relative) {
              // Relative mode: anchor point (where user first touches) becomes
              // PB zero. Movement from anchor uses the SAME pitch-pad layout as
              // Absolute, just re-centered at the anchor.
              bool startGesture = false;
              if (entry.eventId == TouchpadEvent::Finger1X ||
                  entry.eventId == TouchpadEvent::Finger1Y ||
                  entry.eventId == TouchpadEvent::Finger1And2Dist ||
                  entry.eventId == TouchpadEvent::Finger1And2AvgX ||
                  entry.eventId == TouchpadEvent::Finger1And2AvgY) {
                startGesture = localFinger1Down;
              } else if (entry.eventId == TouchpadEvent::Finger2X ||
                         entry.eventId == TouchpadEvent::Finger2Y) {
                startGesture = localFinger2Down;
              }

              if (startGesture) {
                // Store anchor X position and the absolute step it maps to.
                {
                  juce::ScopedLock al(anchorLock);
                  pitchPadRelativeAnchorT[relKey] = t;
                  float anchorXClamped = juce::jlimit(0.0f, 1.0f, t);
                  PitchSample anchorSample = mapXToStep(layout, anchorXClamped);
                  pitchPadRelativeAnchorStep[relKey] = anchorSample.step;
                }
              }

              float anchorStepVal = 0.0f;
              bool hasAnchor = false;
              {
                juce::ScopedLock al(anchorLock);
                auto itAnchorStep = pitchPadRelativeAnchorStep.find(relKey);
                if (itAnchorStep != pitchPadRelativeAnchorStep.end()) {
                  anchorStepVal = itAnchorStep->second;
                  hasAnchor = true;
                }
              }
              if (hasAnchor) {
                float xClamped = juce::jlimit(0.0f, 1.0f, t);
                PitchSample sample = mapXToStep(layout, xClamped);
                stepOffset = sample.step - anchorStepVal;
              } else {
                stepOffset = 0.0f;
              }
            } else {
              // Absolute mode: use normalized coordinate directly. zeroStep is
              // the configured center (currently 0.0f so middle of range).
              float xClamped = juce::jlimit(0.0f, 1.0f, t);
              PitchSample sample = mapXToStep(layout, xClamped);
              stepOffset = sample.step - cfg.zeroStep;
            }
          } else {
            // No pitch-pad config: fall back to simple linear mapping of the
            // normalized touch value into the configured output step range.
            stepOffset = juce::jmap(
                t, static_cast<float>(p.outputMin),
                static_cast<float>(p.outputMax)); // may be fractional
          }

          // Change-only sending: only send CC/PB when the quantized value
          // changes for this (device, layer, eventId, channel, ccNumber/-1 for
          // PB).
          if (act.adsrSettings.target == AdsrTarget::CC) {
            int ccVal =
                juce::jlimit(0, 127, static_cast<int>(std::round(outVal)));
            auto itLast = lastTouchpadContinuousValues.find(keyCont);
            if (itLast != lastTouchpadContinuousValues.end() &&
                itLast->second == ccVal) {
              break; // No CC change – skip send
            }
            voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, ccVal);
            lastTouchpadContinuousValues[keyCont] = ccVal;
          } else {
            // Pitch-based targets: interpret the (possibly fractional) step
            // offset and convert to a PB value.
            int pbRange = juce::jmax(1, settingsManager.getPitchBendRange());
            int pbVal = 8192;

            if (act.adsrSettings.target == AdsrTarget::SmartScaleBend) {
              // SmartScaleBend: stepOffset is SMART SCALE DISTANCE (scale
              // steps). Same semantics as keyboard SmartScaleBend: e.g. C
              // major, press C, bend down 1 = B, bend down 2 = A; bend up 1 =
              // D, bend up 2 = E. Do not clamp stepOffset by semitone range;
              // pass scale steps through. smartStepOffsetToPitchBend clips the
              // final PB to [0,16383] and respects global PB range.
              // Use global scale or per-mapping scale depending on entry.smartScaleFollowGlobal.
              int currentNote = voiceManager.getCurrentPlayingNote(act.channel);
              if (currentNote < 0) {
                currentNote = lastTriggeredNote;
              }
              if (currentNote >= 0 && currentNote < 128) {
                std::vector<int> intervals =
                    entry.smartScaleFollowGlobal
                        ? zoneManager.getGlobalScaleIntervals()
                        : scaleLibrary.getIntervals(entry.smartScaleName);
                if (intervals.empty())
                  intervals = {0, 2, 4, 5, 7, 9, 11}; // Major fallback
                int root = zoneManager.getGlobalRootNote();

                float baseStep = std::floor(stepOffset);
                float frac = stepOffset - baseStep;
                int s0 = static_cast<int>(baseStep);
                int s1 = (frac >= 0.0f) ? s0 + 1 : s0 - 1;
                int pb0 = ScaleUtilities::smartStepOffsetToPitchBend(
                    currentNote, root, intervals, s0, pbRange);
                int pb1 = ScaleUtilities::smartStepOffsetToPitchBend(
                    currentNote, root, intervals, s1, pbRange);
                float f = std::abs(frac);
                float blended =
                    juce::jmap(f, 0.0f, 1.0f, static_cast<float>(pb0),
                               static_cast<float>(pb1));
                pbVal = juce::jlimit(0, 16383,
                                     static_cast<int>(std::round(blended)));
              }
            } else {
              // Standard PitchBend: stepOffset is in semitones. Clamp to global
              // PB range and allow extrapolation up to that range.
              float clampedOffset =
                  juce::jlimit(static_cast<float>(-pbRange),
                               static_cast<float>(pbRange), stepOffset);
              double stepsPerSemitone = settingsManager.getStepsPerSemitone();
              pbVal = static_cast<int>(std::round(
                  8192.0 +
                  (static_cast<double>(clampedOffset) * stepsPerSemitone)));
              pbVal = juce::jlimit(0, 16383, pbVal);
            }

            if (entry.touchGlideMs <= 0) {
              auto itLast = lastTouchpadContinuousValues.find(keyCont);
              if (itLast != lastTouchpadContinuousValues.end() &&
                  itLast->second == pbVal) {
                break; // No PB change – skip send
              }
              voiceManager.sendPitchBend(act.channel, pbVal);
              lastTouchpadContinuousValues[keyCont] = pbVal;
            } else {
              // Touch glide: state machine
              const uint32_t nowMs =
                  static_cast<uint32_t>(juce::Time::getMillisecondCounter());
              TouchGlideState &g = touchpadPitchGlideState[keyCont];
              if (g.phase == TouchGlidePhase::Idle ||
                  g.phase == TouchGlidePhase::GlidingToCenter) {
                g.phase = TouchGlidePhase::GlidingToFinger;
                auto itLast = lastTouchpadContinuousValues.find(keyCont);
                g.startValue = (itLast != lastTouchpadContinuousValues.end())
                                   ? itLast->second
                                   : 8192;
                g.targetValue = pbVal;
                g.startTimeMs = nowMs;
                g.durationMs = entry.touchGlideMs;
                g.lastSentValue = -1;
              }
              if (g.phase == TouchGlidePhase::GlidingToFinger) {
                double progress = (g.durationMs > 0)
                    ? std::min(1.0, static_cast<double>(nowMs - g.startTimeMs) /
                                        static_cast<double>(g.durationMs))
                    : 1.0;
                if (pbVal != g.targetValue) {
                  int currentOutput = juce::jlimit(
                      0, 16383,
                      static_cast<int>(std::round(
                          g.startValue +
                          progress * (g.targetValue - g.startValue))));
                  g.startValue = currentOutput;
                  g.targetValue = pbVal;
                }
                int output = juce::jlimit(
                    0, 16383,
                    static_cast<int>(std::round(
                        g.startValue +
                        progress * (g.targetValue - g.startValue))));
                if (output != g.lastSentValue) {
                  voiceManager.sendPitchBend(act.channel, output);
                  g.lastSentValue = output;
                  lastTouchpadContinuousValues[keyCont] = output;
                }
                if (progress >= 1.0)
                  g.phase = TouchGlidePhase::FollowingFinger;
              } else if (g.phase == TouchGlidePhase::FollowingFinger) {
                auto itLast = lastTouchpadContinuousValues.find(keyCont);
                if (itLast != lastTouchpadContinuousValues.end() &&
                    itLast->second == pbVal) {
                  break;
                }
                voiceManager.sendPitchBend(act.channel, pbVal);
                lastTouchpadContinuousValues[keyCont] = pbVal;
                g.lastSentValue = pbVal;
              }
            }
          }
        }
        break;
      }
    }
  }

  // Slide CC: second pass (single-fader slide). Per-mapping contact list.
  if (!ctx->touchpadMappings.empty()) {
    for (size_t mapIdx = 0; mapIdx < ctx->touchpadMappings.size(); ++mapIdx) {
      const auto &entry = ctx->touchpadMappings[mapIdx];
      if (entry.conversionKind != TouchpadConversionKind::SlideToCC)
        continue;
      if (!activeLayersSnapshot[(size_t)entry.layerId])
        continue;
      if (entry.action.type != ActionType::Expression ||
          entry.action.adsrSettings.target != AdsrTarget::CC)
        continue;
      const auto &act = entry.action;
      const auto &p = entry.conversionParams;

      int soloGroup =
          getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, entry.layerId));
      if ((soloGroup == 0 && entry.layoutGroupId != 0) ||
          (soloGroup > 0 && entry.layoutGroupId != soloGroup))
        continue;
      auto keySlideEnc = std::make_tuple(deviceHandle, entry.layerId,
          entry.eventId, act.channel, act.adsrSettings.ccNumber);
      auto entryKey = std::make_tuple(deviceHandle, entry.layerId, entry.eventId);

      switch (entry.eventId) {
      case TouchpadEvent::Finger1X:
      case TouchpadEvent::Finger1Y:
      case TouchpadEvent::Finger2X:
      case TouchpadEvent::Finger2Y:
      case TouchpadEvent::Finger1And2AvgX:
      case TouchpadEvent::Finger1And2AvgY:
        break;
      default:
        continue;
      }

      MappingLocalState local = buildMappingContactList(entry, mapIdx);
      if (touchpadMappingHasRegion(entry) && local.inRegion.empty())
        continue;

      std::vector<std::pair<size_t, const TouchpadContact *>> active;
      for (const auto &pairs : local.inRegion) {
        if (pairs.second->tipDown)
          active.push_back(pairs);
      }
        // At least one finger drives the slide; Precision mode only affects
        // "apply" semantics (e.g. second finger to commit). So we send CC
        // whenever we have any active contact.
        bool applierDownNow = !active.empty();
        bool prevApplierDown = false;
        {
          auto it = touchpadSlideApplierDownPrev.find(entryKey);
          if (it != touchpadSlideApplierDownPrev.end())
            prevApplierDown = it->second;
        }
        bool applierDownEdge = applierDownNow && !prevApplierDown;
        bool applierUpEdge = !applierDownNow && prevApplierDown;
        touchpadSlideApplierDownPrev[entryKey] = applierDownNow;

        // Any new touch cancels an in-progress return-to-rest glide.
        if (applierDownNow) {
          auto itGlide = touchpadSlideReturnState.find(
              std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                              act.channel, act.adsrSettings.ccNumber));
          if (itGlide != touchpadSlideReturnState.end())
            touchpadSlideReturnState.erase(itGlide);
        }

        if (!applierDownNow) {
          // When the controlling finger(s) lift and rest-on-release is enabled,
          // optionally glide the last CC value back to the configured rest
          // value.
          if (applierUpEdge && p.slideReturnOnRelease) {
            auto itLast = lastTouchpadSlideCCValues.find(keySlideEnc);
            if (itLast != lastTouchpadSlideCCValues.end()) {
              const int currentVal = itLast->second;
              const int restVal = juce::jlimit(p.outputMin, p.outputMax,
                                               p.slideRestValue);
              if (currentVal != restVal) {
                if (p.slideReturnGlideMs <= 0) {
                  voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber,
                                      restVal);
                  lastTouchpadSlideCCValues[keySlideEnc] = restVal;
                } else {
                  TouchGlideState &g = touchpadSlideReturnState[
                      std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                                      act.channel, act.adsrSettings.ccNumber)];
                  g.phase = TouchGlidePhase::GlidingToCenter;
                  g.startValue = currentVal;
                  g.targetValue = restVal;
                  g.startTimeMs = static_cast<uint32_t>(
                      juce::Time::getMillisecondCounter());
                  g.durationMs = p.slideReturnGlideMs;
                  g.lastSentValue = currentVal;
                  startTouchGlideTimerIfNeeded();
                }
              }
            }
          }

          auto itLock = touchpadSlideLockedContact.find(entryKey);
          if (itLock != touchpadSlideLockedContact.end())
            touchpadSlideLockedContact.erase(itLock);
          for (const auto &pairs : local.inRegion) {
            auto k = std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                pairs.second->contactId);
            touchpadSlideContactPrev[k] = {pairs.second->tipDown,
                pairs.second->normX, pairs.second->normY};
          }
          continue;
        }

        const TouchpadContact *positionContact =
            active.empty() ? nullptr : active[0].second;
        float positionX = local.x1;
        float positionY = local.y1;
        if (positionContact && (p.slideModeFlags & kMixerModeLock) != 0) {
          auto itLock = touchpadSlideLockedContact.find(entryKey);
          if (applierDownEdge) {
            touchpadSlideLockedContact[entryKey] = positionContact->contactId;
          } else if (itLock != touchpadSlideLockedContact.end() &&
              itLock->second >= 0) {
            bool found = false;
            for (const auto &pairs : active) {
              if (pairs.second->contactId == itLock->second) {
                positionContact = pairs.second;
                positionX = positionContact->normX;
                positionY = positionContact->normY;
                if (entry.regionLock) {
                  auto it = contactMappingLock.find(
                      std::make_tuple(deviceHandle, positionContact->contactId));
                  if (it != contactMappingLock.end() && it->second == mapIdx) {
                    positionX = std::clamp(positionX, entry.regionLeft,
                                         entry.regionRight);
                    positionY = std::clamp(positionY, entry.regionTop,
                                           entry.regionBottom);
                  }
                }
                found = true;
                break;
              }
            }
            if (!found) {
              for (const auto &pairs : local.inRegion) {
                auto k = std::make_tuple(deviceHandle, entry.layerId,
                    entry.eventId, pairs.second->contactId);
                touchpadSlideContactPrev[k] = {pairs.second->tipDown,
                    pairs.second->normX, pairs.second->normY};
              }
              continue; // Locked contact lifted; don't drive from other finger
            }
          }
        }

        // Choose axis: slideAxis 0 = Vertical (Y), 1 = Horizontal (X).
        float localPos = 0.0f;
        if (p.slideAxis == 0) {
          localPos = (positionY - entry.regionTop) * entry.invRegionHeight; // 0 = top, 1 = bottom
        } else {
          localPos = (positionX - entry.regionLeft) * entry.invRegionWidth; // 0 = left, 1 = right
        }
        localPos = std::clamp(localPos, 0.0f, 1.0f);

        // Work in a unified "axis space" where 0 = bottom/left, 1 = top/right.
        float axisPos = (p.slideAxis == 0) ? (1.0f - localPos) : localPos;

        // Apply input window as a true deadzone outside [inputMin, inputMax].
        const float windowMin = std::clamp(p.inputMin, 0.0f, 1.0f);
        const float windowMax = std::clamp(p.inputMax, 0.0f, 1.0f);
        const float posInWindow = std::clamp(axisPos, windowMin, windowMax);
        const bool inWindow = (axisPos >= windowMin && axisPos <= windowMax);

        // Normalized within window in axis space: 0 at inputMin, 1 at inputMax.
        float tAbs = 0.5f;
        if (p.invInputRange > 0.0f) {
          tAbs = (posInWindow - windowMin) * p.invInputRange;
          tAbs = std::clamp(tAbs, 0.0f, 1.0f);
        }
        const float outputRange =
            static_cast<float>(p.outputMax - p.outputMin);
        bool skipSendThisFrame = false;
        int ccVal = 0;

        // Deadzone: outside input window, do not emit CC. For relative mode,
        // keep anchor pinned to the nearest window edge so re-entry doesn't jump.
        if (!inWindow) {
          if ((p.slideModeFlags & kMixerModeRelative) != 0) {
            float &base = touchpadSlideRelativeValue[keySlideEnc];
            float &anchor = touchpadSlideRelativeAnchor[keySlideEnc];
            if (applierDownEdge) {
              auto itStored = lastTouchpadSlideCCValues.find(keySlideEnc);
              if (itStored != lastTouchpadSlideCCValues.end()) {
                base = static_cast<float>(itStored->second);
              } else {
                base = static_cast<float>(p.outputMin) +
                       outputRange * tAbs;
                lastTouchpadSlideCCValues[keySlideEnc] =
                    static_cast<int>(std::round(base));
              }
            }
            anchor = posInWindow;
          }
          for (const auto &pairs : local.inRegion) {
            auto k = std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                pairs.second->contactId);
            touchpadSlideContactPrev[k] = {pairs.second->tipDown,
                pairs.second->normX, pairs.second->normY};
          }
          continue;
        }

        if ((p.slideModeFlags & kMixerModeRelative) == 0) {
          ccVal = juce::jlimit(p.outputMin, p.outputMax,
              static_cast<int>(std::round(static_cast<float>(p.outputMin) +
                  outputRange * tAbs)));
        } else {
          float &base = touchpadSlideRelativeValue[keySlideEnc];
          float &anchor = touchpadSlideRelativeAnchor[keySlideEnc];
          if (applierDownEdge) {
            auto itStored = lastTouchpadSlideCCValues.find(keySlideEnc);
            if (itStored != lastTouchpadSlideCCValues.end()) {
              base = static_cast<float>(itStored->second);
            } else {
              base = static_cast<float>(p.outputMin) + outputRange * tAbs;
              lastTouchpadSlideCCValues[keySlideEnc] =
                  static_cast<int>(std::round(base));
            }
            anchor = posInWindow;
            skipSendThisFrame = true;
          } else {
            float deltaPos = posInWindow - anchor;
            float val = base + deltaPos * p.invInputRange * outputRange;
            val = std::clamp(val, static_cast<float>(p.outputMin),
                static_cast<float>(p.outputMax));
            ccVal = static_cast<int>(std::round(val));
          }
        }

        // Always send on first touch (applierDownEdge) in absolute mode so
        // Expression CC Slide produces a value immediately; change-only below
        // avoids duplicates on subsequent frames.
        if (!skipSendThisFrame) {
          auto itLast = lastTouchpadSlideCCValues.find(keySlideEnc);
          const bool firstTouch =
              (itLast == lastTouchpadSlideCCValues.end()) || applierDownEdge;
          if (firstTouch || itLast->second != ccVal) {
            voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, ccVal);
            lastTouchpadSlideCCValues[keySlideEnc] = ccVal;
          }
        }
        for (const auto &pairs : local.inRegion) {
          auto k = std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
              pairs.second->contactId);
          touchpadSlideContactPrev[k] = {pairs.second->tipDown,
              pairs.second->normX, pairs.second->normY};
        }
        continue;
      }
    }

  // Encoder CC: rotation (swipe) + push. Per-mapping contact list.
  constexpr float kEncoderBaselineMovement = 0.02f;
  if (!ctx->touchpadMappings.empty()) {
    for (size_t mapIdx = 0; mapIdx < ctx->touchpadMappings.size(); ++mapIdx) {
      const auto &entry = ctx->touchpadMappings[mapIdx];
      if (entry.conversionKind != TouchpadConversionKind::EncoderCC)
        continue;
      if (!activeLayersSnapshot[(size_t)entry.layerId])
        continue;
      if (entry.action.type != ActionType::Expression ||
          entry.action.adsrSettings.target != AdsrTarget::CC)
        continue;
      const auto &act = entry.action;
      const auto &p = entry.conversionParams;

      int soloGroup =
          getEffectiveSoloLayoutGroupForLayer(juce::jlimit(0, 8, entry.layerId));
      if ((soloGroup == 0 && entry.layoutGroupId != 0) ||
          (soloGroup > 0 && entry.layoutGroupId != soloGroup))
        continue;

      MappingLocalState local = buildMappingContactList(entry, mapIdx);
      if (touchpadMappingHasRegion(entry) && local.inRegion.empty())
        continue;

      std::vector<std::pair<size_t, const TouchpadContact *>> active;
      for (const auto &pair : local.inRegion) {
        if (pair.second->tipDown)
          active.push_back(pair);
      }

      auto keyEnc = std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                                   act.channel, act.adsrSettings.ccNumber);
      auto entryKey = std::make_tuple(deviceHandle, entry.layerId, entry.eventId);

      float posX = 0.0f, posY = 0.0f;
      if (!active.empty()) {
        posX = (local.x1 - entry.regionLeft) * entry.invRegionWidth;
        posY = (local.y1 - entry.regionTop) * entry.invRegionHeight;
        posX = std::clamp(posX, 0.0f, 1.0f);
        posY = std::clamp(posY, 0.0f, 1.0f);
      }

      // Rotation: purely distance from anchor (gesture start), not per-frame delta.
      // So slow swipes and fast swipes with the same distance give the same steps.
      int stepCount = 0;
      if (!active.empty() && p.encoderAxis <= 2) {
        const int activeCount = static_cast<int>(active.size());
        int prevCount = -1;
        {
          auto it = touchpadEncoderActiveCount.find(entryKey);
          if (it != touchpadEncoderActiveCount.end())
            prevCount = it->second;
        }
        touchpadEncoderActiveCount[entryKey] = activeCount;
        const bool fingerCountChanged = (prevCount >= 0 && prevCount != activeCount);

        const float stepScale = 1.0f / (p.encoderSensitivity * kEncoderBaselineMovement);
        float distanceFromAnchor = 0.0f;
        bool hadAnchor = false;

        if (p.encoderAxis == 0) {
          float current = 1.0f - posY;
          if (fingerCountChanged) {
            touchpadEncoderAnchor[keyEnc] = current;
            touchpadEncoderLastSentSteps[keyEnc] = 0;
          }
          auto itAnchor = touchpadEncoderAnchor.find(keyEnc);
          if (itAnchor != touchpadEncoderAnchor.end()) {
            distanceFromAnchor = current - itAnchor->second;
            hadAnchor = true;
          } else {
            touchpadEncoderAnchor[keyEnc] = current;
          }
          touchpadEncoderPrevPos[keyEnc] = current;
        } else if (p.encoderAxis == 1) {
          float current = posX;
          if (fingerCountChanged) {
            touchpadEncoderAnchor[keyEnc] = current;
            touchpadEncoderLastSentSteps[keyEnc] = 0;
          }
          auto itAnchor = touchpadEncoderAnchor.find(keyEnc);
          if (itAnchor != touchpadEncoderAnchor.end()) {
            distanceFromAnchor = current - itAnchor->second;
            hadAnchor = true;
          } else {
            touchpadEncoderAnchor[keyEnc] = current;
          }
          touchpadEncoderPrevPos[keyEnc] = current;
        } else {
          float currX = posX, currY = 1.0f - posY;
          if (fingerCountChanged) {
            touchpadEncoderAnchorX[keyEnc] = currX;
            touchpadEncoderAnchorY[keyEnc] = currY;
            touchpadEncoderLastSentSteps[keyEnc] = 0;
          }
          auto itAX = touchpadEncoderAnchorX.find(keyEnc);
          auto itAY = touchpadEncoderAnchorY.find(keyEnc);
          if (itAX != touchpadEncoderAnchorX.end() && itAY != touchpadEncoderAnchorY.end()) {
            float dX = (currX - itAX->second) * static_cast<float>(p.encoderStepSizeX);
            float dY = (currY - itAY->second) * static_cast<float>(p.encoderStepSizeY);
            distanceFromAnchor = dX + dY;
            hadAnchor = true;
          } else {
            touchpadEncoderAnchorX[keyEnc] = currX;
            touchpadEncoderAnchorY[keyEnc] = currY;
          }
          touchpadEncoderPrevPosX[keyEnc] = currX;
          touchpadEncoderPrevPosY[keyEnc] = currY;
        }

        if (hadAnchor) {
          if (std::abs(distanceFromAnchor) < p.encoderDeadZone)
            distanceFromAnchor = 0.0f;
          float idealSteps = distanceFromAnchor * stepScale;
          int idealStepsRounded = static_cast<int>(std::round(idealSteps));
          int lastSent = 0;
          auto itLast = touchpadEncoderLastSentSteps.find(keyEnc);
          if (itLast != touchpadEncoderLastSentSteps.end())
            lastSent = itLast->second;
          int stepsToSend = idealStepsRounded - lastSent;
          touchpadEncoderLastSentSteps[keyEnc] = idealStepsRounded;
          int mult = (p.encoderAxis == 2) ? 1 : p.encoderStepSize;
          stepCount = stepsToSend * mult;
        }
      }

      // Only clear when gesture actually ends (transition from active to empty).
      if (active.empty()) {
        auto itHad = touchpadEncoderHadActivePrev.find(entryKey);
        if (itHad != touchpadEncoderHadActivePrev.end() && itHad->second) {
          touchpadEncoderPrevPos.erase(keyEnc);
          touchpadEncoderPrevPosX.erase(keyEnc);
          touchpadEncoderPrevPosY.erase(keyEnc);
          touchpadEncoderAnchor.erase(keyEnc);
          touchpadEncoderAnchorX.erase(keyEnc);
          touchpadEncoderAnchorY.erase(keyEnc);
          touchpadEncoderLastSentSteps.erase(keyEnc);
          touchpadEncoderHadActivePrev[entryKey] = false;
          touchpadEncoderActiveCount.erase(entryKey);
        }
      } else {
        touchpadEncoderHadActivePrev[entryKey] = true;
      }

      if (stepCount != 0) {
        if (p.encoderOutputMode == 0) {
          int currentVal = p.encoderInitialValue;
          auto it = lastTouchpadEncoderCCValues.find(keyEnc);
          if (it != lastTouchpadEncoderCCValues.end())
            currentVal = it->second;
          currentVal += stepCount;
          if (p.encoderWrap) {
            while (currentVal > p.outputMax) currentVal -= (p.outputMax - p.outputMin + 1);
            while (currentVal < p.outputMin) currentVal += (p.outputMax - p.outputMin + 1);
          } else {
            currentVal = juce::jlimit(p.outputMin, p.outputMax, currentVal);
          }
          voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, currentVal);
          lastTouchpadEncoderCCValues[keyEnc] = currentVal;
        } else if (p.encoderOutputMode == 1) {
          int n = juce::jlimit(-63, 63, stepCount);
          int ccVal = 64;
          switch (p.encoderRelativeEncoding) {
          case 0:
            ccVal = 64 + n;
            break;
          case 1:
            ccVal = (n >= 0) ? (n & 0x7F) : (0x80 | (std::abs(n) & 0x7F));
            break;
          case 2:
            ccVal = (n >= 0) ? (n & 0x7F) : (256 + n) & 0xFF;
            break;
          case 3:
            ccVal = (n > 0) ? 1 : (n < 0 ? 127 : 64);
            break;
          default:
            ccVal = 64 + n;
            break;
          }
          ccVal = juce::jlimit(0, 127, ccVal);
          voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, ccVal);
        } else if (p.encoderOutputMode == 2) {
          int currentVal = 8192;
          auto it = lastTouchpadEncoderCCValues.find(keyEnc);
          if (it != lastTouchpadEncoderCCValues.end())
            currentVal = it->second;
          currentVal += stepCount * 128;
          currentVal = juce::jlimit(0, 16383, currentVal);
          int nrpnMsb = (p.encoderNRPNNumber >> 7) & 0x7F;
          int nrpnLsb = p.encoderNRPNNumber & 0x7F;
          voiceManager.sendCC(act.channel, 99, nrpnMsb);
          voiceManager.sendCC(act.channel, 98, nrpnLsb);
          voiceManager.sendCC(act.channel, 6, (currentVal >> 7) & 0x7F);
          voiceManager.sendCC(act.channel, 38, currentVal & 0x7F);
          lastTouchpadEncoderCCValues[keyEnc] = currentVal;
        }
      }

      bool pushActive = (p.encoderPushDetection == 0 && active.size() >= 2) ||
                        (p.encoderPushDetection == 1 && active.size() >= 3);
      bool pushPrev = false;
      {
        auto it = touchpadEncoderPushPrev.find(entryKey);
        if (it != touchpadEncoderPushPrev.end())
          pushPrev = it->second;
      }
      touchpadEncoderPushPrev[entryKey] = pushActive;
      bool pushEdgeOn = pushActive && !pushPrev;
      bool pushEdgeOff = !pushActive && pushPrev;

      if (p.encoderPushMode != 0) {
        if (pushEdgeOn) {
          if (p.encoderPushMode == 2) {
            bool &on = touchpadEncoderPushOn[entryKey];
            on = !on;
            if (p.encoderPushOutputType == 0) {
              voiceManager.sendCC(p.encoderPushChannel, p.encoderPushCCNumber,
                                 on ? p.encoderPushValue : 0);
            } else if (p.encoderPushOutputType == 1) {
              if (on)
                voiceManager.sendNoteOn(p.encoderPushChannel, p.encoderPushNote, 127);
              else
                voiceManager.sendNoteOff(p.encoderPushChannel, p.encoderPushNote);
            } else if (p.encoderPushOutputType == 2) {
              voiceManager.sendProgramChange(p.encoderPushChannel, p.encoderPushProgram);
            }
          } else if (p.encoderPushMode == 1) {
            if (p.encoderPushOutputType == 0)
              voiceManager.sendCC(p.encoderPushChannel, p.encoderPushCCNumber, p.encoderPushValue);
            else if (p.encoderPushOutputType == 1)
              voiceManager.sendNoteOn(p.encoderPushChannel, p.encoderPushNote, 127);
            else if (p.encoderPushOutputType == 2)
              voiceManager.sendProgramChange(p.encoderPushChannel, p.encoderPushProgram);
          } else if (p.encoderPushMode == 3) {
            if (p.encoderPushOutputType == 0)
              voiceManager.sendCC(p.encoderPushChannel, p.encoderPushCCNumber, p.encoderPushValue);
            else if (p.encoderPushOutputType == 1)
              voiceManager.sendNoteOn(p.encoderPushChannel, p.encoderPushNote, 127);
            else if (p.encoderPushOutputType == 2)
              voiceManager.sendProgramChange(p.encoderPushChannel, p.encoderPushProgram);
          }
        }
        if (pushEdgeOff && p.encoderPushMode == 1) {
          if (p.encoderPushOutputType == 0)
            voiceManager.sendCC(p.encoderPushChannel, p.encoderPushCCNumber, 0);
          else if (p.encoderPushOutputType == 1)
            voiceManager.sendNoteOff(p.encoderPushChannel, p.encoderPushNote);
        }
      }
    }
  }

  // Touchpad mixer layouts (CC faders). Per-layout: count fingers only in this
  // strip's region. 1 finger = Quick; 2+ = Precision (F1=target, F2=apply).
  bool touchpadMixerStateChanged = false;
  if (!ctx->touchpadMixerStrips.empty()) {
    juce::ScopedWriteLock wl(mixerStateLock);
    for (size_t stripIdx = 0; stripIdx < ctx->touchpadMixerStrips.size();
         ++stripIdx) {
      const auto &strip = ctx->touchpadMixerStrips[stripIdx];
      if (!activeLayersSnapshot[(size_t)strip.layerId])
        continue;
      const int N = strip.numFaders;
      if (N <= 0)
        continue;

      // Get contacts in this strip's region, ordered by contactId
      std::vector<std::pair<size_t, const TouchpadContact *>> inRegion;
      for (size_t i = 0; i < contacts.size(); ++i) {
        const auto &c = contacts[i];
        auto layout = i < layoutPerContact.size()
                          ? layoutPerContact[i]
                          : findLayoutForPoint(c.normX, c.normY);
        if (!layout || layout->first != TouchpadType::Mixer ||
            layout->second != stripIdx)
          continue;
        inRegion.push_back({i, &c});
      }
      std::sort(inRegion.begin(), inRegion.end(),
                [](const auto &a, const auto &b) {
                  return a.second->contactId < b.second->contactId;
                });

      // Filter to tipDown for applier; 1 = Quick, 2+ = Precision
      std::vector<std::pair<size_t, const TouchpadContact *>> active;
      for (const auto &p : inRegion) {
        if (p.second->tipDown)
          active.push_back(p);
      }

      bool usePrecision = (strip.modeFlags & kMixerModeUseFinger1) == 0;
      auto stripKey = std::make_tuple(deviceHandle, static_cast<int>(stripIdx));

      // Region lock: when strip has regionLock, clamp position to region bounds
      auto getEffectivePos = [&](int contactId, float nx,
                                 float ny) -> std::pair<float, float> {
        if (!strip.regionLock)
          return {nx, ny};
        auto lockKey = std::make_tuple(deviceHandle, contactId);
        auto itLock = contactLayoutLock.find(lockKey);
        if (itLock == contactLayoutLock.end())
          return {nx, ny};
        if (itLock->second.first != TouchpadType::Mixer ||
            itLock->second.second != stripIdx)
          return {nx, ny};
        float ex = std::clamp(nx, strip.regionLeft, strip.regionRight);
        float ey = std::clamp(ny, strip.regionTop, strip.regionBottom);
        return {ex, ey};
      };

      // Precision: applier (finger2) is considered down iff we have 2+ active
      // contacts. Track this per-strip so finger2 down/up edges are reliable
      // even when we early-out (active.size() < 2).
      bool applierDownNow =
          usePrecision ? (active.size() >= 2) : !active.empty();
      bool prevApplierDownNow = false;
      {
        auto itPrev = touchpadMixerApplierDownPrev.find(stripKey);
        if (itPrev != touchpadMixerApplierDownPrev.end())
          prevApplierDownNow = itPrev->second;
      }
      bool applierDownEdge = applierDownNow && !prevApplierDownNow;
      touchpadMixerApplierDownPrev[stripKey] = applierDownNow;

      if (!applierDownNow) {
        auto itLock = touchpadMixerLockedFader.find(stripKey);
        if (itLock != touchpadMixerLockedFader.end())
          touchpadMixerLockedFader.erase(itLock);
        touchpadMixerLastFaderIndex.erase(stripKey);
        for (const auto &p : inRegion) {
          auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   p.second->contactId);
          auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                          p.second->normY);
          touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
        }
        continue;
      }

      // First finger = position (drives fader/CC). Second finger = mode switch
      // (enables first finger to act in Precision). Quick: F1 does both.
      const TouchpadContact *positionContact = active[0].second;
      const TouchpadContact *applierContact =
          usePrecision ? active[1].second : active[0].second;
      (void)applierContact;

      auto [posX, posY] =
          getEffectivePos(positionContact->contactId, positionContact->normX,
                          positionContact->normY);
      float positionX = posX;
      float positionY = posY;

      // In Precision: first finger drives value; second finger only enables.
      // In Quick: first finger = only finger, drives value.
      auto prevPositionKey = std::make_tuple(
          deviceHandle, static_cast<int>(stripIdx), positionContact->contactId);
      auto itPrevPosition = touchpadMixerContactPrev.find(prevPositionKey);

      // First finger position drives fader index and CC value (both modes)
      float localX = (positionX - strip.regionLeft) * strip.invRegionWidth;
      float localY = (positionY - strip.regionTop) * strip.invRegionHeight;
      localX = std::clamp(localX, 0.0f, 1.0f);
      localY = std::clamp(localY, 0.0f, 1.0f);

      // Fader index from local X: [0,1) -> [0, N) clamped
      float fx = std::clamp(localX, 0.0f, 1.0f - 1e-6f);
      int faderIndex = static_cast<int>(fx * static_cast<float>(N));
      if (faderIndex >= N)
        faderIndex = N - 1;

      auto lockKey = stripKey;
      if ((strip.modeFlags & kMixerModeLock) != 0) {
        auto itLock = touchpadMixerLockedFader.find(lockKey);
        if (applierDownEdge) {
          touchpadMixerLockedFader[lockKey] = faderIndex;
        } else if (itLock != touchpadMixerLockedFader.end()) {
          faderIndex = itLock->second;
        }
      }

      // Mute: first finger dictates which fader; second finger down = apply
      // mute
      bool posContactDown = positionContact->tipDown;
      if ((strip.modeFlags & kMixerModeMuteButtons) != 0 && applierDownEdge &&
          posContactDown && localY >= kMuteButtonRegionTop &&
          positionX >= strip.regionLeft && positionX < strip.regionRight &&
          positionY >= strip.regionTop && positionY < strip.regionBottom) {
        float sx = std::clamp(localX, 0.0f, 1.0f - 1e-6f);
        int col = static_cast<int>(sx * static_cast<float>(N));
        if (col >= N)
          col = N - 1;
        auto muteKey =
            std::make_tuple(deviceHandle, static_cast<int>(stripIdx), col);
        bool &muted = touchpadMixerMuteState[muteKey];
        if (!muted) {
          auto lastKey =
              std::make_tuple(deviceHandle, static_cast<int>(stripIdx), col);
          auto itLast = lastTouchpadMixerCCValues.find(lastKey);
          if (itLast != lastTouchpadMixerCCValues.end())
            touchpadMixerValueBeforeMute[muteKey] = itLast->second;
        }
        muted = !muted;
        touchpadMixerStateChanged = true;
        int ccNum = strip.ccStart + col;
        voiceManager.sendCC(strip.midiChannel, ccNum, muted ? 0 : 64);
      }

      // Fader area: finger in mute zone must not drive fader.
      if ((strip.modeFlags & kMixerModeMuteButtons) != 0 &&
          localY >= kMuteButtonRegionTop) {
        for (const auto &p : inRegion) {
          auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   p.second->contactId);
          auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                          p.second->normY);
          touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
        }
        continue;
      }

      auto muteKey =
          std::make_tuple(deviceHandle, static_cast<int>(stripIdx), faderIndex);
      bool isMuted = false;
      {
        auto itMute = touchpadMixerMuteState.find(muteKey);
        if (itMute != touchpadMixerMuteState.end())
          isMuted = itMute->second;
      }
      if (isMuted) {
        for (const auto &p : inRegion) {
          auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   p.second->contactId);
          auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                          p.second->normY);
          touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
        }
        continue;
      }

      float effectiveY = localY * strip.effectiveYScale;

      // Input Y min/max: map touchpad Y from [inputMin, inputMax] to normalized
      // 0..1 (then inverted).
      float effectiveYClamped =
          juce::jlimit(strip.inputMin, strip.inputMax, effectiveY);
      float t = (effectiveYClamped - strip.inputMin) * strip.invInputRange;
      t = std::clamp(t, 0.0f, 1.0f);
      t = 1.0f - t; // Invert Y: screen up = fader up (high value)

      // Output Y min/max: map normalized t to [outputMin, outputMax].
      const float outputRange =
          static_cast<float>(strip.outputMax - strip.outputMin);
      bool skipSendThisFrame = false;
      int ccVal = 0;
      if ((strip.modeFlags & kMixerModeRelative) == 0) {
        ccVal = juce::jlimit(
            strip.outputMin, strip.outputMax,
            static_cast<int>(std::round(static_cast<float>(strip.outputMin) +
                                        outputRange * t)));
      } else {
        // Relative: finger2 down = anchor (starting position). Finger1 movement
        // from anchor => delta => fader value. Free mode: on fader switch,
        // apply to old fader then set anchor at entry point for new fader.
        auto relKey = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                      faderIndex);
        int lastFader = -1;
        {
          auto itLF = touchpadMixerLastFaderIndex.find(stripKey);
          if (itLF != touchpadMixerLastFaderIndex.end())
            lastFader = itLF->second;
        }
        float &base = touchpadMixerRelativeValue[relKey];
        float &anchor = touchpadMixerRelativeAnchor[relKey];
        auto normYToEffectiveClamped = [&](float ny) -> float {
          float ly = (ny - strip.regionTop) * strip.invRegionHeight;
          ly = std::clamp(ly, 0.0f, 1.0f);
          float ey = ly * strip.effectiveYScale;
          return juce::jlimit(strip.inputMin, strip.inputMax, ey);
        };

        if (applierDownEdge) {
          // Finger2 down: establish anchor/base only. Do NOT emit CC this frame.
          // Base should be the current fader value; prefer last-sent CC if we
          // have it, otherwise fall back to value-under-finger.
          auto faderKey = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                          faderIndex);
          auto itStored = lastTouchpadMixerCCValues.find(faderKey);
          if (itStored != lastTouchpadMixerCCValues.end()) {
            base = static_cast<float>(itStored->second);
          } else {
            base = static_cast<float>(strip.outputMin) + outputRange * t;
            // Seed last value so we don't immediately send on the next frame
            // when delta is still 0.
            lastTouchpadMixerCCValues[faderKey] =
                static_cast<int>(std::round(base));
          }
          anchor = effectiveYClamped;
          touchpadMixerLastFaderIndex[stripKey] = faderIndex;
          skipSendThisFrame = true;
        } else if ((strip.modeFlags & kMixerModeLock) == 0 && lastFader >= 0 &&
                   lastFader != faderIndex) {
          // Free mode: switched fader. Apply to old fader (value at exit), then
          // set anchor at entry for new fader.
          float exitY = itPrevPosition != touchpadMixerContactPrev.end()
                            ? itPrevPosition->second.y
                            : positionY;
          float exitEffectiveY = normYToEffectiveClamped(exitY);
          auto oldRelKey = std::make_tuple(
              deviceHandle, static_cast<int>(stripIdx), lastFader);
          float oldBase = touchpadMixerRelativeValue[oldRelKey];
          float oldAnchor = touchpadMixerRelativeAnchor[oldRelKey];
          float deltaY = exitEffectiveY - oldAnchor;
          float oldVal = oldBase - deltaY * strip.invInputRange * outputRange;
          oldVal = std::clamp(oldVal, static_cast<float>(strip.outputMin),
                              static_cast<float>(strip.outputMax));
          int oldCc = static_cast<int>(std::round(oldVal));
          voiceManager.sendCC(strip.midiChannel, strip.ccStart + lastFader,
                              oldCc);
          lastTouchpadMixerCCValues[std::make_tuple(
              deviceHandle, static_cast<int>(stripIdx), lastFader)] = oldCc;
          touchpadMixerStateChanged = true;

          // Entering a new fader: establish anchor/base only. Do NOT emit CC
          // for the new fader until finger1 moves.
          auto newFaderKey = std::make_tuple(deviceHandle,
                                             static_cast<int>(stripIdx),
                                             faderIndex);
          auto itStored = lastTouchpadMixerCCValues.find(newFaderKey);
          if (itStored != lastTouchpadMixerCCValues.end()) {
            base = static_cast<float>(itStored->second);
          } else {
            base = static_cast<float>(strip.outputMin) + outputRange * t;
            lastTouchpadMixerCCValues[newFaderKey] =
                static_cast<int>(std::round(base));
          }
          anchor = effectiveYClamped; // entry point for new fader
          touchpadMixerLastFaderIndex[stripKey] = faderIndex;
          skipSendThisFrame = true;
        } else {
          touchpadMixerLastFaderIndex[stripKey] = faderIndex;
          float deltaY = effectiveYClamped - anchor;
          float val = base - deltaY * strip.invInputRange * outputRange;
          val = std::clamp(val, static_cast<float>(strip.outputMin),
                           static_cast<float>(strip.outputMax));
          ccVal = static_cast<int>(std::round(val));
        }
      }

      if (skipSendThisFrame) {
        for (const auto &p : inRegion) {
          auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   p.second->contactId);
          auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                          p.second->normY);
          touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
        }
        continue;
      }

      int ccNum = strip.ccStart + faderIndex;
      auto lastKey =
          std::make_tuple(deviceHandle, static_cast<int>(stripIdx), faderIndex);
      auto itLast = lastTouchpadMixerCCValues.find(lastKey);
      if (itLast != lastTouchpadMixerCCValues.end() &&
          itLast->second == ccVal) {
        for (const auto &p : inRegion) {
          auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   p.second->contactId);
          auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                          p.second->normY);
          touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
        }
        continue;
      }
      voiceManager.sendCC(strip.midiChannel, ccNum, ccVal);
      lastTouchpadMixerCCValues[lastKey] = ccVal;
      touchpadMixerStateChanged = true;

      for (const auto &p : inRegion) {
        auto k = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                 p.second->contactId);
        auto [ex, ey] = getEffectivePos(p.second->contactId, p.second->normX,
                                        p.second->normY);
        touchpadMixerContactPrev[k] = {p.second->tipDown, ex, ey};
      }
    }
    if (touchpadMixerStateChanged) {
      int64_t now = juce::Time::getMillisecondCounter();
      int64_t last = lastMixerChangeNotifyMs.load(std::memory_order_relaxed);
      if (now - last >= 16) { // ~60 Hz throttle
        lastMixerChangeNotifyMs.store(now, std::memory_order_relaxed);
        sendChangeMessage();
      }
    }
  }

  // Chord Pad layouts: each pad triggers a chord. Behaviour depends on
  // latchMode: momentary (finger-held) vs toggle (pad latches chord on/off).
  if (!ctx->touchpadChordPads.empty()) {
    juce::ScopedWriteLock wl(mixerStateLock);
    for (size_t stripIdx = 0; stripIdx < ctx->touchpadChordPads.size();
         ++stripIdx) {
      const auto &strip = ctx->touchpadChordPads[stripIdx];
      if (!activeLayersSnapshot[(size_t)strip.layerId])
        continue;
      if (strip.rows <= 0 || strip.columns <= 0)
        continue;

      auto contactToPad = [&strip](float nx,
                                   float ny) -> int {
        float localX = (nx - strip.regionLeft) * strip.invRegionWidth;
        float localY = (ny - strip.regionTop) * strip.invRegionHeight;
        if (localX < 0.0f || localX >= 1.0f || localY < 0.0f || localY >= 1.0f)
          return -1;
        int col = static_cast<int>(localX * static_cast<float>(strip.columns));
        int row = static_cast<int>(localY * static_cast<float>(strip.rows));
        col = juce::jlimit(0, strip.columns - 1, col);
        row = juce::jlimit(0, strip.rows - 1, row);
        return row * strip.columns + col;
      };

      auto getEffectivePos = [&](int contactId, float nx,
                                 float ny) -> std::pair<float, float> {
        if (!strip.regionLock)
          return {nx, ny};
        auto lockKey = std::make_tuple(deviceHandle, contactId);
        auto itLock = contactLayoutLock.find(lockKey);
        if (itLock == contactLayoutLock.end())
          return {nx, ny};
        if (itLock->second.first != TouchpadType::ChordPad ||
            itLock->second.second != stripIdx)
          return {nx, ny};
        float ex = std::clamp(nx, strip.regionLeft, strip.regionRight);
        float ey = std::clamp(ny, strip.regionTop, strip.regionBottom);
        return {ex, ey};
      };

      auto buildChordForPad = [&](int padIndex) -> std::vector<int> {
        std::vector<int> notes;
        if (padIndex < 0)
          return notes;

        // Use global scale intervals to build chords relative to baseRootNote.
        std::vector<int> intervals = zoneManager.getGlobalScaleIntervals();
        if (intervals.empty())
          intervals = {0, 2, 4, 5, 7, 9, 11}; // Major fallback

        const int numDeg = static_cast<int>(intervals.size());
        int degreeIndex = padIndex % juce::jmax(1, numDeg);
        int octaveOffset = padIndex / juce::jmax(1, numDeg);
        degreeIndex = juce::jlimit(0, numDeg - 1, degreeIndex);

        auto degreeToNote = [&](int baseDegree, int offset) -> int {
          int deg = baseDegree + offset;
          int octaveAdj = 0;
          if (deg >= 0) {
            octaveAdj = deg / numDeg;
          } else {
            octaveAdj = (deg - (numDeg - 1)) / numDeg;
          }
          int idx = ((deg % numDeg) + numDeg) % numDeg;
          int semitone =
              intervals[idx] + 12 * (octaveOffset + octaveAdj);
          int base = strip.baseRootNote;
          return juce::jlimit(0, 127, base + semitone);
        };

        int presetId = strip.presetId;
        if (presetId < 0)
          presetId = 0;

        // Preset 0: diatonic triads (root, 3rd, 5th).
        if (presetId == 0) {
          int n0 = degreeToNote(degreeIndex, 0);
          int n1 = degreeToNote(degreeIndex, 2);
          int n2 = degreeToNote(degreeIndex, 4);
          notes = {n0, n1, n2};
        }
        // Preset 1: diatonic 7ths (root, 3rd, 5th, 7th).
        else if (presetId == 1) {
          int n0 = degreeToNote(degreeIndex, 0);
          int n1 = degreeToNote(degreeIndex, 2);
          int n2 = degreeToNote(degreeIndex, 4);
          int n3 = degreeToNote(degreeIndex, 6);
          notes = {n0, n1, n2, n3};
        }
        // Preset 2: pop extended – root, fifth, add9.
        else {
          int n0 = degreeToNote(degreeIndex, 0);
          int n1 = degreeToNote(degreeIndex, 4);
          int n2 = degreeToNote(degreeIndex, 7); // roughly add9 colour
          notes = {n0, n1, n2};
        }

        // Clamp and dedupe.
        std::sort(notes.begin(), notes.end());
        notes.erase(std::unique(notes.begin(), notes.end()), notes.end());
        return notes;
      };

      // Clear latched pads if latchMode was turned off since last frame.
      {
        int stripKey = static_cast<int>(stripIdx);
        bool prevLatch =
            chordPadLastLatchMode.count(stripKey)
                ? chordPadLastLatchMode[stripKey]
                : strip.latchMode;
        if (prevLatch && !strip.latchMode) {
          for (auto it = chordPadLatchedPads.begin();
               it != chordPadLatchedPads.end();) {
            uintptr_t dev = std::get<0>(it->first);
            int sIdx = std::get<1>(it->first);
            if (sIdx == stripKey) {
              voiceManager.handleKeyUp(it->second.inputId);
              it = chordPadLatchedPads.erase(it);
            } else {
              ++it;
            }
          }
        }
        chordPadLastLatchMode[stripKey] = strip.latchMode;
      }

      if (strip.latchMode) {
        // Latch mode: pad toggles chord on/off; contacts only drive toggle
        // edges.
        size_t ci = 0;
        for (const auto &c : contacts) {
          if (!c.tipDown) {
            ++ci;
            continue;
          }
          auto layoutMatch = ci < layoutPerContact.size()
                                 ? layoutPerContact[ci]
                                 : findLayoutForPoint(c.normX, c.normY);
          ++ci;
          if (!layoutMatch || layoutMatch->first != TouchpadType::ChordPad ||
              layoutMatch->second != stripIdx)
            continue;
          auto lockKey = std::make_tuple(deviceHandle, c.contactId);
          auto itLock = contactLayoutLock.find(lockKey);
          if (itLock != contactLayoutLock.end() &&
              (itLock->second.first != TouchpadType::ChordPad ||
               itLock->second.second != stripIdx))
            continue;

          auto keyContact = std::make_tuple(deviceHandle,
                                            static_cast<int>(stripIdx),
                                            c.contactId);
          // Only toggle on the first frame this contact appears.
          if (chordPadActiveChords.find(keyContact) !=
              chordPadActiveChords.end())
            continue;

          auto [effX, effY] = getEffectivePos(c.contactId, c.normX, c.normY);
          int padIndex = contactToPad(effX, effY);
          if (padIndex < 0)
            continue;

          auto padKey = std::make_tuple(deviceHandle,
                                        static_cast<int>(stripIdx), padIndex);
          auto itPad = chordPadLatchedPads.find(padKey);
          if (itPad == chordPadLatchedPads.end()) {
            // Toggle ON – build chord and noteOn.
            auto chordNotes = buildChordForPad(padIndex);
            if (chordNotes.empty())
              continue;
            std::vector<int> velocities;
            velocities.reserve(chordNotes.size());
            for (size_t i = 0; i < chordNotes.size(); ++i) {
              int vel = calculateVelocity(strip.baseVelocity,
                                          strip.velocityRandom);
              velocities.push_back(vel);
            }
            int keyCode =
                static_cast<int>((stripIdx << 8) | (padIndex & 0xFF));
            InputID padInput{deviceHandle, keyCode};
            voiceManager.noteOn(padInput, chordNotes, velocities,
                                strip.midiChannel, 0, true, 0,
                                PolyphonyMode::Poly, 50);
            if (!chordNotes.empty())
              lastTriggeredNote = chordNotes.front();
            chordPadLatchedPads[padKey] = {padInput, padIndex};
          } else {
            // Toggle OFF – release latched chord.
            voiceManager.handleKeyUp(itPad->second.inputId);
            chordPadLatchedPads.erase(itPad);
          }

          // Mark this contact as having triggered a toggle so we don't retrigger
          // while it stays down.
          chordPadActiveChords[keyContact] = {InputID{deviceHandle, 0},
                                              padIndex};
        }

        // Clean up contact-based toggle markers when fingers lift.
        for (auto it = chordPadActiveChords.begin();
             it != chordPadActiveChords.end();) {
          uintptr_t kDev = std::get<0>(it->first);
          int kStrip = std::get<1>(it->first);
          int kContact = std::get<2>(it->first);
          if (kDev != deviceHandle ||
              kStrip != static_cast<int>(stripIdx)) {
            ++it;
            continue;
          }
          bool stillDown = false;
          for (const auto &c : contacts) {
            if (c.contactId == kContact && c.tipDown) {
              stillDown = true;
              break;
            }
          }
          if (!stillDown)
            it = chordPadActiveChords.erase(it);
          else
            ++it;
        }
      } else {
        // Momentary mode: chord lives while finger stays down on pad.
        size_t ci = 0;
        for (const auto &c : contacts) {
          if (!c.tipDown) {
            ++ci;
            continue;
          }
          auto layoutMatch = ci < layoutPerContact.size()
                                 ? layoutPerContact[ci]
                                 : findLayoutForPoint(c.normX, c.normY);
          ++ci;
          if (!layoutMatch || layoutMatch->first != TouchpadType::ChordPad ||
              layoutMatch->second != stripIdx)
            continue;
          auto lockKey = std::make_tuple(deviceHandle, c.contactId);
          auto itLock = contactLayoutLock.find(lockKey);
          if (itLock != contactLayoutLock.end() &&
              (itLock->second.first != TouchpadType::ChordPad ||
               itLock->second.second != stripIdx))
            continue;
          auto [effX, effY] = getEffectivePos(c.contactId, c.normX, c.normY);
          int padIndex = contactToPad(effX, effY);
          if (padIndex < 0)
            continue;
          auto chordNotes = buildChordForPad(padIndex);
          if (chordNotes.empty())
            continue;

          auto key = std::make_tuple(deviceHandle,
                                     static_cast<int>(stripIdx),
                                     c.contactId);
          int keyCode =
              static_cast<int>((stripIdx << 8) | (c.contactId & 0xFF));
          InputID input{deviceHandle, keyCode};

          auto it = chordPadActiveChords.find(key);
          if (it != chordPadActiveChords.end()) {
            if (it->second.padIndex == padIndex)
              continue;
            voiceManager.handleKeyUp(it->second.inputId);
            chordPadActiveChords.erase(it);
          }

          std::vector<int> velocities;
          velocities.reserve(chordNotes.size());
          for (size_t i = 0; i < chordNotes.size(); ++i) {
            int vel =
                calculateVelocity(strip.baseVelocity, strip.velocityRandom);
            velocities.push_back(vel);
          }
          voiceManager.noteOn(input, chordNotes, velocities, strip.midiChannel,
                              0, true, 0, PolyphonyMode::Poly, 50);
          if (!chordNotes.empty())
            lastTriggeredNote = chordNotes.front();
          chordPadActiveChords[key] = {input, padIndex};
        }

        for (auto it = chordPadActiveChords.begin();
             it != chordPadActiveChords.end();) {
          uintptr_t kDev = std::get<0>(it->first);
          int kStrip = std::get<1>(it->first);
          int kContact = std::get<2>(it->first);
          if (kDev != deviceHandle ||
              kStrip != static_cast<int>(stripIdx)) {
            ++it;
            continue;
          }
          bool stillValid = false;
          for (const auto &c : contacts) {
            if (c.contactId != kContact)
              continue;
            if (!c.tipDown)
              break;
            auto [effX, effY] = getEffectivePos(c.contactId, c.normX, c.normY);
            int padIndex = contactToPad(effX, effY);
            if (padIndex >= 0 && padIndex == it->second.padIndex) {
              stillValid = true;
              break;
            }
            if (padIndex >= 0)
              stillValid = true;
            break;
          }
          if (!stillValid) {
            voiceManager.handleKeyUp(it->second.inputId);
            it = chordPadActiveChords.erase(it);
          } else {
            ++it;
          }
        }
      }
  }
  }

  // Drum pad layouts: grid -> Note On/Off per contact. Region-based: only
  // process contacts in this strip's region. Content stretched to fit region.
  if (!ctx->touchpadDrumPadStrips.empty()) {
    juce::ScopedWriteLock wl(mixerStateLock);
    for (size_t stripIdx = 0; stripIdx < ctx->touchpadDrumPadStrips.size();
         ++stripIdx) {
      const auto &strip = ctx->touchpadDrumPadStrips[stripIdx];
      if (!activeLayersSnapshot[(size_t)strip.layerId] || strip.numPads <= 0)
        continue;

      MidiAction actTemplate{};
      actTemplate.type = ActionType::Note;
      actTemplate.channel = strip.midiChannel;
      actTemplate.data2 = strip.baseVelocity;
      actTemplate.velocityRandom = strip.velocityRandom;
      actTemplate.releaseBehavior = NoteReleaseBehavior::SendNoteOff;

      // Helper: compute pad index and MIDI note from contact position (local
      // coords within region), or -1 if outside region. Note mapping depends on
      // layoutMode (Classic vs HarmonicGrid).
      auto contactToPad = [this, &strip](
                              float nx,
                              float ny) -> std::pair<int, int> {
        float localX = (nx - strip.regionLeft) * strip.invRegionWidth;
        float localY = (ny - strip.regionTop) * strip.invRegionHeight;
        if (localX < 0.0f || localX >= 1.0f || localY < 0.0f || localY >= 1.0f)
          return {-1, -1};
        int col = static_cast<int>(localX * static_cast<float>(strip.columns));
        int row = static_cast<int>(localY * static_cast<float>(strip.rows));
        col = juce::jlimit(0, strip.columns - 1, col);
        row = juce::jlimit(0, strip.rows - 1, row);
        int padIndex = row * strip.columns + col;

        // Classic: chromatic grid from midiNoteStart.
        if (strip.layoutMode == DrumPadLayoutMode::Classic) {
          int midiNote =
              juce::jlimit(0, 127, strip.midiNoteStart + padIndex);
          return {padIndex, midiNote};
        }

        // HarmonicGrid: isomorphic harmonic grid with optional scale filter.
        int rawNote = juce::jlimit(
            0, 127,
            strip.midiNoteStart + col + row * strip.harmonicRowInterval);

        int midiNote = rawNote;
        if (strip.harmonicUseScaleFilter) {
          int root = zoneManager.getGlobalRootNote();
          std::vector<int> intervals = zoneManager.getGlobalScaleIntervals();
          if (intervals.empty())
            intervals = {0, 2, 4, 5, 7, 9, 11}; // Major fallback

          int rel = rawNote - root;
          int octave = (rel >= 0) ? (rel / 12) : ((rel - 11) / 12);
          int within = rel - octave * 12;
          if (within < 0) {
            within += 12;
            --octave;
          }

          int chosen = intervals.back();
          for (int iv : intervals) {
            if (within <= iv) {
              chosen = iv;
              break;
            }
          }
          int snapped = root + octave * 12 + chosen;
          midiNote = juce::jlimit(0, 127, snapped);
        }

        return {padIndex, midiNote};
      };

      // Region lock: get effective position (clamped to region when locked)
      auto getDrumEffectivePos = [&](int contactId, float nx,
                                     float ny) -> std::pair<float, float> {
        if (!strip.regionLock)
          return {nx, ny};
        auto lockKey = std::make_tuple(deviceHandle, contactId);
        auto itLock = contactLayoutLock.find(lockKey);
        if (itLock == contactLayoutLock.end())
          return {nx, ny};
        if (itLock->second.first != TouchpadType::DrumPad ||
            itLock->second.second != stripIdx)
          return {nx, ny};
        float ex = std::clamp(nx, strip.regionLeft, strip.regionRight);
        float ey = std::clamp(ny, strip.regionTop, strip.regionBottom);
        return {ex, ey};
      };

      // Process contacts that are tipDown and in this strip's region
      size_t ci = 0;
      for (const auto &c : contacts) {
        if (!c.tipDown) {
          ++ci;
          continue;
        }
        auto layoutMatch = ci < layoutPerContact.size()
                               ? layoutPerContact[ci]
                               : findLayoutForPoint(c.normX, c.normY);
        ++ci;
        if (!layoutMatch || layoutMatch->first != TouchpadType::DrumPad ||
            layoutMatch->second != stripIdx)
          continue;
        // Region lock: if locked to different layout, skip
        auto lockKey = std::make_tuple(deviceHandle, c.contactId);
        auto itLock = contactLayoutLock.find(lockKey);
        if (itLock != contactLayoutLock.end() &&
            (itLock->second.first != TouchpadType::DrumPad ||
             itLock->second.second != stripIdx))
          continue; // Locked to different layout
        auto [effX, effY] = getDrumEffectivePos(c.contactId, c.normX, c.normY);
        auto [padIndex, midiNote] = contactToPad(effX, effY);
        if (padIndex < 0)
          continue; // Outside pad area
        auto key = std::make_tuple(deviceHandle, static_cast<int>(stripIdx),
                                   c.contactId);
        int keyCode = static_cast<int>((stripIdx << 8) | (c.contactId & 0xFF));
        InputID drumPadInput{deviceHandle, keyCode};

        auto it = drumPadActiveNotes.find(key);
        if (it != drumPadActiveNotes.end()) {
          if (it->second.padIndex == padIndex)
            continue; // Same pad, holding
          // Moved to different pad: note off for old, then note on for new
          MidiAction releaseAct = actTemplate;
          triggerManualNoteRelease(it->second.inputId, releaseAct);
          drumPadActiveNotes.erase(it);
        }

        MidiAction act = actTemplate;
        act.data1 = midiNote;
        triggerManualNoteOn(drumPadInput, act, true);
        drumPadActiveNotes[key] = {drumPadInput, padIndex};
      }

      // For each tracked contact: if no longer valid (tipUp or outside pad),
      // send note off
      for (auto it = drumPadActiveNotes.begin();
           it != drumPadActiveNotes.end();) {
        uintptr_t kDev = std::get<0>(it->first);
        int kStrip = std::get<1>(it->first);
        int kContact = std::get<2>(it->first);
        if (kDev != deviceHandle || kStrip != static_cast<int>(stripIdx)) {
          ++it;
          continue;
        }
        bool stillValid = false;
        for (const auto &c : contacts) {
          if (c.contactId != kContact)
            continue;
          if (!c.tipDown)
            break; // Finger lifted
          auto [effX, effY] =
              getDrumEffectivePos(c.contactId, c.normX, c.normY);
          auto [padIndex, _] = contactToPad(effX, effY);
          if (padIndex >= 0 && padIndex == it->second.padIndex) {
            stillValid = true;
            break;
          }
          // Finger still down but moved outside or to different pad - will be
          // handled by the "moved to different pad" path when we process that
          // contact; but if outside, we need note off here
          if (padIndex >= 0)
            stillValid = true; // Different pad: new note on was sent above
          break;
        }
        if (!stillValid) {
          MidiAction releaseAct = actTemplate;
          releaseAct.releaseBehavior = NoteReleaseBehavior::SendNoteOff;
          triggerManualNoteRelease(it->second.inputId, releaseAct);
          it = drumPadActiveNotes.erase(it);
        } else {
          ++it;
        }
      }
    }
  }

  // Release touchpad Expression envelopes when no mapping with this
  // (layerId, eventId) has its local finger still active (per-mapping finger
  // counting).
  for (auto it = touchpadExpressionActive.begin();
       it != touchpadExpressionActive.end();) {
    uintptr_t dev = std::get<0>(*it);
    int layerId = std::get<1>(*it);
    int evId = std::get<2>(*it);
    bool fingerActive = false;
    if (dev == deviceHandle && ctx && !ctx->touchpadMappings.empty()) {
      for (size_t mapIdx = 0; mapIdx < ctx->touchpadMappings.size();
           ++mapIdx) {
        const auto &entry = ctx->touchpadMappings[mapIdx];
        if (entry.layerId != layerId ||
            static_cast<int>(entry.eventId) != evId)
          continue;
        MappingLocalState local = buildMappingContactList(entry, mapIdx);
        if ((evId == TouchpadEvent::Finger1Down && local.tip1) ||
            (evId == TouchpadEvent::Finger1Up && !local.tip1) ||
            (evId == TouchpadEvent::Finger2Down && local.tip2) ||
            (evId == TouchpadEvent::Finger2Up && !local.tip2)) {
          fingerActive = true;
          break;
        }
      }
    } else if (dev != deviceHandle) {
      fingerActive = true; // other device: keep entry
    }
    if (!fingerActive) {
      // For Expression CC, send release value when finger lifts. When ADSR is
      // off (fast path) no envelope is in ExpressionEngine so releaseEnvelope
      // does nothing; sending here is the only way to get value-when-off.
      for (const auto &entry : ctx->touchpadMappings) {
        if (entry.layerId == layerId &&
            static_cast<int>(entry.eventId) == evId &&
            entry.action.type == ActionType::Expression &&
            entry.action.adsrSettings.target == AdsrTarget::CC &&
            entry.action.sendReleaseValue) {
          voiceManager.sendCC(entry.action.channel,
                              entry.action.adsrSettings.ccNumber,
                              entry.action.releaseValue);
          break;
        }
      }
      expressionEngine.releaseEnvelope(InputID{dev, evId});
      it = touchpadExpressionActive.erase(it);
    } else {
      ++it;
    }
  }

  prev.tip1 = tip1;
  prev.tip2 = tip2;
  prev.x1 = x1;
  prev.y1 = y1;
  prev.x2 = x2;
  prev.y2 = y2;
}

void InputProcessor::startTouchGlideTimerIfNeeded() {
  for (const auto &kv : touchpadPitchGlideState) {
    if (kv.second.phase == TouchGlidePhase::GlidingToCenter) {
      if (!isTimerRunning())
        startTimer(5);
      return;
    }
  }
  for (const auto &kv : touchpadSlideReturnState) {
    if (kv.second.phase == TouchGlidePhase::GlidingToCenter) {
      if (!isTimerRunning())
        startTimer(5);
      return;
    }
  }
}

void InputProcessor::stopTouchGlideTimerIfIdle() {
  for (const auto &kv : touchpadPitchGlideState) {
    if (kv.second.phase == TouchGlidePhase::GlidingToCenter)
      return;
  }
  for (const auto &kv : touchpadSlideReturnState) {
    if (kv.second.phase == TouchGlidePhase::GlidingToCenter)
      return;
  }
  stopTimer();
}

void InputProcessor::timerCallback() {
  const uint32_t nowMs =
      static_cast<uint32_t>(juce::Time::getMillisecondCounter());
  std::vector<TouchpadPitchGlideKey> toRemove;
  for (auto &kv : touchpadPitchGlideState) {
    if (kv.second.phase != TouchGlidePhase::GlidingToCenter)
      continue;
    TouchGlideState &g = kv.second;
    double progress = (g.durationMs > 0)
        ? std::min(1.0, static_cast<double>(nowMs - g.startTimeMs) /
                            static_cast<double>(g.durationMs))
        : 1.0;
    int output = juce::jlimit(
        0, 16383,
        static_cast<int>(std::round(
            g.startValue + progress * (8192 - g.startValue))));
    if (output != g.lastSentValue) {
      int ch = std::get<3>(kv.first);
      voiceManager.sendPitchBend(ch, output);
      g.lastSentValue = output;
    }
    if (progress >= 1.0) {
      int ch = std::get<3>(kv.first);
      voiceManager.sendPitchBend(ch, 8192);
      lastTouchpadContinuousValues.erase(kv.first);
      toRemove.push_back(kv.first);
    }
  }
  for (const auto &k : toRemove)
    touchpadPitchGlideState.erase(k);

  // Slide CC: glide back to rest value when configured.
  std::vector<TouchpadSlideReturnKey> slideToRemove;
  for (auto &kv : touchpadSlideReturnState) {
    if (kv.second.phase != TouchGlidePhase::GlidingToCenter)
      continue;
    TouchGlideState &g = kv.second;
    double progress = (g.durationMs > 0)
        ? std::min(1.0, static_cast<double>(nowMs - g.startTimeMs) /
                            static_cast<double>(g.durationMs))
        : 1.0;
    const int target = g.targetValue;
    int output = juce::jlimit(
        0, 127,
        static_cast<int>(std::round(
            g.startValue + progress * (target - g.startValue))));
    if (output != g.lastSentValue) {
      int ch = std::get<3>(kv.first);
      int cc = std::get<4>(kv.first);
      voiceManager.sendCC(ch, cc, output);
      g.lastSentValue = output;
      // Keep slide CC last-value map in sync so future gestures pick up from
      // the latest value.
      auto slideKey = std::make_tuple(std::get<0>(kv.first),
                                      std::get<1>(kv.first),
                                      std::get<2>(kv.first), ch, cc);
      lastTouchpadSlideCCValues[slideKey] = output;
    }
    if (progress >= 1.0) {
      slideToRemove.push_back(kv.first);
    }
  }
  for (const auto &k : slideToRemove)
    touchpadSlideReturnState.erase(k);

  stopTouchGlideTimerIfIdle();
}