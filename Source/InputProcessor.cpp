#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "ScaleLibrary.h"
#include "SettingsManager.h"
#include <algorithm>
#include <array>

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
  }
}

void InputProcessor::rebuildGrid() {
  auto newContext = GridCompiler::compile(presetManager, deviceManager,
                                          zoneManager, settingsManager);
  {
    juce::ScopedWriteLock sl(mapLock);
    activeContext = std::move(newContext);
  }
  // Phase 53.7: Layer state under stateLock only
  {
    juce::ScopedLock sl(stateLock);
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

  // Optimization: Only rebuild if relevant properties changed
  if (property == juce::Identifier("inputKey") ||
      property == juce::Identifier("layerID") ||
      property == juce::Identifier("deviceHash") ||
      property == juce::Identifier("inputAlias") ||
      property == juce::Identifier("type") ||
      property == juce::Identifier("channel") ||
      property == juce::Identifier("data1") ||
      property == juce::Identifier("data2") ||
      property == juce::Identifier("velRandom") ||
      property == juce::Identifier("adsrTarget") ||
      property == juce::Identifier("pbRange") ||
      property == juce::Identifier("pbShift") ||
      property == juce::Identifier("adsrAttack") ||
      property == juce::Identifier("adsrDecay") ||
      property == juce::Identifier("adsrSustain") ||
      property == juce::Identifier("adsrRelease") ||
      property == juce::Identifier("useCustomEnvelope") ||
      property == juce::Identifier("sendReleaseValue") ||
      property == juce::Identifier("releaseValue")) {
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
                     midiAction.adsrSettings.target == AdsrTarget::SmartScaleBend)
              voiceManager.sendPitchBend(midiAction.channel, 8192);
          }
          return;
        }
        // Phase 55.4: Note release behavior
        if (midiAction.type == ActionType::Note &&
            midiAction.releaseBehavior == NoteReleaseBehavior::Nothing) {
          return; // Do nothing on release
        }
        if (zone && zone->playMode == Zone::PlayMode::Strum) {
          juce::ScopedWriteLock lock(bufferLock);
          noteBuffer.clear();
          bufferedStrumSpeedMs = 50;
          bool shouldSustain =
              (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain);
          voiceManager.handleKeyUp(input, zone->releaseDurationMs,
                                   shouldSustain);
        } else {
          voiceManager.handleKeyUp(input);
        }
        return; // Stop searching lower layers
      }

      // Key-down handling
      if (midiAction.type == ActionType::Expression) {
        int peakValue = midiAction.data2;
        if (midiAction.adsrSettings.target == AdsrTarget::SmartScaleBend) {
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
        if (cmd == static_cast<int>(OmniKey::CommandID::LayerSolo)) {
          if (isDown) {
            int target = juce::jlimit(0, 8, layerId);
            {
              juce::ScopedLock sl(stateLock);
              std::fill(layerLatchedState.begin(), layerLatchedState.end(),
                        false);
              layerLatchedState[(size_t)target] = true;
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
        else if (cmd == static_cast<int>(OmniKey::CommandID::LatchToggle))
          voiceManager.setLatch(!voiceManager.isLatchActive());
        else if (cmd == static_cast<int>(OmniKey::CommandID::Panic))
          voiceManager.panic();
        else if (cmd == static_cast<int>(OmniKey::CommandID::PanicLatch))
          voiceManager.panicLatch();
        else if (cmd == static_cast<int>(OmniKey::CommandID::GlobalPitchUp)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom + 1, deg);
        } else if (cmd ==
                   static_cast<int>(OmniKey::CommandID::GlobalPitchDown)) {
          int chrom = zoneManager.getGlobalChromaticTranspose();
          int deg = zoneManager.getGlobalDegreeTranspose();
          zoneManager.setGlobalTranspose(chrom - 1, deg);
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
            // Manual mapping: simple playback
            int vel =
                calculateVelocity(midiAction.data2, midiAction.velocityRandom);
            bool alwaysLatch =
                (midiAction.releaseBehavior == NoteReleaseBehavior::AlwaysLatch);
            voiceManager.noteOn(input, midiAction.data1, vel,
                                midiAction.channel, true, 0,
                                PolyphonyMode::Poly, 50, alwaysLatch);
            lastTriggeredNote = midiAction.data1;
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
  bool allowSustain = zone->allowSustain;

  if (chordNotes.has_value() && !chordNotes->empty()) {
    // Calculate per-note velocities with ghost note scaling
    int mainVelocity =
        calculateVelocity(zone->baseVelocity, zone->velocityRandom);

    std::vector<int> finalNotes;
    std::vector<int> finalVelocities;
    finalNotes.reserve(chordNotes->size());
    finalVelocities.reserve(chordNotes->size());

    for (const auto &cn : *chordNotes) {
      finalNotes.push_back(cn.pitch);
      if (cn.isGhost) {
        int ghostVel =
            static_cast<int>(mainVelocity * zone->ghostVelocityScale);
        finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
      } else {
        finalVelocities.push_back(mainVelocity);
      }
    }

    if (zone->playMode == Zone::PlayMode::Direct) {
      int releaseMs = zone->releaseDurationMs;
      int glideSpeed = zone->glideTimeMs;
      if (zone->isAdaptiveGlide &&
          zone->polyphonyMode == PolyphonyMode::Legato) {
        rhythmAnalyzer.logTap();
        glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                     zone->maxGlideTimeMs);
      }

      if (finalNotes.size() > 1) {
        voiceManager.noteOn(input, finalNotes, finalVelocities, action.channel,
                            zone->strumSpeedMs, allowSustain, releaseMs,
                            zone->polyphonyMode, glideSpeed);
        if (!finalNotes.empty())
          lastTriggeredNote = finalNotes.front();
      } else {
        voiceManager.noteOn(input, finalNotes.front(), finalVelocities.front(),
                            action.channel, allowSustain, releaseMs,
                            zone->polyphonyMode, glideSpeed);
        lastTriggeredNote = finalNotes.front();
      }
    } else if (zone->playMode == Zone::PlayMode::Strum) {
      int strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50;
      int glideSpeed = zone->glideTimeMs;
      if (zone->isAdaptiveGlide &&
          zone->polyphonyMode == PolyphonyMode::Legato) {
        rhythmAnalyzer.logTap();
        glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                     zone->maxGlideTimeMs);
      }

      voiceManager.handleKeyUp(lastStrumSource);
      voiceManager.noteOn(input, finalNotes, finalVelocities, action.channel,
                          strumMs, allowSustain, 0, zone->polyphonyMode,
                          glideSpeed);
      lastStrumSource = input;
      if (!finalNotes.empty())
        lastTriggeredNote = finalNotes.front();
      {
        juce::ScopedWriteLock bufferWriteLock(bufferLock);
        noteBuffer = finalNotes;
        bufferedStrumSpeedMs = strumMs;
      }
    }
  } else {
    // Fallback: use action velocity
    int vel = calculateVelocity(action.data2, action.velocityRandom);
    voiceManager.noteOn(input, action.data1, vel, action.channel, allowSustain,
                        0, PolyphonyMode::Poly, 50);
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
  bool allowSustain = zone->allowSustain;

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

  // Use zone's chord notes with ghost note scaling
  int mainVelocity =
      calculateVelocity(zone->baseVelocity, zone->velocityRandom);
  std::vector<int> finalNotes;
  std::vector<int> finalVelocities;
  finalNotes.reserve(chordNotes->size());
  finalVelocities.reserve(chordNotes->size());

  for (const auto &cn : *chordNotes) {
    finalNotes.push_back(cn.pitch);
    if (cn.isGhost) {
      int ghostVel = static_cast<int>(mainVelocity * zone->ghostVelocityScale);
      finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
    } else {
      finalVelocities.push_back(mainVelocity);
    }
  }

  if (zone->playMode == Zone::PlayMode::Direct) {
    int releaseMs = zone->releaseDurationMs;
    int glideSpeed = zone->glideTimeMs;
    if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
      rhythmAnalyzer.logTap();
      glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                   zone->maxGlideTimeMs);
    }
    voiceManager.noteOn(input, finalNotes, finalVelocities, rootAction.channel,
                        zone->strumSpeedMs, allowSustain, releaseMs,
                        zone->polyphonyMode, glideSpeed);
    if (!finalNotes.empty())
      lastTriggeredNote = finalNotes.front();
  } else if (zone->playMode == Zone::PlayMode::Strum) {
    int strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50;
    int glideSpeed = zone->glideTimeMs;
    if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
      rhythmAnalyzer.logTap();
      glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs,
                                                   zone->maxGlideTimeMs);
    }
    voiceManager.handleKeyUp(lastStrumSource);
    voiceManager.noteOn(input, finalNotes, finalVelocities, rootAction.channel,
                        strumMs, allowSustain, 0, zone->polyphonyMode,
                        glideSpeed);
    lastStrumSource = input;
    if (!finalNotes.empty())
      lastTriggeredNote = finalNotes.front();
    {
      juce::ScopedWriteLock bufferWriteLock(bufferLock);
      noteBuffer = finalNotes;
      bufferedStrumSpeedMs = strumMs;
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

void InputProcessor::forceRebuildMappings() { rebuildGrid(); }

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