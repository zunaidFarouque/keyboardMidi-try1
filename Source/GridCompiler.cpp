#include "GridCompiler.h"
#include "MidiNoteUtilities.h"
#include "SettingsManager.h"
#include <algorithm>
#include <numeric>

namespace {

// Helper to convert alias name to hash.
// Must stay in sync with InputProcessor/ZonePropertiesPanel logic.
uintptr_t aliasNameToHash(const juce::String &aliasName) {
  const juce::String trimmed = aliasName.trim();
  if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("Any / Master") ||
      trimmed.equalsIgnoreCase("Global (All Devices)") ||
      trimmed.equalsIgnoreCase("Global") ||
      trimmed.equalsIgnoreCase("Unassigned"))
    return 0; // Hash 0 = Global (All Devices)

  return static_cast<uintptr_t>(std::hash<juce::String>{}(trimmed));
}

// Helper to parse legacy deviceHash property (hex string or int).
uintptr_t parseDeviceHash(const juce::var &var) {
  if (var.isString())
    return (uintptr_t)var.toString().getHexValue64();
  return (uintptr_t)static_cast<juce::int64>(var);
}

// Helpers for generic modifier mapping (VK codes).
bool isGenericShift(int keyCode) { return keyCode == 0x10; }   // VK_SHIFT
bool isGenericControl(int keyCode) { return keyCode == 0x11; } // VK_CONTROL
bool isGenericAlt(int keyCode) { return keyCode == 0x12; }     // VK_MENU

// Create a fresh AudioGrid with all slots inactive.
std::shared_ptr<AudioGrid> makeAudioGrid() {
  auto grid = std::make_shared<AudioGrid>();
  for (auto &slot : *grid) {
    slot.isActive = false;
    slot.chordIndex = -1;
  }
  return grid;
}

// Obtain a mutable AudioGrid from a shared_ptr<const AudioGrid> storage slot.
// This keeps CompiledMapContext read-only for consumers while allowing the
// compiler to fill in data.
std::shared_ptr<AudioGrid>
getMutableAudioGrid(std::shared_ptr<const AudioGrid> &ptr) {
  if (!ptr) {
    auto grid = makeAudioGrid();
    ptr = grid;
    return grid;
  }
  return std::const_pointer_cast<AudioGrid>(ptr);
}

// Create a fresh VisualGrid with default visual state.
std::shared_ptr<VisualGrid> makeVisualGrid() {
  auto grid = std::make_shared<VisualGrid>();
  for (auto &slot : *grid) {
    slot.state = VisualState::Empty;
    slot.displayColor = juce::Colours::transparentBlack;
    slot.label.clear();
    slot.sourceName.clear();
  }
  return grid;
}

// Ensure a VisualGrid exists for a given aliasHash/layer in the context.
std::shared_ptr<VisualGrid> getOrCreateVisualGrid(CompiledMapContext &ctx,
                                                  uintptr_t aliasHash,
                                                  int layerIndex) {
  auto &layerVec = ctx.visualLookup[aliasHash];
  if ((int)layerVec.size() < layerIndex + 1) {
    layerVec.resize(layerIndex + 1);
  }
  if (!layerVec[(size_t)layerIndex]) {
    layerVec[(size_t)layerIndex] = makeVisualGrid();
  }
  return std::const_pointer_cast<VisualGrid>(
      layerVec[(size_t)layerIndex]); // safe: we only store as const
}

// Resolve color for a mapping type, using SettingsManager, with sensible
// fallbacks (Phase 37 + 50.4).
juce::Colour getColorForType(ActionType type,
                             SettingsManager &settingsManager) {
  // Phase 37 colors (mapping-type colors) are exposed via SettingsManager.
  // If anything goes wrong, fall back to hard-coded defaults.
  try {
    return settingsManager.getTypeColor(type);
  } catch (...) {
    // Fall through to defaults below.
  }

  switch (type) {
  case ActionType::Note:
    return juce::Colours::orange;
  case ActionType::CC:
    return juce::Colours::dodgerblue;
  case ActionType::Command:
    return juce::Colours::yellow;
  case ActionType::Macro:
    return juce::Colours::mediumvioletred;
  case ActionType::Envelope:
    return juce::Colours::darkcyan;
  default:
    return juce::Colours::lightgrey;
  }
}

// Human-readable name for a CommandID (fallbacks to numeric if unknown).
juce::String getCommandLabel(int cmdId) {
  using OmniKey::CommandID;
  switch (static_cast<CommandID>(cmdId)) {
  case CommandID::SustainMomentary:
    return "Sustain (Hold)";
  case CommandID::SustainToggle:
    return "Sustain (Toggle)";
  case CommandID::SustainInverse:
    return "Sustain (Inverse)";
  case CommandID::LatchToggle:
    return "Latch";
  case CommandID::Panic:
    return "Panic";
  case CommandID::PanicLatch:
    return "Panic (Latch)";
  case CommandID::GlobalPitchUp:
    return "Global +1";
  case CommandID::GlobalPitchDown:
    return "Global -1";
  case CommandID::GlobalModeUp:
    return "Mode +1";
  case CommandID::GlobalModeDown:
    return "Mode -1";
  case CommandID::LayerMomentary:
    return "Layer (Hold)";
  case CommandID::LayerToggle:
    return "Layer (Toggle)";
  case CommandID::LayerSolo:
    return "Layer Solo";
  default:
    break;
  }
  return "Command " + juce::String(cmdId);
}

// Simple label helper ("C4", "CC 1", "Sustain", etc.) for manual mappings.
juce::String makeLabelForAction(const MidiAction &action) {
  switch (action.type) {
  case ActionType::Note: {
    return MidiNoteUtilities::getMidiNoteName(action.data1);
  }
  case ActionType::CC:
    return "CC " + juce::String(action.data1);
  case ActionType::Command:
    return getCommandLabel(action.data1);
  case ActionType::Macro:
    return "Macro " + juce::String(action.data1);
  case ActionType::Envelope:
    return "Env " + juce::String(action.data1);
  default:
    return {};
  }
}

// Write a MidiAction into an AudioGrid slot and handle generic modifier
// replication for that grid.
void writeAudioSlot(AudioGrid &grid, int keyCode, const MidiAction &action) {
  if (keyCode < 0 || keyCode >= (int)grid.size())
    return;

  auto &slot = grid[(size_t)keyCode];
  slot.isActive = true;
  slot.action = action;
  slot.chordIndex = -1;

  // Generic -> specific replication
  if (isGenericShift(keyCode)) {
    auto &l = grid[(size_t)InputTypes::Key_LShift];
    if (!l.isActive)
      l = slot;
    auto &r = grid[(size_t)InputTypes::Key_RShift];
    if (!r.isActive)
      r = slot;
  } else if (isGenericControl(keyCode)) {
    auto &l = grid[(size_t)InputTypes::Key_LControl];
    if (!l.isActive)
      l = slot;
    auto &r = grid[(size_t)InputTypes::Key_RControl];
    if (!r.isActive)
      r = slot;
  } else if (isGenericAlt(keyCode)) {
    auto &l = grid[(size_t)InputTypes::Key_LAlt];
    if (!l.isActive)
      l = slot;
    auto &r = grid[(size_t)InputTypes::Key_RAlt];
    if (!r.isActive)
      r = slot;
  }
}

// Apply a visual slot with stacking semantics: if the slot already has a value
// (from a lower layer), mark as Override, otherwise Active.
// Phase 50.8: Added conflict detection via touchedKeys vector.
void applyVisualSlot(VisualGrid &grid, int keyCode, const juce::Colour &color,
                     const juce::String &label, const juce::String &sourceName,
                     std::vector<bool> *touchedKeys = nullptr) {
  if (keyCode < 0 || keyCode >= (int)grid.size())
    return;

  auto &slot = grid[(size_t)keyCode];
  const bool hadContent = (slot.state != VisualState::Empty);

  // Phase 50.8: Conflict detection - check if this key was already written to
  // in the current layer pass
  bool isConflict = false;
  if (touchedKeys && keyCode < (int)touchedKeys->size()) {
    if ((*touchedKeys)[(size_t)keyCode]) {
      isConflict = true;
    } else {
      (*touchedKeys)[(size_t)keyCode] = true;
    }
  }

  if (isConflict) {
    // Conflict: Multiple assignments in same layer
    slot.state = VisualState::Conflict;
    slot.displayColor = juce::Colours::red;
    slot.label = label + " (!)";
    slot.sourceName = sourceName;
  } else {
    // Normal assignment
    slot.displayColor = color;
    slot.label = label;
    slot.sourceName = sourceName;
    slot.state = hadContent ? VisualState::Override : VisualState::Active;
  }
}

// Apply a visual slot and replicate to L/R modifier keys if the source key is
// a generic modifier. Phase 51.6: Only expand to L/R if that key is not already
// touched (so a specific mapping for LShift can override the generic Shift).
void applyVisualWithModifiers(VisualGrid &grid, int keyCode,
                              const juce::Colour &color,
                              const juce::String &label,
                              const juce::String &sourceName,
                              std::vector<bool> *touchedKeys = nullptr) {
  applyVisualSlot(grid, keyCode, color, label, sourceName, touchedKeys);

  auto shouldExpandTo = [&](int sideKey) -> bool {
    if (!touchedKeys || sideKey < 0 || sideKey >= (int)touchedKeys->size())
      return true;
    return !(*touchedKeys)[(size_t)sideKey];
  };

  if (isGenericShift(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LShift))
      applyVisualSlot(grid, InputTypes::Key_LShift, color, label, sourceName,
                      touchedKeys);
    if (shouldExpandTo(InputTypes::Key_RShift))
      applyVisualSlot(grid, InputTypes::Key_RShift, color, label, sourceName,
                      touchedKeys);
  } else if (isGenericControl(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LControl))
      applyVisualSlot(grid, InputTypes::Key_LControl, color, label, sourceName,
                      touchedKeys);
    if (shouldExpandTo(InputTypes::Key_RControl))
      applyVisualSlot(grid, InputTypes::Key_RControl, color, label, sourceName,
                      touchedKeys);
  } else if (isGenericAlt(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LAlt))
      applyVisualSlot(grid, InputTypes::Key_LAlt, color, label, sourceName,
                      touchedKeys);
    if (shouldExpandTo(InputTypes::Key_RAlt))
      applyVisualSlot(grid, InputTypes::Key_RAlt, color, label, sourceName,
                      touchedKeys);
  }
}

// Build a MidiAction from a mapping ValueTree (simplified – matches the core
// fields used by manual mappings; envelope/smart-bend details will be handled
// later when we move more logic into the compiler).
MidiAction buildMidiActionFromMapping(juce::ValueTree mappingNode) {
  MidiAction action{};

  // Type
  juce::var typeVar = mappingNode.getProperty("type");
  ActionType actionType = ActionType::Note;
  if (typeVar.isString()) {
    juce::String t = typeVar.toString();
    if (t == "CC")
      actionType = ActionType::CC;
    else if (t == "Command")
      actionType = ActionType::Command;
    else if (t == "Macro")
      actionType = ActionType::Macro;
    else if (t == "Envelope")
      actionType = ActionType::Envelope;
  } else if (typeVar.isInt()) {
    int t = static_cast<int>(typeVar);
    if (t == 1)
      actionType = ActionType::CC;
    else if (t == 2)
      actionType = ActionType::Macro;
    else if (t == 3)
      actionType = ActionType::Command;
    else if (t == 4)
      actionType = ActionType::Envelope;
  }

  action.type = actionType;
  action.channel = (int)mappingNode.getProperty("channel", 1);
  action.data1 = (int)mappingNode.getProperty("data1", 60);
  action.data2 = (int)mappingNode.getProperty("data2", 127);
  action.velocityRandom = (int)mappingNode.getProperty("velRandom", 0);

  // ADSR / SmartScaleBend details intentionally left at defaults here –
  // existing InputProcessor path still handles them; they will be migrated
  // fully into the compiler in later phases.

  return action;
}

// Phase 51.4: Apply zones for a single layer to vGrid and aGrid.
// touchedKeys: conflict detection (same key touched twice in this layer).
// chordPool: append chord vectors for multi-note zones.
void compileZonesForLayer(VisualGrid &vGrid, AudioGrid &aGrid,
                          ZoneManager &zoneMgr, DeviceManager &deviceMgr,
                          uintptr_t aliasHash, int layerId,
                          std::vector<bool> &touchedKeys,
                          std::vector<std::vector<MidiAction>> &chordPool) {
  const int globalChrom = zoneMgr.getGlobalChromaticTranspose();
  const int globalDeg = zoneMgr.getGlobalDegreeTranspose();
  const auto zones = zoneMgr.getZones();

  for (const auto &zone : zones) {
    if (!zone)
      continue;

    const int zoneLayerId = juce::jlimit(0, 8, zone->layerID);
    if (zoneLayerId != layerId)
      continue;

    if (zone->targetAliasHash != aliasHash)
      continue;

    const auto &keyCodes = zone->getInputKeyCodes();
    for (int keyCode : keyCodes) {
      if (keyCode < 0 || keyCode > 0xFF)
        continue;

      auto chordOpt = zone->getNotesForKey(keyCode, globalChrom, globalDeg);
      if (!chordOpt.has_value() || chordOpt->empty()) {
        // Zone covers this key but has no notes (e.g. cache not built yet).
        // Claim the key for conflict detection (Mapping + Zone = Conflict).
        touchedKeys[(size_t)keyCode] = true;
        continue;
      }

      const auto &chordNotes = chordOpt.value();
      juce::Colour color = zone->zoneColor;
      juce::String label = zone->getKeyLabel(keyCode);
      juce::String sourceName = "Zone: " + zone->name;

      applyVisualWithModifiers(vGrid, keyCode, color, label, sourceName,
                               &touchedKeys);

      if (vGrid[(size_t)keyCode].state == VisualState::Conflict)
        continue;

      MidiAction rootAction{};
      rootAction.type = ActionType::Note;
      rootAction.channel = zone->midiChannel;
      rootAction.data1 = chordNotes.front().pitch;
      rootAction.data2 = zone->baseVelocity;
      rootAction.velocityRandom = zone->velocityRandom;

      int chordIndex = -1;
      if (chordNotes.size() > 1) {
        std::vector<MidiAction> chordActions;
        chordActions.reserve(chordNotes.size());
        for (const auto &note : chordNotes) {
          MidiAction a = rootAction;
          a.data1 = note.pitch;
          chordActions.push_back(a);
        }
        chordPool.push_back(std::move(chordActions));
        chordIndex = static_cast<int>(chordPool.size()) - 1;
      }

      writeAudioSlot(aGrid, keyCode, rootAction);
      aGrid[(size_t)keyCode].chordIndex = chordIndex;
    }
  }
}

// Phase 51.6: Specific modifier keys (0xA0-0xA5) should be processed before
// generic (0x10, 0x11, 0x12) so that "LShift -> Note" overrides "Shift -> CC".
static bool isSpecificModifierKey(int keyCode) {
  return (keyCode >= 0xA0 && keyCode <= 0xA5);
}
static bool isGenericModifierKey(int keyCode) {
  return (keyCode == 0x10 || keyCode == 0x11 || keyCode == 0x12);
}

// Phase 51.4: Apply manual mappings for a single layer to vGrid and aGrid.
void compileMappingsForLayer(VisualGrid &vGrid, AudioGrid &aGrid,
                             PresetManager &presetMgr, DeviceManager &deviceMgr,
                             SettingsManager &settingsMgr, uintptr_t aliasHash,
                             int layerId, std::vector<bool> &touchedKeys) {
  auto mappingsNode = presetMgr.getMappingsListForLayer(layerId);
  if (!mappingsNode.isValid())
    return;

  // Phase 51.6: Process specific modifier keys (LShift, RShift, etc.) before
  // generic (Shift, Control, Alt) so specific mappings override expansion.
  std::vector<int> order(mappingsNode.getNumChildren());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    int keyA = (int)mappingsNode.getChild(a).getProperty("inputKey", 0);
    int keyB = (int)mappingsNode.getChild(b).getProperty("inputKey", 0);
    bool specificA = isSpecificModifierKey(keyA);
    bool specificB = isSpecificModifierKey(keyB);
    if (specificA != specificB)
      return specificA; // specific first
    return a < b;
  });

  for (int i : order) {
    auto mapping = mappingsNode.getChild(i);
    if (!mapping.isValid() || !mapping.hasType("Mapping"))
      continue;

    const int inputKey = (int)mapping.getProperty("inputKey", 0);
    if (inputKey < 0 || inputKey > 0xFF)
      continue;

    juce::String aliasName =
        mapping.getProperty("inputAlias", "").toString().trim();
    uintptr_t mappingAliasHash = aliasNameToHash(aliasName);

    juce::var deviceHashVar = mapping.getProperty("deviceHash");
    bool hasDeviceHash =
        !deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty();
    uintptr_t deviceHash = hasDeviceHash ? parseDeviceHash(deviceHashVar) : 0;

    if (mappingAliasHash == 0 && hasDeviceHash && deviceHash != 0) {
      juce::String resolvedAlias = deviceMgr.getAliasForHardware(deviceHash);
      if (resolvedAlias != "Unassigned" && resolvedAlias.isNotEmpty()) {
        mappingAliasHash = aliasNameToHash(resolvedAlias);
      }
      // Unresolved deviceHash: treat as device-specific (only apply when
      // compiling that device). Tests use alias hash as deviceHash.
      if (mappingAliasHash == 0)
        mappingAliasHash = deviceHash;
    }

    if (mappingAliasHash != aliasHash)
      continue;

    MidiAction action = buildMidiActionFromMapping(mapping);
    juce::Colour color = getColorForType(action.type, settingsMgr);
    juce::String label = makeLabelForAction(action);
    juce::String sourceName =
        aliasName.isNotEmpty() ? ("Mapping: " + aliasName) : "Mapping";

    applyVisualWithModifiers(vGrid, inputKey, color, label, sourceName,
                             &touchedKeys);

    if (vGrid[(size_t)inputKey].state != VisualState::Conflict)
      writeAudioSlot(aGrid, inputKey, action);
  }
}

} // namespace

std::shared_ptr<CompiledMapContext>
GridCompiler::compile(PresetManager &presetMgr, DeviceManager &deviceMgr,
                      ZoneManager &zoneMgr, SettingsManager &settingsMgr) {
  // 1. Setup Context
  auto context = std::make_shared<CompiledMapContext>();

  // 2. Define Helper Lambda "applyLayerToGrid"
  // Encapsulates adding Zones + Mappings to a specific grid instance for one
  // layer. Zones first, then Mappings (higher priority in same layer).
  auto applyLayerToGrid = [&](VisualGrid &vGrid, AudioGrid &aGrid, int layerId,
                              uintptr_t aliasHash) {
    std::vector<bool> touchedKeys(256, false);

    compileZonesForLayer(vGrid, aGrid, zoneMgr, deviceMgr, aliasHash, layerId,
                         touchedKeys, context->chordPool);

    compileMappingsForLayer(vGrid, aGrid, presetMgr, deviceMgr, settingsMgr,
                            aliasHash, layerId, touchedKeys);
  };

  // 3. PASS 1: Compile Global Stack (Vertical) – Hash 0 only
  const uintptr_t globalHash = 0;
  context->visualLookup[globalHash].resize(9);

  for (int L = 0; L < 9; ++L) {
    auto vGrid = std::make_shared<VisualGrid>();
    auto aGrid = std::make_shared<AudioGrid>();

    // INHERITANCE: Copy from layer below, then mark as Inherited
    if (L > 0) {
      *vGrid = *context->visualLookup[globalHash][(size_t)(L - 1)];
      *aGrid = *context->globalGrids[(size_t)(L - 1)];

      for (auto &slot : *vGrid) {
        if (slot.state != VisualState::Empty) {
          slot.state = VisualState::Inherited;
          // Dim inherited slots (~0.3f alpha)
          slot.displayColor =
              slot.displayColor.withAlpha(static_cast<juce::uint8>(76));
        }
      }
    }

    applyLayerToGrid(*vGrid, *aGrid, L, globalHash);

    context->visualLookup[globalHash][(size_t)L] = vGrid;
    context->globalGrids[(size_t)L] = aGrid;
  }

  // 4. PASS 2: Compile Device Stacks (Horizontal – Device inherits Global,
  // then applies device-specific layers 0..L)
  juce::StringArray aliases = deviceMgr.getAllAliasNames();
  for (const auto &aliasName : aliases) {
    uintptr_t devHash =
        static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName.trim()));
    if (devHash == 0)
      continue;

    context->visualLookup[devHash].resize(9);

    for (int L = 0; L < 9; ++L) {
      auto vGrid = std::make_shared<VisualGrid>();
      auto aGrid = std::make_shared<AudioGrid>();

      // STEP A: INHERIT FROM GLOBAL AT THIS LAYER
      *vGrid = *context->visualLookup[globalHash][(size_t)L];
      *aGrid = *context->globalGrids[(size_t)L];

      // VISUAL TRANSITION: Global data is "Inherited" from the device's
      // perspective
      for (auto &slot : *vGrid) {
        if (slot.state != VisualState::Empty) {
          slot.state = VisualState::Inherited;
          slot.displayColor =
              slot.displayColor.withAlpha(static_cast<juce::uint8>(76));
        }
      }

      // STEP B: APPLY DEVICE SPECIFIC STACK (0 to L)
      for (int k = 0; k <= L; ++k)
        applyLayerToGrid(*vGrid, *aGrid, k, devHash);

      context->visualLookup[devHash][(size_t)L] = vGrid;
      context->deviceGrids[devHash][(size_t)L] = aGrid;
    }
  }

  return context;
}

// ---------------------------------------------------------------------------
// Phase 50.3 – Zone Compilation & Chord Baking
// ---------------------------------------------------------------------------

void GridCompiler::compileZones(CompiledMapContext &context,
                                ZoneManager &zoneMgr, DeviceManager &deviceMgr,
                                int layerId) {
  const int globalChrom = zoneMgr.getGlobalChromaticTranspose();
  const int globalDeg = zoneMgr.getGlobalDegreeTranspose();

  const auto zones = zoneMgr.getZones();

  for (const auto &zone : zones) {
    if (!zone)
      continue;

    const int zoneLayerId = juce::jlimit(0, 8, zone->layerID);
    if (zoneLayerId != layerId)
      continue;
    const uintptr_t targetAliasHash = zone->targetAliasHash;

    const auto &keyCodes = zone->getInputKeyCodes();
    for (int keyCode : keyCodes) {
      if (keyCode < 0 || keyCode > 0xFF)
        continue;

      auto chordOpt = zone->getNotesForKey(keyCode, globalChrom, globalDeg);
      if (!chordOpt.has_value())
        continue;

      const auto &chordNotes = chordOpt.value();
      if (chordNotes.empty())
        continue;

      // Root action for this slot (first note of chord, or single note).
      MidiAction rootAction{};
      rootAction.type = ActionType::Note;
      rootAction.channel = zone->midiChannel;
      rootAction.data1 = chordNotes.front().pitch;
      rootAction.data2 = zone->baseVelocity;
      rootAction.velocityRandom = zone->velocityRandom;

      int chordIndex = -1;
      if (chordNotes.size() > 1) {
        std::vector<MidiAction> chordActions;
        chordActions.reserve(chordNotes.size());
        for (const auto &note : chordNotes) {
          MidiAction a = rootAction;
          a.data1 = note.pitch;
          // Ghost-note velocity scaling and other nuances will be handled
          // when wiring playback in later phases.
          chordActions.push_back(a);
        }

        context.chordPool.push_back(std::move(chordActions));
        chordIndex = static_cast<int>(context.chordPool.size()) - 1;
      }

      // AUDIO TARGETING -----------------------------------------------------
      auto writeZoneAudioSlot = [&](AudioGrid &grid) {
        if (keyCode < 0 || keyCode >= (int)grid.size())
          return;

        auto &slot = grid[(size_t)keyCode];
        slot.isActive = true;
        slot.action = rootAction;
        slot.chordIndex = chordIndex;

        // Generic modifier replication for zones.
        if (isGenericShift(keyCode)) {
          auto &l = grid[(size_t)InputTypes::Key_LShift];
          if (!l.isActive)
            l = slot;
          auto &r = grid[(size_t)InputTypes::Key_RShift];
          if (!r.isActive)
            r = slot;
        } else if (isGenericControl(keyCode)) {
          auto &l = grid[(size_t)InputTypes::Key_LControl];
          if (!l.isActive)
            l = slot;
          auto &r = grid[(size_t)InputTypes::Key_RControl];
          if (!r.isActive)
            r = slot;
        } else if (isGenericAlt(keyCode)) {
          auto &l = grid[(size_t)InputTypes::Key_LAlt];
          if (!l.isActive)
            l = slot;
          auto &r = grid[(size_t)InputTypes::Key_RAlt];
          if (!r.isActive)
            r = slot;
        }
      };

      if (targetAliasHash == 0) {
        // Global Zone: write to globalGrids[layerId] AND every
        // deviceGrid[layerId].
        auto globalGrid =
            getMutableAudioGrid(context.globalGrids[(size_t)layerId]);
        writeZoneAudioSlot(*globalGrid);

        for (auto &pair : context.deviceGrids) {
          auto deviceGrid = getMutableAudioGrid(pair.second[(size_t)layerId]);
          writeZoneAudioSlot(*deviceGrid);
        }
      } else {
        // Specific Zone: write only to the specific deviceGrids corresponding
        // to this alias, plus its visual grid (below).
        juce::String aliasName = deviceMgr.getAliasName(targetAliasHash);
        if (!aliasName.isEmpty() && aliasName != "Unknown") {
          auto hardwareIds = deviceMgr.getHardwareForAlias(aliasName);
          for (auto hardwareId : hardwareIds) {
            if (hardwareId == 0)
              continue;
            auto &gridPtr = context.deviceGrids[hardwareId][(size_t)layerId];
            auto deviceGrid = getMutableAudioGrid(gridPtr);
            writeZoneAudioSlot(*deviceGrid);
          }
        }
      }

      // VISUALS -------------------------------------------------------------
      auto &layerVec = context.visualLookup[targetAliasHash];
      if (layerId < 0 || layerId >= (int)layerVec.size() ||
          !layerVec[(size_t)layerId]) {
        continue;
      }
      auto &visualGrid =
          *std::const_pointer_cast<VisualGrid>(layerVec[(size_t)layerId]);

      juce::Colour color = zone->zoneColor;
      juce::String label = zone->getKeyLabel(keyCode);
      juce::String sourceName = "Zone: " + zone->name;

      applyVisualWithModifiers(visualGrid, keyCode, color, label, sourceName);
    }
  }
}
