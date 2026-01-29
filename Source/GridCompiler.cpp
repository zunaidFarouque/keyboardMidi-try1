#include "GridCompiler.h"
#include "MidiNoteUtilities.h"
#include "SettingsManager.h"

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
void applyVisualSlot(VisualGrid &grid, int keyCode, const juce::Colour &color,
                     const juce::String &label,
                     const juce::String &sourceName) {
  if (keyCode < 0 || keyCode >= (int)grid.size())
    return;

  auto &slot = grid[(size_t)keyCode];
  const bool hadContent = (slot.state != VisualState::Empty);

  slot.displayColor = color;
  slot.label = label;
  slot.sourceName = sourceName;
  slot.state = hadContent ? VisualState::Override : VisualState::Active;
}

// Apply a visual slot and replicate to L/R modifier keys if the source key is
// a generic modifier, using the same stacking override logic.
void applyVisualWithModifiers(VisualGrid &grid, int keyCode,
                              const juce::Colour &color,
                              const juce::String &label,
                              const juce::String &sourceName) {
  applyVisualSlot(grid, keyCode, color, label, sourceName);

  if (isGenericShift(keyCode)) {
    applyVisualSlot(grid, InputTypes::Key_LShift, color, label, sourceName);
    applyVisualSlot(grid, InputTypes::Key_RShift, color, label, sourceName);
  } else if (isGenericControl(keyCode)) {
    applyVisualSlot(grid, InputTypes::Key_LControl, color, label, sourceName);
    applyVisualSlot(grid, InputTypes::Key_RControl, color, label, sourceName);
  } else if (isGenericAlt(keyCode)) {
    applyVisualSlot(grid, InputTypes::Key_LAlt, color, label, sourceName);
    applyVisualSlot(grid, InputTypes::Key_RAlt, color, label, sourceName);
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

} // namespace

std::shared_ptr<CompiledMapContext>
GridCompiler::compile(PresetManager &presetMgr, DeviceManager &deviceMgr,
                      ZoneManager &zoneMgr, SettingsManager &settingsMgr) {
  auto context = std::make_shared<CompiledMapContext>();

  // -------------------------------------------------------------------------
  // Step 1: Setup – per-device audio layer arrays and visualLookup
  // -------------------------------------------------------------------------

  // Collect all aliases and their hardware, and ensure AudioGrid arrays exist
  // for each device (grids for individual layers are created lazily).
  juce::StringArray aliasNames = deviceMgr.getAllAliasNames();
  std::vector<uintptr_t> aliasHashes;
  aliasHashes.reserve((size_t)aliasNames.size() + 1);
  aliasHashes.push_back(0); // Global (All Devices)

  for (const auto &aliasName : aliasNames) {
    uintptr_t aliasHash = aliasNameToHash(aliasName);
    if (aliasHash != 0)
      aliasHashes.push_back(aliasHash);

    auto hardwareIds = deviceMgr.getHardwareForAlias(aliasName);
    for (auto hardwareId : hardwareIds) {
      if (hardwareId == 0)
        continue;
      // Default-construct the per-device layer array (all null grids) if
      // needed.
      (void)context->deviceGrids[hardwareId];
    }
  }

  const int maxLayers = 9;

  // 1. Initialize Visual Lookup Vectors (aliases) – just size; grids filled
  // during stacking loop.
  for (auto aliasHash : aliasHashes) {
    auto &layerVec = context->visualLookup[aliasHash];
    layerVec.resize(maxLayers);
  }

  // Phase 50.7: Helper to apply zones for a specific alias/layer to a grid
  auto applyZonesForLayer = [&](VisualGrid &grid, uintptr_t aliasHash,
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
      if (targetAliasHash != aliasHash)
        continue;

      const auto &keyCodes = zone->getInputKeyCodes();
      for (int keyCode : keyCodes) {
        if (keyCode < 0 || keyCode > 0xFF)
          continue;

        auto chordOpt = zone->getNotesForKey(keyCode, globalChrom, globalDeg);
        if (!chordOpt.has_value() || chordOpt->empty())
          continue;

        juce::Colour color = zone->zoneColor;
        juce::String label = zone->getKeyLabel(keyCode);
        juce::String sourceName = "Zone: " + zone->name;

        applyVisualWithModifiers(grid, keyCode, color, label, sourceName);
      }
    }
  };

  // Phase 50.7: Helper to apply mappings for a specific alias/layer to a grid
  auto applyMappingsForLayer = [&](VisualGrid &grid, uintptr_t aliasHash,
                                   int layerId) {
    auto mappingsNode = presetMgr.getMappingsListForLayer(layerId);
    if (!mappingsNode.isValid())
      return;

    for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
      auto mapping = mappingsNode.getChild(i);
      if (!mapping.isValid() || !mapping.hasType("Mapping"))
        continue;

      const int inputKey = (int)mapping.getProperty("inputKey", 0);
      if (inputKey < 0 || inputKey > 0xFF)
        continue;

      // Determine targeting information
      juce::String aliasName =
          mapping.getProperty("inputAlias", "").toString().trim();
      uintptr_t mappingAliasHash = aliasNameToHash(aliasName);

      juce::var deviceHashVar = mapping.getProperty("deviceHash");
      bool hasDeviceHash =
          !deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty();
      uintptr_t deviceHash = hasDeviceHash ? parseDeviceHash(deviceHashVar) : 0;

      // If alias empty but deviceHash set, try to resolve alias for display.
      if (mappingAliasHash == 0 && hasDeviceHash && deviceHash != 0) {
        juce::String resolvedAlias = deviceMgr.getAliasForHardware(deviceHash);
        if (resolvedAlias != "Unassigned" && resolvedAlias.isNotEmpty()) {
          mappingAliasHash = aliasNameToHash(resolvedAlias);
        }
      }

      // Only apply if this mapping targets the alias we're compiling
      if (mappingAliasHash != aliasHash)
        continue;

      // Build MidiAction
      MidiAction action = buildMidiActionFromMapping(mapping);

      juce::Colour color = getColorForType(action.type, settingsMgr);
      juce::String label = makeLabelForAction(action);
      juce::String sourceName =
          aliasName.isNotEmpty() ? ("Mapping: " + aliasName) : "Mapping";

      applyVisualWithModifiers(grid, inputKey, color, label, sourceName);
    }
  };

  // Phase 50.7: Helper to apply a single layer's content (zones + mappings)
  auto applyLayerContent = [&](VisualGrid &grid, uintptr_t aliasHash,
                               int layerId) {
    applyZonesForLayer(grid, aliasHash, layerId);
    applyMappingsForLayer(grid, aliasHash, layerId);
  };

  // Phase 50.7: PHASE 1 - Compile Global Chain (Hash 0)
  uintptr_t globalHash = 0;
  for (int L = 0; L < maxLayers; ++L) {
    auto grid = makeVisualGrid();
    if (L > 0) {
      // Inherit from previous Global layer
      auto prevGrid = context->visualLookup[globalHash][(size_t)(L - 1)];
      if (prevGrid) {
        *grid = *prevGrid;
        // Mark everything copied as INHERITED
        for (auto &slot : *grid) {
          if (slot.state == VisualState::Active ||
              slot.state == VisualState::Override) {
            slot.state = VisualState::Inherited;
          }
        }
      }
    }

    // Apply Global zones and mappings for this layer
    applyLayerContent(*grid, globalHash, L);
    context->visualLookup[globalHash][(size_t)L] = grid;
  }

  // Phase 50.7: PHASE 2 - Compile Device Chains
  // For each specific alias, build grids that start with Global[L] and
  // re-apply device layers 0..L
  for (uintptr_t aliasHash : aliasHashes) {
    if (aliasHash == 0)
      continue; // Skip global, already done

    for (int L = 0; L < maxLayers; ++L) {
      // Start with the Global state at this layer (Global[L] > Device[L-1])
      auto globalGrid = context->visualLookup[globalHash][(size_t)L];
      if (!globalGrid) {
        // Fallback: create empty grid if global doesn't exist
        auto grid = makeVisualGrid();
        context->visualLookup[aliasHash][(size_t)L] = grid;
        continue;
      }

      auto grid = std::make_shared<VisualGrid>(*globalGrid);
      // Mark everything from global as INHERITED (device will override)
      for (auto &slot : *grid) {
        if (slot.state == VisualState::Active ||
            slot.state == VisualState::Override) {
          slot.state = VisualState::Inherited;
        }
      }

      // Re-apply specific device stack from 0 to L (Device[K] > Global[K])
      for (int k = 0; k <= L; ++k) {
        applyLayerContent(*grid, aliasHash, k);
      }

      context->visualLookup[aliasHash][(size_t)L] = grid;
    }
  }

  // Phase 50.7: Apply Audio targeting (unchanged - audio doesn't need
  // interleaved stacking)
  for (int layerId = 0; layerId < maxLayers; ++layerId) {
    // B. Apply Zones for this Layer (zones first)
    GridCompiler::compileZones(*context, zoneMgr, deviceMgr, layerId);

    // C. Apply Manual Mappings for this Layer
    auto mappingsNode = presetMgr.getMappingsListForLayer(layerId);
    if (!mappingsNode.isValid())
      continue;

    for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
      auto mapping = mappingsNode.getChild(i);
      if (!mapping.isValid() || !mapping.hasType("Mapping"))
        continue;

      const int inputKey = (int)mapping.getProperty("inputKey", 0);
      if (inputKey < 0 || inputKey > 0xFF)
        continue; // Out of grid range

      // Build MidiAction
      MidiAction action = buildMidiActionFromMapping(mapping);

      // Determine targeting information
      juce::String aliasName =
          mapping.getProperty("inputAlias", "").toString().trim();
      uintptr_t aliasHash = aliasNameToHash(aliasName);

      juce::var deviceHashVar = mapping.getProperty("deviceHash");
      bool hasDeviceHash =
          !deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty();
      uintptr_t deviceHash = hasDeviceHash ? parseDeviceHash(deviceHashVar) : 0;

      // AUDIO TARGETING -----------------------------------------------------
      auto writeToAllDevicesAndGlobal = [&]() {
        // Global grid for this layer
        auto globalGrid =
            getMutableAudioGrid(context->globalGrids[(size_t)layerId]);
        writeAudioSlot(*globalGrid, inputKey, action);
        // All existing device grids for this layer
        for (auto &pair : context->deviceGrids) {
          auto deviceGrid = getMutableAudioGrid(pair.second[(size_t)layerId]);
          writeAudioSlot(*deviceGrid, inputKey, action);
        }
      };

      if ((hasDeviceHash && deviceHash == 0) || aliasHash == 0) {
        // Global mapping: write to global grid and every device grid.
        writeToAllDevicesAndGlobal();
      } else {
        juce::Array<uintptr_t> hardwareTargets;

        if (!aliasName.isEmpty()) {
          hardwareTargets = deviceMgr.getHardwareForAlias(aliasName);
        } else if (hasDeviceHash && deviceHash != 0) {
          hardwareTargets.add(deviceHash);
        }

        for (auto hardwareId : hardwareTargets) {
          if (hardwareId == 0)
            continue;

          auto &gridPtr = context->deviceGrids[hardwareId][(size_t)layerId];
          auto deviceGrid = getMutableAudioGrid(gridPtr);
          writeAudioSlot(*deviceGrid, inputKey, action);
        }

        // If nothing targeted and aliasHash is still 0, treat as global.
        if (hardwareTargets.isEmpty() && aliasHash == 0) {
          writeToAllDevicesAndGlobal();
        }
      }
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
