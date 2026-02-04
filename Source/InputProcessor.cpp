#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "PitchPadUtilities.h"
#include "ScaleLibrary.h"
#include "ScaleUtilities.h"
#include "SettingsManager.h"
#include <algorithm>
#include <array>
#include <cmath>
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
                               SettingsManager &settingsMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr),
      deviceManager(deviceMgr), zoneManager(scaleLib), scaleLibrary(scaleLib),
      expressionEngine(midiEng), settingsManager(settingsMgr) {
  // Phase 53.2: 9 layers; momentary = ref count, latched = persistent
  layerLatchedState.resize(9);
  layerMomentaryCounts.resize(9);
  for (int i = 0; i < 9; ++i) {
    layerLatchedState[(size_t)i] = false;
    layerMomentaryCounts[(size_t)i] = 0;
  }
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

void InputProcessor::initialize() {
  presetManager.getRootNode().addListener(this);
  presetManager.getLayersList().addListener(this);
  presetManager.addChangeListener(this);
  deviceManager.addChangeListener(this);
  settingsManager.addChangeListener(this);
  zoneManager.addChangeListener(this);
  rebuildGrid();
  applySustainDefaultFromPreset();
}

void InputProcessor::runExpressionEngineOneTick() {
  expressionEngine.processOneTick();
}

InputProcessor::~InputProcessor() {
  // Remove listeners
  presetManager.getRootNode().removeListener(this);
  presetManager.getLayersList().removeListener(
      this); // Phase 41: Remove layer listener
  presetManager.removeChangeListener(this);
  deviceManager.removeChangeListener(this);
  settingsManager.removeChangeListener(this);
  zoneManager.removeChangeListener(this);
}

void InputProcessor::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &presetManager || source == &deviceManager ||
      source == &settingsManager || source == &zoneManager) {
    rebuildGrid();
    if (source == &presetManager)
      applySustainDefaultFromPreset(); // Preset load: apply sustain default
  }
}

// Sustain: if any Sustain Inverse (data1=2) is mapped, default sustain = ON;
// otherwise default = OFF and clear sustain state
static bool hasSustainInverseMapped(PresetManager &presetMgr) {
  const int sustainInverse =
      static_cast<int>(OmniKey::CommandID::SustainInverse);
  for (int layer = 0; layer < 9; ++layer) {
    auto mappings = presetMgr.getMappingsListForLayer(layer);
    for (int i = 0; i < mappings.getNumChildren(); ++i) {
      auto m = mappings.getChild(i);
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
  auto newContext = GridCompiler::compile(presetManager, deviceManager,
                                          zoneManager, settingsManager);
  {
    juce::ScopedWriteLock sl(mapLock);
    activeContext = std::move(newContext);
  }
  touchpadNoteOnSent.clear();
  touchpadPrevState.clear();
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
    }
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
  if (property == juce::Identifier("inputKey") ||
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
      property == juce::Identifier("adsrTarget") ||
      property == juce::Identifier("smartStepShift") ||
      property == juce::Identifier("pbRange") ||
      property == juce::Identifier("pbShift") ||
      property == juce::Identifier("adsrAttack") ||
      property == juce::Identifier("adsrDecay") ||
      property == juce::Identifier("adsrSustain") ||
      property == juce::Identifier("adsrRelease") ||
      property == juce::Identifier("useCustomEnvelope") ||
      property == juce::Identifier("sendReleaseValue") ||
      property == juce::Identifier("releaseValue") ||
      property == juce::Identifier("releaseLatchedOnToggleOff")) {
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
          if (cmd == static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
            int target = juce::jlimit(0, 8, midiAction.data2);
            {
              juce::ScopedLock sl(stateLock);
              if (layerMomentaryCounts[(size_t)target] > 0)
                layerMomentaryCounts[(size_t)target]--;
              sendChangeMessage();
            }
            return;
          }
          if (cmd == static_cast<int>(OmniKey::CommandID::SustainMomentary))
            voiceManager.setSustain(false);
          else if (cmd == static_cast<int>(OmniKey::CommandID::SustainInverse))
            voiceManager.setSustain(true);
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
          int range = juce::jmax(1, settingsManager.getPitchBendRange());
          double stepsPerSemitone = 8192.0 / static_cast<double>(range);
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
        if (cmd == static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
          int target = juce::jlimit(0, 8, midiAction.data2);
          {
            juce::ScopedLock sl(stateLock);
            layerMomentaryCounts[(size_t)target]++;
            momentaryLayerHolds[held] = target;
            sendChangeMessage();
          }
          return;
        }
        if (cmd == static_cast<int>(OmniKey::CommandID::LayerToggle)) {
          if (isDown) {
            int target = juce::jlimit(0, 8, layerId);
            {
              juce::ScopedLock sl(stateLock);
              layerLatchedState[(size_t)target] =
                  !layerLatchedState[(size_t)target];
              sendChangeMessage();
            }
            return;
          }
          return;
        }
        if (cmd == static_cast<int>(OmniKey::CommandID::SustainMomentary))
          voiceManager.setSustain(true);
        else if (cmd == static_cast<int>(OmniKey::CommandID::SustainToggle))
          voiceManager.setSustain(!voiceManager.isSustainActive());
        else if (cmd == static_cast<int>(OmniKey::CommandID::SustainInverse))
          voiceManager.setSustain(false);
        else if (cmd == static_cast<int>(OmniKey::CommandID::LatchToggle)) {
          bool wasActive = voiceManager.isLatchActive();
          voiceManager.setLatch(!wasActive);
          if (wasActive && !voiceManager.isLatchActive() &&
              midiAction.releaseLatchedOnLatchToggleOff)
            voiceManager.panicLatch();
        } else if (cmd == static_cast<int>(OmniKey::CommandID::Panic)) {
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
        } else if (cmd == static_cast<int>(OmniKey::CommandID::PanicLatch)) {
          voiceManager.panicLatch(); // Backward compat: old Panic Latch mapping
        } else if (cmd == static_cast<int>(OmniKey::CommandID::Transpose) ||
                   cmd ==
                       static_cast<int>(OmniKey::CommandID::GlobalPitchDown)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          int modify = midiAction.transposeModify;
          if (cmd == static_cast<int>(OmniKey::CommandID::GlobalPitchDown))
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
        } else if (cmd == static_cast<int>(OmniKey::CommandID::GlobalModeUp)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom, deg + 1);
        } else if (cmd ==
                   static_cast<int>(OmniKey::CommandID::GlobalModeDown)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom, deg - 1);
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

  juce::ScopedReadLock rl(mapLock);
  auto ctx = activeContext;
  if (!ctx || ctx->touchpadMappings.empty())
    return;

  TouchpadPrevState &prev = touchpadPrevState[deviceHandle];
  bool tip1 = false, tip2 = false;
  float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
  if (contacts.size() >= 1) {
    tip1 = contacts[0].tipDown;
    x1 = contacts[0].normX;
    y1 = contacts[0].normY;
  }
  if (contacts.size() >= 2) {
    tip2 = contacts[1].tipDown;
    x2 = contacts[1].normX;
    y2 = contacts[1].normY;
  }
  float dist = 0.0f, avgX = x1, avgY = y1;
  if (contacts.size() >= 2) {
    float dx = x2 - x1, dy = y2 - y1;
    dist = std::sqrt(dx * dx + dy * dy);
    avgX = (x1 + x2) * 0.5f;
    avgY = (y1 + y2) * 0.5f;
  }

  bool finger1Down = tip1 && !prev.tip1;
  bool finger1Up = !tip1 && prev.tip1;
  bool finger2Down = tip2 && !prev.tip2;
  bool finger2Up = !tip2 && prev.tip2;

  for (const auto &entry : ctx->touchpadMappings) {
    if (!activeLayersSnapshot[(size_t)entry.layerId])
      continue;

    bool boolVal = false;
    float continuousVal = 0.0f;
    switch (entry.eventId) {
    case TouchpadEvent::Finger1Down:
      boolVal = finger1Down;
      break;
    case TouchpadEvent::Finger1Up:
      boolVal = finger1Up;
      break;
    case TouchpadEvent::Finger1X:
      continuousVal = x1;
      break;
    case TouchpadEvent::Finger1Y:
      continuousVal = y1;
      break;
    case TouchpadEvent::Finger2Down:
      boolVal = finger2Down;
      break;
    case TouchpadEvent::Finger2Up:
      boolVal = finger2Up;
      break;
    case TouchpadEvent::Finger2X:
      continuousVal = x2;
      break;
    case TouchpadEvent::Finger2Y:
      continuousVal = y2;
      break;
    case TouchpadEvent::Finger1And2Dist:
      continuousVal = dist;
      break;
    case TouchpadEvent::Finger1And2AvgX:
      continuousVal = avgX;
      break;
    case TouchpadEvent::Finger1And2AvgY:
      continuousVal = avgY;
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
        bool releaseThisFrame =
            (entry.eventId == TouchpadEvent::Finger1Down && finger1Up) ||
            (entry.eventId == TouchpadEvent::Finger2Down && finger2Up);
        InputID touchpadInput{deviceHandle, 0};
        if (isDownEvent && boolVal)
          triggerManualNoteOn(touchpadInput, act,
                              true); // Latch applies to Down
        else if (releaseThisFrame)
          triggerManualNoteRelease(touchpadInput, act);
        else if (!isDownEvent && boolVal)
          // Finger1Up/Finger2Up: trigger note when finger lifts (one-shot); no
          // Send Note Off, Latch only applies to Down so pass false
          triggerManualNoteOn(touchpadInput, act, false);
      } else if (act.type == ActionType::Command && boolVal) {
        int cmd = act.data1;
        if (cmd == static_cast<int>(OmniKey::CommandID::SustainMomentary))
          voiceManager.setSustain(true);
        else if (cmd == static_cast<int>(OmniKey::CommandID::SustainToggle))
          voiceManager.setSustain(!voiceManager.isSustainActive());
        else if (cmd == static_cast<int>(OmniKey::CommandID::Panic))
          voiceManager.panic();
        else if (cmd == static_cast<int>(OmniKey::CommandID::PanicLatch))
          voiceManager.panicLatch();
      }
      break;
    case TouchpadConversionKind::BoolToCC:
      if (act.type == ActionType::Expression) {
        if (boolVal) {
          InputID touchpadExprInput{deviceHandle,
                                    static_cast<int>(entry.eventId)};
          int peakValue;
          if (act.adsrSettings.target == AdsrTarget::CC) {
            peakValue = act.adsrSettings.valueWhenOn;
          } else if (act.adsrSettings.target == AdsrTarget::PitchBend) {
            int range = juce::jmax(1, settingsManager.getPitchBendRange());
            double stepsPerSemitone = 8192.0 / static_cast<double>(range);
            peakValue =
                static_cast<int>(8192.0 + (act.data2 * stepsPerSemitone));
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
        // Determine whether this continuous event is currently active based on
        // the touch contact state.
        bool eventActive = true;
        switch (entry.eventId) {
        case TouchpadEvent::Finger1X:
        case TouchpadEvent::Finger1Y:
        case TouchpadEvent::Finger1And2AvgX:
        case TouchpadEvent::Finger1And2AvgY:
          eventActive = tip1;
          break;
        case TouchpadEvent::Finger2X:
        case TouchpadEvent::Finger2Y:
          eventActive = tip2;
          break;
        case TouchpadEvent::Finger1And2Dist:
          eventActive = tip1 && tip2;
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

        // If the event is no longer active and we previously sent a value, send
        // the configured release value (for CC) or center pitch (for PB) when
        // requested, then clear the last-value entry so this happens only once.
        if (!eventActive) {
          auto itLast = lastTouchpadContinuousValues.find(keyCont);
          if (itLast != lastTouchpadContinuousValues.end()) {
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
          break;
        }

        float inRange = p.inputMax - p.inputMin;
        float t =
            (inRange > 0.0f) ? (continuousVal - p.inputMin) / inRange : 0.0f;
        t = std::clamp(t, 0.0f, 1.0f);

        int outVal =
            p.outputMin +
            static_cast<int>(std::round(t * (p.outputMax - p.outputMin)));

        // If a pitch-pad config is present and this is a pitch-based target,
        // use the shared pitch-pad layout to derive the discrete step from X
        // instead of direct linear min/max mapping.
        if (p.pitchPadConfig.has_value() &&
            (act.adsrSettings.target == AdsrTarget::PitchBend ||
             act.adsrSettings.target == AdsrTarget::SmartScaleBend)) {
          const PitchPadConfig &cfg = *p.pitchPadConfig;
          PitchPadLayout layout = buildPitchPadLayout(cfg);

          // Base X determined by start position.
          float baseX = 0.5f;
          switch (cfg.start) {
          case PitchPadStart::Left:
            baseX = 0.0f;
            break;
          case PitchPadStart::Center:
            baseX = 0.5f;
            break;
          case PitchPadStart::Right:
            baseX = 1.0f;
            break;
          case PitchPadStart::Custom:
            baseX = cfg.customStartX;
            break;
          }

          float x = t;
          auto relKey = std::make_tuple(deviceHandle, entry.layerId,
                                        entry.eventId, act.channel);

          if (cfg.mode == PitchPadMode::Relative) {
            bool startGesture = false;
            if (entry.eventId == TouchpadEvent::Finger1X ||
                entry.eventId == TouchpadEvent::Finger1Y ||
                entry.eventId == TouchpadEvent::Finger1And2Dist ||
                entry.eventId == TouchpadEvent::Finger1And2AvgX ||
                entry.eventId == TouchpadEvent::Finger1And2AvgY) {
              startGesture = finger1Down;
            } else if (entry.eventId == TouchpadEvent::Finger2X ||
                       entry.eventId == TouchpadEvent::Finger2Y) {
              startGesture = finger2Down;
            }

            if (startGesture) {
              pitchPadRelativeAnchorT[relKey] = t;
              pitchPadRelativeBaseX[relKey] = baseX;
            }

            auto itAnchor = pitchPadRelativeAnchorT.find(relKey);
            auto itBase = pitchPadRelativeBaseX.find(relKey);
            if (itAnchor != pitchPadRelativeAnchorT.end() &&
                itBase != pitchPadRelativeBaseX.end()) {
              float dx = t - itAnchor->second;
              x = itBase->second + dx;
            } else {
              // Fallback: treat as offset from base around center.
              x = baseX + (t - 0.5f);
            }
          } else {
            // Absolute: use normalized coordinate directly.
            x = t;
          }

          x = juce::jlimit(0.0f, 1.0f, x);
          PitchSample sample = mapXToStep(layout, x);
          outVal = sample.step;
        }

        // Change-only sending: only send CC/PB when the quantized value changes
        // for this (device, layer, eventId, channel, ccNumber/-1 for PB).
        if (act.adsrSettings.target == AdsrTarget::CC) {
          outVal = juce::jlimit(0, 127, outVal);
          auto itLast = lastTouchpadContinuousValues.find(keyCont);
          if (itLast != lastTouchpadContinuousValues.end() &&
              itLast->second == outVal) {
            break; // No CC change – skip send
          }
          voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, outVal);
          lastTouchpadContinuousValues[keyCont] = outVal;
        } else {
          // Pitch-based targets: interpret outVal as a discrete step offset.
          int pbRange = juce::jmax(1, settingsManager.getPitchBendRange());

          int pbVal = 8192;
          if (act.adsrSettings.target == AdsrTarget::SmartScaleBend) {
            // SmartScale: convert scale step offset -> PB using current note
            // and global scale.
            int currentNote = voiceManager.getCurrentPlayingNote(act.channel);
            if (currentNote >= 0) {
              std::vector<int> intervals =
                  zoneManager.getGlobalScaleIntervals();
              if (intervals.empty())
                intervals = {0, 2, 4, 5, 7, 9, 11}; // Major fallback
              int root = zoneManager.getGlobalRootNote();
              pbVal = ScaleUtilities::smartStepOffsetToPitchBend(
                  currentNote, root, intervals, outVal, pbRange);
            }
          } else {
            // Standard PitchBend: treat outVal directly as semitone offset.
            int clampedSteps = juce::jlimit(-pbRange, pbRange, outVal);
            double stepsPerSemitone = 8192.0 / static_cast<double>(pbRange);
            pbVal =
                static_cast<int>(8192.0 + (clampedSteps * stepsPerSemitone));
            pbVal = juce::jlimit(0, 16383, pbVal);
          }

          auto itLast = lastTouchpadContinuousValues.find(keyCont);
          if (itLast != lastTouchpadContinuousValues.end() &&
              itLast->second == pbVal) {
            break; // No PB change – skip send
          }
          voiceManager.sendPitchBend(act.channel, pbVal);
          lastTouchpadContinuousValues[keyCont] = pbVal;
        }
      }
      break;
    }
  }

  // Release touchpad Expression envelopes when finger is no longer active
  for (auto it = touchpadExpressionActive.begin();
       it != touchpadExpressionActive.end();) {
    uintptr_t dev = std::get<0>(*it);
    int evId = std::get<2>(*it);
    bool fingerActive = (evId == TouchpadEvent::Finger1Down && tip1) ||
                        (evId == TouchpadEvent::Finger1Up && !tip1) ||
                        (evId == TouchpadEvent::Finger2Down && tip2) ||
                        (evId == TouchpadEvent::Finger2Up && !tip2);
    if (!fingerActive) {
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