#include "GridCompiler.h"
#include "MappingDefinition.h"
#include "MappingDefaults.h"
#include "MidiNoteUtilities.h"
#include "PitchPadUtilities.h"
#include "ScaleUtilities.h"
#include "SettingsManager.h"
#include "TouchpadMixerTypes.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

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

// Build SmartScaleBend lookup: for each MIDI note 0-127, PB value to reach
// (note + scale step) using global scale and PB range.
void buildSmartBendLookup(MidiAction &action, const juce::ValueTree &mapping,
                          ZoneManager &zoneMgr, SettingsManager &settingsMgr) {
  int stepShift = (int)mapping.getProperty("smartStepShift", MappingDefaults::SmartStepShift);
  std::vector<int> intervals = zoneMgr.getGlobalScaleIntervals();
  if (intervals.empty())
    intervals = {0, 2, 4, 5, 7, 9, 11}; // Major
  int root = zoneMgr.getGlobalRootNote();
  int pbRange = settingsMgr.getPitchBendRange();
  if (pbRange < 1)
    pbRange = 12;

  action.smartBendLookup.resize(128);
  for (int note = 0; note < 128; ++note) {
    int degree = ScaleUtilities::findScaleDegree(note, root, intervals);
    int targetDegree = degree + stepShift;
    int targetNote =
        ScaleUtilities::calculateMidiNote(root, intervals, targetDegree);
    int semitones = targetNote - note;
    double frac = (pbRange > 0) ? (semitones / (double)pbRange) : 0.0;
    int pbValue = 8192 + static_cast<int>(std::round(frac * 8192));
    action.smartBendLookup[(size_t)note] = juce::jlimit(0, 16383, pbValue);
  }
}

// Helpers for generic modifier mapping (VK codes).
bool isGenericShift(int keyCode) { return keyCode == 0x10; }   // VK_SHIFT
bool isGenericControl(int keyCode) { return keyCode == 0x11; } // VK_CONTROL
bool isGenericAlt(int keyCode) { return keyCode == 0x12; }     // VK_MENU

// Layer inheritance: mark key (and generic modifier expansion) as written by
// this layer for "private to layer" stripping when building the next layer.
static void markKeyWritten(int keyCode, std::vector<bool> *keysWrittenOut) {
  if (!keysWrittenOut || keyCode < 0 || keyCode >= (int)keysWrittenOut->size())
    return;
  (*keysWrittenOut)[(size_t)keyCode] = true;
  if (isGenericShift(keyCode)) {
    (*keysWrittenOut)[(size_t)InputTypes::Key_LShift] = true;
    (*keysWrittenOut)[(size_t)InputTypes::Key_RShift] = true;
  } else if (isGenericControl(keyCode)) {
    (*keysWrittenOut)[(size_t)InputTypes::Key_LControl] = true;
    (*keysWrittenOut)[(size_t)InputTypes::Key_RControl] = true;
  } else if (isGenericAlt(keyCode)) {
    (*keysWrittenOut)[(size_t)InputTypes::Key_LAlt] = true;
    (*keysWrittenOut)[(size_t)InputTypes::Key_RAlt] = true;
  }
}

// Clear audio and visual slots for keys where keysToClear[k] is true (used for
// "private to layer" so higher layers do not inherit those keys).
static void clearSlotsForKeys(AudioGrid &aGrid, VisualGrid &vGrid,
                              const std::vector<bool> &keysToClear) {
  const int n = static_cast<int>(std::min(keysToClear.size(), aGrid.size()));
  for (int k = 0; k < n; ++k) {
    if (!keysToClear[(size_t)k])
      continue;
    aGrid[(size_t)k].isActive = false;
    aGrid[(size_t)k].chordIndex = -1;
    vGrid[(size_t)k].state = VisualState::Empty;
    vGrid[(size_t)k].displayColor = juce::Colours::transparentBlack;
    vGrid[(size_t)k].label.clear();
    vGrid[(size_t)k].sourceName.clear();
  }
}

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
  case ActionType::Expression:
    return juce::Colours::dodgerblue;
  case ActionType::Command:
    return juce::Colours::yellow;
  case ActionType::Macro:
    return juce::Colours::mediumvioletred;
  default:
    return juce::Colours::lightgrey;
  }
}

// Human-readable name for a CommandID (fallbacks to numeric if unknown).
juce::String getCommandLabel(int cmdId) {
  using MIDIQy::CommandID;
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
    return "Panic (Latch)"; // Backward compat
  case CommandID::Transpose:
    return "Transpose";
  case CommandID::GlobalPitchDown:
    return "Transpose (legacy)";
  case CommandID::GlobalModeUp:
    return "Mode +1";
  case CommandID::GlobalModeDown:
    return "Mode -1";
  case CommandID::LayerMomentary:
    return "Layer (Hold)";
  case CommandID::LayerToggle:
    return "Layer (Toggle)";
  case CommandID::GlobalRootUp:
    return "Root +1";
  case CommandID::GlobalRootDown:
    return "Root -1";
  case CommandID::GlobalRootSet:
    return "Root Set";
  case CommandID::GlobalScaleNext:
    return "Scale Next";
  case CommandID::GlobalScalePrev:
    return "Scale Prev";
  case CommandID::GlobalScaleSet:
    return "Scale Set";
  case CommandID::TouchpadLayoutGroupSoloMomentary:
    return "Touchpad Solo (Hold)";
  case CommandID::TouchpadLayoutGroupSoloToggle:
    return "Touchpad Solo (Toggle)";
  case CommandID::TouchpadLayoutGroupSoloSet:
    return "Touchpad Solo (Set)";
  case CommandID::TouchpadLayoutGroupSoloClear:
    return "Touchpad Solo (Clear)";
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
  case ActionType::Expression: {
    juce::String target = "CC";
    if (action.adsrSettings.target == AdsrTarget::PitchBend)
      target = "PB";
    else if (action.adsrSettings.target == AdsrTarget::SmartScaleBend)
      target = "Smart";
    return "Expr: " + target;
  }
  case ActionType::Command: {
    juce::String base = getCommandLabel(action.data1);
    if (action.data1 == static_cast<int>(MIDIQy::CommandID::Panic) &&
        action.data2 == 1)
      return "Panic (Latch)";
    if (action.data1 == static_cast<int>(MIDIQy::CommandID::Panic) &&
        action.data2 == 2)
      return "Panic (Chords)";
    return base;
  }
  case ActionType::Macro:
    return "Macro " + juce::String(action.data1);
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

// Phase 53.5: targetState = Active for "current layer" content, Inherited for
// "lower layer" content (device Pass 2). When slot already has content, Active
// -> Override, Inherited -> stays Inherited.
void applyVisualSlot(VisualGrid &grid, int keyCode, const juce::Colour &color,
                     const juce::String &label, const juce::String &sourceName,
                     std::vector<bool> *touchedKeys = nullptr,
                     VisualState targetState = VisualState::Active) {
  if (keyCode < 0 || keyCode >= (int)grid.size())
    return;

  auto &slot = grid[(size_t)keyCode];
  const bool hadContent = (slot.state != VisualState::Empty);

  bool isConflict = false;
  if (touchedKeys && keyCode < (int)touchedKeys->size()) {
    if ((*touchedKeys)[(size_t)keyCode]) {
      isConflict = true;
    } else {
      (*touchedKeys)[(size_t)keyCode] = true;
    }
  }

  if (isConflict) {
    slot.state = VisualState::Conflict;
    slot.displayColor = juce::Colours::red;
    slot.label = label + " (!)";
    slot.sourceName = sourceName;
  } else {
    slot.displayColor = color;
    slot.label = label;
    slot.sourceName = sourceName;
    if (!hadContent)
      slot.state = targetState;
    else
      slot.state = (targetState == VisualState::Active)
                       ? VisualState::Override
                       : VisualState::Inherited;
  }
}

// Apply a visual slot and replicate to L/R modifier keys if the source key is
// a generic modifier. Phase 53.5: targetState passed through for inheritance.
void applyVisualWithModifiers(VisualGrid &grid, int keyCode,
                              const juce::Colour &color,
                              const juce::String &label,
                              const juce::String &sourceName,
                              std::vector<bool> *touchedKeys = nullptr,
                              VisualState targetState = VisualState::Active) {
  applyVisualSlot(grid, keyCode, color, label, sourceName, touchedKeys,
                  targetState);

  auto shouldExpandTo = [&](int sideKey) -> bool {
    if (!touchedKeys || sideKey < 0 || sideKey >= (int)touchedKeys->size())
      return true;
    return !(*touchedKeys)[(size_t)sideKey];
  };

  if (isGenericShift(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LShift))
      applyVisualSlot(grid, InputTypes::Key_LShift, color, label, sourceName,
                      touchedKeys, targetState);
    if (shouldExpandTo(InputTypes::Key_RShift))
      applyVisualSlot(grid, InputTypes::Key_RShift, color, label, sourceName,
                      touchedKeys, targetState);
  } else if (isGenericControl(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LControl))
      applyVisualSlot(grid, InputTypes::Key_LControl, color, label, sourceName,
                      touchedKeys, targetState);
    if (shouldExpandTo(InputTypes::Key_RControl))
      applyVisualSlot(grid, InputTypes::Key_RControl, color, label, sourceName,
                      touchedKeys, targetState);
  } else if (isGenericAlt(keyCode)) {
    if (shouldExpandTo(InputTypes::Key_LAlt))
      applyVisualSlot(grid, InputTypes::Key_LAlt, color, label, sourceName,
                      touchedKeys, targetState);
    if (shouldExpandTo(InputTypes::Key_RAlt))
      applyVisualSlot(grid, InputTypes::Key_RAlt, color, label, sourceName,
                      touchedKeys, targetState);
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
    if (t == "Expression")
      actionType = ActionType::Expression;
    else if (t == "Command")
      actionType = ActionType::Command;
    else if (t == "Macro")
      actionType = ActionType::Macro;
  } else if (typeVar.isInt()) {
    int t = static_cast<int>(typeVar);
    if (t == 1)
      actionType = ActionType::Expression;
    else if (t == 2)
      actionType = ActionType::Macro;
    else if (t == 3)
      actionType = ActionType::Command;
  }

  action.type = actionType;
  action.channel = (int)mappingNode.getProperty("channel", MappingDefaults::Channel);
  action.data1 = (int)mappingNode.getProperty("data1", MappingDefaults::Data1);
  action.data2 = (int)mappingNode.getProperty("data2", MappingDefaults::Data2);
  action.velocityRandom = (int)mappingNode.getProperty("velRandom", MappingDefaults::VelRandom);

  // ADSR / SmartScaleBend details intentionally left at defaults here –
  // existing InputProcessor path still handles them; they will be migrated
  // fully into the compiler in later phases.

  return action;
}

// Apply Note options (followTranspose, releaseBehavior) from mapping to action.
// Shared by keyboard and touchpad mapping paths.
static void applyNoteOptionsFromMapping(juce::ValueTree mapping,
                                        ZoneManager &zoneMgr,
                                        MidiAction &action) {
  if (action.type != ActionType::Note)
    return;
  bool followTranspose = (bool)mapping.getProperty("followTranspose", true);
  if (followTranspose) {
    int chrom = zoneMgr.getGlobalChromaticTranspose();
    action.data1 = juce::jlimit(0, 127, action.data1 + chrom);
  }
  juce::String rbStr =
      mapping.getProperty("releaseBehavior", MappingDefaults::ReleaseBehaviorSendNoteOff).toString().trim();
  if (rbStr.equalsIgnoreCase("Sustain until retrigger"))
    action.releaseBehavior = NoteReleaseBehavior::SustainUntilRetrigger;
  else if (rbStr.equalsIgnoreCase("Always Latch"))
    action.releaseBehavior = NoteReleaseBehavior::AlwaysLatch;
  else
    action.releaseBehavior = NoteReleaseBehavior::SendNoteOff;
}

// Phase 51.4 / 53.5: Apply zones for a single layer. targetState = Active or
// Inherited for device Pass 2. keysWrittenOut: if non-null, mark keys written
// by this layer (for "private to layer" inheritance stripping).
void compileZonesForLayer(VisualGrid &vGrid, AudioGrid &aGrid,
                          ZoneManager &zoneMgr, DeviceManager &deviceMgr,
                          uintptr_t aliasHash, int layerId,
                          std::vector<bool> &touchedKeys,
                          std::vector<std::vector<MidiAction>> &chordPool,
                          VisualState targetState,
                          std::vector<bool> *keysWrittenOut = nullptr) {
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

    std::vector<int> zoneIntervals =
        zoneMgr.getScaleIntervalsForZone(zone.get());
    const auto &keyCodes = zone->getInputKeyCodes();
    for (int keyCode : keyCodes) {
      if (keyCode < 0 || keyCode > 0xFF)
        continue;

      auto chordOpt =
          zone->getNotesForKey(keyCode, globalChrom, globalDeg, &zoneIntervals);
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
                               &touchedKeys, targetState);

      if (!chordNotes.empty() && chordNotes.front().isGhost)
        vGrid[(size_t)keyCode].isGhost = true;

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
      markKeyWritten(keyCode, keysWrittenOut);
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

// Phase 53.5: Layer switching commands must not be inherited to higher layers.
static bool isLayerCommand(const MidiAction &action) {
  if (action.type != ActionType::Command)
    return false;
  const int cmd = action.data1;
  return (cmd == static_cast<int>(MIDIQy::CommandID::LayerMomentary) ||
          cmd == static_cast<int>(MIDIQy::CommandID::LayerToggle));
}

static bool isTouchpadEventBoolean(int eventId) {
  return (eventId == TouchpadEvent::Finger1Down ||
          eventId == TouchpadEvent::Finger1Up ||
          eventId == TouchpadEvent::Finger2Down ||
          eventId == TouchpadEvent::Finger2Up);
}

struct ForcedMapping {
  int inputKey{};
  MidiAction action{};
  juce::Colour color;
  juce::String label;
  juce::String sourceName;
};

static void collectForcedMappings(
    PresetManager &presetMgr, DeviceManager &deviceMgr, ZoneManager &zoneMgr,
    SettingsManager &settingsMgr,
    std::unordered_map<uintptr_t, std::vector<ForcedMapping>> &forcedByAlias) {
  forcedByAlias.clear();

  std::vector<juce::ValueTree> baseList =
      presetMgr.getEnabledMappingsForLayer(0);
  for (auto mapping : baseList) {
    if (!mapping.isValid() || !mapping.hasType("Mapping"))
      continue;

    const bool forceAllLayers =
        (bool)mapping.getProperty("forceAllLayers", false);
    if (!forceAllLayers)
      continue;

    juce::String aliasName =
        mapping.getProperty("inputAlias", "").toString().trim();
    if (aliasName.equalsIgnoreCase("Touchpad"))
      continue;

    const int inputKey = (int)mapping.getProperty("inputKey", MappingDefaults::InputKey);
    if (inputKey < 0 || inputKey > 0xFF)
      continue;

    uintptr_t mappingAliasHash = aliasNameToHash(aliasName);

    juce::var deviceHashVar = mapping.getProperty("deviceHash");
    bool hasDeviceHash =
        !deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty();
    uintptr_t deviceHash =
        hasDeviceHash ? parseDeviceHash(deviceHashVar) : 0;

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

    MidiAction action = buildMidiActionFromMapping(mapping);
    applyNoteOptionsFromMapping(mapping, zoneMgr, action);

    if (action.type == ActionType::Expression) {
      juce::String adsrTargetStr =
          mapping.getProperty("adsrTarget", MappingDefaults::AdsrTargetCC).toString().trim();
      bool useCustomEnvelope =
          (bool)mapping.getProperty("useCustomEnvelope", false);

      if (adsrTargetStr.equalsIgnoreCase("PitchBend"))
        action.adsrSettings.target = AdsrTarget::PitchBend;
      else if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend"))
        action.adsrSettings.target = AdsrTarget::SmartScaleBend;
      else
        action.adsrSettings.target = AdsrTarget::CC;

      bool isPB = action.adsrSettings.target == AdsrTarget::PitchBend;
      bool isSmartBend =
          action.adsrSettings.target == AdsrTarget::SmartScaleBend;
      action.adsrSettings.useCustomEnvelope =
          useCustomEnvelope && !isPB && !isSmartBend;

      if (!useCustomEnvelope) {
        action.adsrSettings.attackMs = 0;
        action.adsrSettings.decayMs = 0;
        action.adsrSettings.sustainLevel = 1.0f;
        action.adsrSettings.releaseMs = 0;
      } else {
        action.adsrSettings.attackMs =
            (int)mapping.getProperty("adsrAttack", MappingDefaults::ADSRAttackMs);
        action.adsrSettings.decayMs =
            (int)mapping.getProperty("adsrDecay", MappingDefaults::ADSRDecayMs);
        action.adsrSettings.sustainLevel =
            (float)mapping.getProperty("adsrSustain", MappingDefaults::ADSRSustain);
        action.adsrSettings.releaseMs =
            (int)mapping.getProperty("adsrRelease", MappingDefaults::ADSRReleaseMs);
      }

      if (action.adsrSettings.target == AdsrTarget::CC) {
        action.adsrSettings.ccNumber = (int)mapping.getProperty("data1", MappingDefaults::ExpressionData1);
        action.adsrSettings.valueWhenOn =
            (int)mapping.getProperty("touchpadValueWhenOn", MappingDefaults::TouchpadValueWhenOn);
        action.adsrSettings.valueWhenOff =
            (int)mapping.getProperty("touchpadValueWhenOff", MappingDefaults::TouchpadValueWhenOff);
        action.data2 = action.adsrSettings.valueWhenOn;
      } else if (action.adsrSettings.target == AdsrTarget::PitchBend) {
        const int pbRange = settingsMgr.getPitchBendRange();
        int semitones = (int)mapping.getProperty("data2", 0);
        action.data2 = juce::jlimit(-juce::jmax(1, pbRange),
                                    juce::jmax(1, pbRange), semitones);
      } else if (action.adsrSettings.target == AdsrTarget::SmartScaleBend) {
        buildSmartBendLookup(action, mapping, zoneMgr, settingsMgr);
        action.data2 = 8192;
      }
      bool defaultResetPitch =
          (action.adsrSettings.target == AdsrTarget::PitchBend ||
           action.adsrSettings.target == AdsrTarget::SmartScaleBend);
      action.sendReleaseValue =
          (bool)mapping.getProperty("sendReleaseValue", defaultResetPitch);
      action.releaseValue = (int)mapping.getProperty("releaseValue", MappingDefaults::ReleaseValue);
    }

    if (action.type == ActionType::Command) {
      if (action.data1 == static_cast<int>(MIDIQy::CommandID::LatchToggle)) {
        action.releaseLatchedOnLatchToggleOff =
            (bool)mapping.getProperty("releaseLatchedOnToggleOff", true);
      }

      if (action.data1 ==
              static_cast<int>(
                  MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary) ||
          action.data1 ==
              static_cast<int>(
                  MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle) ||
          action.data1 ==
              static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet) ||
          action.data1 ==
              static_cast<int>(
                  MIDIQy::CommandID::TouchpadLayoutGroupSoloClear)) {
        action.touchpadLayoutGroupId =
            (int)mapping.getProperty("touchpadLayoutGroupId", MappingDefaults::TouchpadLayoutGroupId);
        action.touchpadSoloScope =
            juce::jlimit(0, 2, (int)mapping.getProperty("touchpadSoloScope", MappingDefaults::TouchpadSoloScope));
      }
    }

    if (action.type == ActionType::Command &&
        (action.data1 == static_cast<int>(MIDIQy::CommandID::Transpose) ||
         action.data1 ==
             static_cast<int>(MIDIQy::CommandID::GlobalPitchDown))) {
      juce::String modeStr =
          mapping.getProperty("transposeMode", "Global").toString();
      action.transposeLocal = modeStr.equalsIgnoreCase("Local");
      int modify = (int)mapping.getProperty("transposeModify", MappingDefaults::TransposeModify);
      if (action.data1 ==
          static_cast<int>(MIDIQy::CommandID::GlobalPitchDown)) {
        modify = 1;
      }
      action.transposeModify = juce::jlimit(0, 4, modify);
      action.transposeSemitones =
          (int)mapping.getProperty("transposeSemitones", MappingDefaults::TransposeSemitones);
      action.transposeSemitones =
          juce::jlimit(-48, 48, action.transposeSemitones);
    }

    if (action.type == ActionType::Command) {
      const int cmd = action.data1;
      if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootUp) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootDown) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootSet)) {
        int rm = (int)mapping.getProperty("rootModify", MappingDefaults::RootModify);
        action.rootModify = juce::jlimit(0, 2, rm);
        action.rootNote =
            juce::jlimit(0, 127, (int)mapping.getProperty("rootNote", MappingDefaults::RootNote));
      }
      if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalScaleNext) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalScalePrev) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalScaleSet)) {
        int sm = (int)mapping.getProperty("scaleModify", MappingDefaults::ScaleModify);
        action.scaleModify = juce::jlimit(0, 2, sm);
        action.scaleIndex =
            juce::jmax(0, (int)mapping.getProperty("scaleIndex", MappingDefaults::ScaleIndex));
      }
    }

    juce::Colour color = getColorForType(action.type, settingsMgr);
    juce::String label = makeLabelForAction(action);
    juce::String sourceName =
        aliasName.isNotEmpty() ? ("Mapping: " + aliasName) : "Mapping";

    forcedByAlias[mappingAliasHash].push_back(
        ForcedMapping{inputKey, action, color, label, sourceName});
  }
}

// Compile one touchpad mapping ValueTree into a TouchpadMappingEntry and append
// it to out. Used by the Touchpad tab (TouchpadMixerManager). Channel is taken
// from the header (headerChannel), not from the mapping ValueTree.
// If region is non-null, entry region is set from it; otherwise full pad (0,0,1,1).
// layoutGroupId / zIndex / regionLock are propagated from the Touchpad tab
// header so runtime + visualizer can apply the same group/stacking rules as
// layouts and respect per-mapping region lock.
static void compileTouchpadMappingFromValueTree(
    const juce::ValueTree &mapping, int layerId, int headerChannel,
    int layoutGroupId, int zIndex, bool regionLock,
    ZoneManager &zoneMgr, SettingsManager &settingsMgr,
    std::vector<TouchpadMappingEntry> &out,
    const TouchpadLayoutRegion *region = nullptr) {
  if (!mapping.isValid() || !mapping.hasType("Mapping"))
    return;
  
  // Skip disabled mappings
  if (!MappingDefinition::isMappingEnabled(mapping))
    return;

  int eventId = (int)mapping.getProperty("inputTouchpadEvent", MappingDefaults::InputTouchpadEvent);
  eventId = juce::jlimit(0, TouchpadEvent::Count - 1, eventId);

  MidiAction action = buildMidiActionFromMapping(mapping);
  // Channel always from header for touchpad mappings
  action.channel = juce::jlimit(1, 16, headerChannel);

  TouchpadMappingEntry entry;
  entry.layerId = layerId;
  entry.eventId = eventId;
  entry.layoutGroupId = juce::jmax(0, layoutGroupId);
  entry.zIndex = zIndex;
  entry.regionLock = regionLock;
  entry.action = std::move(action);
  if (region) {
    entry.regionLeft = region->left;
    entry.regionTop = region->top;
    entry.regionRight = region->right;
    entry.regionBottom = region->bottom;
    float rw = region->right - region->left;
    float rh = region->bottom - region->top;
    entry.invRegionWidth = (rw > 0.0f) ? (1.0f / rw) : 1.0f;
    entry.invRegionHeight = (rh > 0.0f) ? (1.0f / rh) : 1.0f;
  }
  TouchpadConversionParams &p = entry.conversionParams;

  juce::String typeStr =
      mapping.getProperty("type", "Note").toString().trim();
  const bool inputBool = isTouchpadEventBoolean(eventId);

  if (typeStr.equalsIgnoreCase("Note")) {
    applyNoteOptionsFromMapping(mapping, zoneMgr, entry.action);
    // Apply touchpad-specific hold behavior
    juce::String holdBehaviorStr = mapping.getProperty("touchpadHoldBehavior",
        MappingDefaults::TouchpadHoldBehaviorHold).toString().trim();
    if (holdBehaviorStr.equalsIgnoreCase("Ignore, send note off immediately"))
      entry.action.touchpadHoldBehavior =
          TouchpadHoldBehavior::IgnoreSendNoteOffImmediately;
    else
      entry.action.touchpadHoldBehavior =
          TouchpadHoldBehavior::HoldToNotSendNoteOffImmediately;
    if (inputBool) {
      entry.conversionKind = TouchpadConversionKind::BoolToGate;
    } else {
      entry.conversionKind = TouchpadConversionKind::ContinuousToGate;
      p.threshold = (float)mapping.getProperty("touchpadThreshold", static_cast<double>(MappingDefaults::TouchpadThreshold));
      int triggerId = (int)mapping.getProperty("touchpadTriggerAbove", MappingDefaults::TouchpadTriggerAbove);
      p.triggerAbove = (triggerId == 2);
    }
  } else if (typeStr.equalsIgnoreCase("Expression")) {
    juce::String adsrTargetStr =
        mapping.getProperty("adsrTarget", MappingDefaults::AdsrTargetCC).toString().trim();
    const bool isCC = adsrTargetStr.equalsIgnoreCase("CC");
    const bool isPB = adsrTargetStr.equalsIgnoreCase("PitchBend");
    const bool isSmartBend =
        adsrTargetStr.equalsIgnoreCase("SmartScaleBend");
    juce::String expressionCCModeStr =
        mapping.getProperty("expressionCCMode", MappingDefaults::ExpressionCCModePosition).toString().trim();

    if (isCC && expressionCCModeStr.equalsIgnoreCase("Slide")) {
      // Slide requires continuous X/Y events at runtime. New touchpad
      // mappings default to Finger1Down; auto-promote that to Finger1Y so CC
      // actually emits without requiring the user to find an event selector.
      if (eventId == TouchpadEvent::Finger1Down || eventId == TouchpadEvent::Finger1Up ||
          eventId == TouchpadEvent::Finger2Down || eventId == TouchpadEvent::Finger2Up) {
        entry.eventId = TouchpadEvent::Finger1Y;
      }
      entry.conversionKind = TouchpadConversionKind::SlideToCC;
      entry.action.adsrSettings.target = AdsrTarget::CC;
      entry.action.adsrSettings.ccNumber = (int)mapping.getProperty("data1", MappingDefaults::ExpressionData1);
      const int slideAxisVal =
          (int)mapping.getProperty("slideAxis", MappingDefaults::SlideAxis);
      const bool isXYPad = (slideAxisVal == 2);

      p.inputMin = (float)mapping.getProperty("touchpadInputMin", static_cast<double>(MappingDefaults::TouchpadInputMin));
      p.inputMax = (float)mapping.getProperty("touchpadInputMax", static_cast<double>(MappingDefaults::TouchpadInputMax));
      float r = p.inputMax - p.inputMin;
      p.invInputRange = (r > 0.0f) ? (1.0f / r) : 0.0f;
      p.outputMin = (int)mapping.getProperty("touchpadOutputMin", MappingDefaults::TouchpadOutputMin);
      p.outputMax = (int)mapping.getProperty("touchpadOutputMax", MappingDefaults::TouchpadOutputMax);
      int quickPrecision = (int)mapping.getProperty("slideQuickPrecision", MappingDefaults::SlideQuickPrecision);
      int absRel = (int)mapping.getProperty("slideAbsRel", MappingDefaults::SlideAbsRel);
      int lockFree = (int)mapping.getProperty("slideLockFree", MappingDefaults::SlideLockFree);
      // Quick (0) = one finger drives; Precision (1) = need two fingers. Same bit layout as mixer.
      p.slideModeFlags = (quickPrecision == 0 ? kMixerModeUseFinger1 : 0) |
                         (lockFree == 0 ? kMixerModeLock : 0) |
                         (absRel ? kMixerModeRelative : 0);
      p.slideAxis = (uint8_t)juce::jlimit(0, 2, slideAxisVal);
      p.slideReturnOnRelease =
          (bool)mapping.getProperty("slideReturnOnRelease",
                                    MappingDefaults::SlideReturnOnRelease);
      p.slideRestValue = juce::jlimit(0, 127,
          (int)mapping.getProperty("slideRestValue",
                                   MappingDefaults::SlideRestValue));
      p.slideReturnGlideMs = juce::jlimit(0, 5000,
          (int)mapping.getProperty("slideReturnGlideMs",
                                   MappingDefaults::SlideReturnGlideMs));

      if (isXYPad) {
        // XY pad: duplicate this mapping into two entries, one per axis.
        const int ccX = juce::jlimit(
            0, 127,
            (int)mapping.getProperty("slideCcNumberX",
                                     entry.action.adsrSettings.ccNumber));
        const int ccY = juce::jlimit(
            0, 127,
            (int)mapping.getProperty("slideCcNumberY",
                                     entry.action.adsrSettings.ccNumber));
        const bool separateRanges =
            (bool)mapping.getProperty("slideSeparateAxisRanges", false);

        auto populateRangesForAxis =
            [&](bool isX, float &inMin, float &inMax, int &outMin,
                int &outMax) {
              if (!separateRanges) {
                inMin = p.inputMin;
                inMax = p.inputMax;
                outMin = p.outputMin;
                outMax = p.outputMax;
                return;
              }
              if (isX) {
                inMin = (float)mapping.getProperty(
                    "touchpadInputMinX",
                    static_cast<double>(MappingDefaults::TouchpadInputMin));
                inMax = (float)mapping.getProperty(
                    "touchpadInputMaxX",
                    static_cast<double>(MappingDefaults::TouchpadInputMax));
                outMin = (int)mapping.getProperty(
                    "touchpadOutputMinX", MappingDefaults::TouchpadOutputMin);
                outMax = (int)mapping.getProperty(
                    "touchpadOutputMaxX", MappingDefaults::TouchpadOutputMax);
              } else {
                inMin = (float)mapping.getProperty(
                    "touchpadInputMinY",
                    static_cast<double>(MappingDefaults::TouchpadInputMin));
                inMax = (float)mapping.getProperty(
                    "touchpadInputMaxY",
                    static_cast<double>(MappingDefaults::TouchpadInputMax));
                outMin = (int)mapping.getProperty(
                    "touchpadOutputMinY", MappingDefaults::TouchpadOutputMin);
                outMax = (int)mapping.getProperty(
                    "touchpadOutputMaxY", MappingDefaults::TouchpadOutputMax);
              }
            };

        float inMinX = 0.0f, inMaxX = 1.0f;
        int outMinX = 0, outMaxX = 127;
        float inMinY = 0.0f, inMaxY = 1.0f;
        int outMinY = 0, outMaxY = 127;
        populateRangesForAxis(true, inMinX, inMaxX, outMinX, outMaxX);
        populateRangesForAxis(false, inMinY, inMaxY, outMinY, outMaxY);

        // Y axis entry (vertical)
        TouchpadMappingEntry entryY = entry;
        TouchpadConversionParams &pY = entryY.conversionParams;
        pY.slideAxis = 0;
        pY.inputMin = inMinY;
        pY.inputMax = inMaxY;
        float rY = pY.inputMax - pY.inputMin;
        pY.invInputRange = (rY > 0.0f) ? (1.0f / rY) : 0.0f;
        pY.outputMin = outMinY;
        pY.outputMax = outMaxY;
        entryY.action.adsrSettings.ccNumber = ccY;

        // X axis entry (horizontal)
        TouchpadMappingEntry entryX = entry;
        TouchpadConversionParams &pX = entryX.conversionParams;
        pX.slideAxis = 1;
        pX.inputMin = inMinX;
        pX.inputMax = inMaxX;
        float rX = pX.inputMax - pX.inputMin;
        pX.invInputRange = (rX > 0.0f) ? (1.0f / rX) : 0.0f;
        pX.outputMin = outMinX;
        pX.outputMax = outMaxX;
        entryX.action.adsrSettings.ccNumber = ccX;

        out.push_back(std::move(entryY));
        out.push_back(std::move(entryX));
        return;
      }
    } else if (isCC && expressionCCModeStr.equalsIgnoreCase("Encoder")) {
      // Encoder: rotation (swipe) + optional push. Requires continuous X/Y; auto-promote boolean events.
      if (eventId == TouchpadEvent::Finger1Down || eventId == TouchpadEvent::Finger1Up ||
          eventId == TouchpadEvent::Finger2Down || eventId == TouchpadEvent::Finger2Up) {
        entry.eventId = TouchpadEvent::Finger1Y;
      }
      entry.conversionKind = TouchpadConversionKind::EncoderCC;
      entry.action.adsrSettings.target = AdsrTarget::CC;
      entry.action.adsrSettings.ccNumber = (int)mapping.getProperty("data1", MappingDefaults::ExpressionData1);
      p.outputMin = (int)mapping.getProperty("touchpadOutputMin", MappingDefaults::TouchpadOutputMin);
      p.outputMax = (int)mapping.getProperty("touchpadOutputMax", MappingDefaults::TouchpadOutputMax);
      p.encoderAxis = (uint8_t)juce::jlimit(0, 2, (int)mapping.getProperty("encoderAxis", MappingDefaults::EncoderAxis));
      p.encoderSensitivity = (float)mapping.getProperty("encoderSensitivity", static_cast<double>(MappingDefaults::EncoderSensitivity));
      p.encoderSensitivity = juce::jlimit(0.1f, 10.0f, p.encoderSensitivity);
      p.encoderStepSize = juce::jlimit(1, 16, (int)mapping.getProperty("encoderStepSize", MappingDefaults::EncoderStepSize));
      p.encoderStepSizeX = juce::jlimit(1, 16, (int)mapping.getProperty("encoderStepSizeX", MappingDefaults::EncoderStepSizeX));
      p.encoderStepSizeY = juce::jlimit(1, 16, (int)mapping.getProperty("encoderStepSizeY", MappingDefaults::EncoderStepSizeY));
      juce::String outModeStr = mapping.getProperty("encoderOutputMode", MappingDefaults::EncoderOutputModeAbsoluteStr).toString().trim();
      if (outModeStr.equalsIgnoreCase("Relative"))
        p.encoderOutputMode = 1;
      else if (outModeStr.equalsIgnoreCase("NRPN"))
        p.encoderOutputMode = 2;
      else
        p.encoderOutputMode = 0;
      p.encoderRelativeEncoding = (uint8_t)juce::jlimit(0, 3, (int)mapping.getProperty("encoderRelativeEncoding", MappingDefaults::EncoderRelativeEncoding));
      p.encoderWrap = (bool)mapping.getProperty("encoderWrap", MappingDefaults::EncoderWrap);
      p.encoderInitialValue = juce::jlimit(0, 127, (int)mapping.getProperty("encoderInitialValue", MappingDefaults::EncoderInitialValue));
      p.encoderNRPNNumber = juce::jlimit(0, 16383, (int)mapping.getProperty("encoderNRPNNumber", MappingDefaults::EncoderNRPNNumber));
      p.encoderPushDetection = (uint8_t)juce::jlimit(0, 2, (int)mapping.getProperty("encoderPushDetection", MappingDefaults::EncoderPushDetection));
      juce::String pushTypeStr = mapping.getProperty("encoderPushOutputType", "CC").toString().trim();
      if (pushTypeStr.equalsIgnoreCase("Note"))
        p.encoderPushOutputType = 1;
      else if (pushTypeStr.equalsIgnoreCase("ProgramChange"))
        p.encoderPushOutputType = 2;
      else
        p.encoderPushOutputType = 0;
      p.encoderPushMode = (uint8_t)juce::jlimit(0, 3, (int)mapping.getProperty("encoderPushMode", MappingDefaults::EncoderPushMode));
      p.encoderPushCCNumber = juce::jlimit(0, 127, (int)mapping.getProperty("encoderPushCCNumber", entry.action.adsrSettings.ccNumber));
      p.encoderPushValue = juce::jlimit(0, 127, (int)mapping.getProperty("encoderPushValue", MappingDefaults::EncoderPushValue));
      p.encoderPushNote = juce::jlimit(0, 127, (int)mapping.getProperty("encoderPushNote", MappingDefaults::EncoderPushNote));
      p.encoderPushProgram = juce::jlimit(0, 127, (int)mapping.getProperty("encoderPushProgram", MappingDefaults::EncoderPushProgram));
      p.encoderPushChannel = juce::jlimit(1, 16, (int)mapping.getProperty("encoderPushChannel", headerChannel));
      p.encoderDeadZone = (float)mapping.getProperty("encoderDeadZone", static_cast<double>(MappingDefaults::EncoderDeadZone));
      p.encoderDeadZone = juce::jlimit(0.0f, 0.5f, p.encoderDeadZone);
    } else if (inputBool && isCC) {
      // BoolToCC only for CC target; PitchBend/SmartScaleBend use ContinuousToRange
      // (with boolean events auto-promoted to Finger1X) so visualizer and first-touch work.
      entry.conversionKind = TouchpadConversionKind::BoolToCC;
      p.valueWhenOn =
          (int)mapping.getProperty("touchpadValueWhenOn", MappingDefaults::TouchpadValueWhenOn);
      p.valueWhenOff =
          (int)mapping.getProperty("touchpadValueWhenOff", MappingDefaults::TouchpadValueWhenOff);

      // CC Position mode release behaviour (instant vs latch). Defaults to instant
      // for existing presets that do not have ccReleaseBehavior set.
      juce::String ccReleaseStr =
          mapping
              .getProperty("ccReleaseBehavior",
                           MappingDefaults::CcReleaseBehaviorInstant)
              .toString()
              .trim();
      if (ccReleaseStr.equalsIgnoreCase("Always Latch"))
        p.ccReleaseBehavior = CcReleaseBehavior::AlwaysLatch;
      else
        p.ccReleaseBehavior = CcReleaseBehavior::SendReleaseInstant;
    } else {
      entry.conversionKind = TouchpadConversionKind::ContinuousToRange;
      // PitchBend/SmartScaleBend need continuous X/Y; auto-promote boolean
      // events so the first touch sends position updates and the visualizer works.
      if (isPB || isSmartBend) {
        if (eventId == TouchpadEvent::Finger1Down || eventId == TouchpadEvent::Finger1Up ||
            eventId == TouchpadEvent::Finger2Down || eventId == TouchpadEvent::Finger2Up) {
          entry.eventId = TouchpadEvent::Finger1X;
        }
      }
      p.inputMin = (float)mapping.getProperty("touchpadInputMin", static_cast<double>(MappingDefaults::TouchpadInputMin));
      p.inputMax = (float)mapping.getProperty("touchpadInputMax", static_cast<double>(MappingDefaults::TouchpadInputMax));
      float r = p.inputMax - p.inputMin;
      p.invInputRange = (r > 0.0f) ? (1.0f / r) : 0.0f;
      if (isPB || isSmartBend) {
        // For pitch-based Expression targets, interpret the existing
        // touchpadOutputMin/Max as discrete step bounds and also store them
        // in a PitchPadConfig for shared runtime/visualizer use.
        const bool useCustomRange = (bool)mapping.getProperty("pitchPadUseCustomRange", false);
        if (isPB && !useCustomRange) {
          const int pbRange = settingsMgr.getPitchBendRange();
          int semitones = (int)mapping.getProperty("data2", 0);
          semitones = juce::jlimit(-juce::jmax(1, pbRange),
                                  juce::jmax(1, pbRange), semitones);
          const int half = juce::jmax(0, std::abs(semitones));
          p.outputMin = -half;
          p.outputMax = half;
        } else if (isSmartBend && !useCustomRange) {
          int stepShift = (int)mapping.getProperty("smartStepShift", MappingDefaults::SmartStepShift);
          stepShift = juce::jlimit(0, 12, std::abs(stepShift));
          p.outputMin = -stepShift;
          p.outputMax = stepShift;
        } else {
          p.outputMin = (int)mapping.getProperty("touchpadOutputMin", -1);
          p.outputMax = (int)mapping.getProperty("touchpadOutputMax", MappingDefaults::TouchpadOutputMaxPitchBend);
        }

        PitchPadConfig cfg;

        juce::String modeStr =
            mapping.getProperty("pitchPadMode", "Absolute").toString();
        cfg.mode = modeStr.equalsIgnoreCase("Relative")
                       ? PitchPadMode::Relative
                       : PitchPadMode::Absolute;

        juce::String startStr =
            mapping.getProperty("pitchPadStart", "Center").toString();
        if (startStr.equalsIgnoreCase("Left"))
          cfg.start = PitchPadStart::Left;
        else if (startStr.equalsIgnoreCase("Right"))
          cfg.start = PitchPadStart::Right;
        else if (startStr.equalsIgnoreCase("Custom"))
          cfg.start = PitchPadStart::Custom;
        else
          cfg.start = PitchPadStart::Center;

        cfg.customStartX =
            (float)mapping.getProperty("pitchPadCustomStart", static_cast<double>(MappingDefaults::PitchPadCustomStart));
        cfg.minStep = p.outputMin;
        cfg.maxStep = p.outputMax;
        cfg.restZonePercent =
            (float)mapping.getProperty("pitchPadRestZonePercent", static_cast<double>(MappingDefaults::PitchPadRestZonePercent));
        cfg.transitionZonePercent =
            (float)mapping.getProperty("pitchPadTransitionZonePercent", static_cast<double>(MappingDefaults::PitchPadTransitionZonePercent));
        cfg.restingSpacePercent =
            (float)mapping.getProperty("pitchPadRestingPercent", static_cast<double>(MappingDefaults::PitchPadRestingPercent));
        switch (cfg.start) {
        case PitchPadStart::Left:
          cfg.zeroStep = static_cast<float>(cfg.minStep);
          break;
        case PitchPadStart::Right:
          cfg.zeroStep = static_cast<float>(cfg.maxStep);
          break;
        case PitchPadStart::Center:
          cfg.zeroStep = 0.0f;
          break;
        case PitchPadStart::Custom:
          cfg.zeroStep = juce::jmap(cfg.customStartX, 0.0f, 1.0f,
                                    static_cast<float>(cfg.minStep),
                                    static_cast<float>(cfg.maxStep));
          break;
        }

        p.pitchPadConfig = cfg;
        p.cachedPitchPadLayout = buildPitchPadLayout(cfg);
      } else {
        p.outputMin = (int)mapping.getProperty("touchpadOutputMin", MappingDefaults::TouchpadOutputMin);
        p.outputMax = (int)mapping.getProperty("touchpadOutputMax", MappingDefaults::TouchpadOutputMax);
        p.pitchPadConfig.reset();
      }
    }
    // Apply Expression ADSR and release behavior.
    if (isPB)
      entry.action.adsrSettings.target = AdsrTarget::PitchBend;
    else if (isSmartBend)
      entry.action.adsrSettings.target = AdsrTarget::SmartScaleBend;
    else
      entry.action.adsrSettings.target = AdsrTarget::CC;

    // Custom ADSR envelope is not supported for PitchBend/SmartScaleBend
    entry.action.adsrSettings.useCustomEnvelope =
        (bool)mapping.getProperty("useCustomEnvelope", false) && !isPB &&
        !isSmartBend;
    if (!entry.action.adsrSettings.useCustomEnvelope) {
      entry.action.adsrSettings.attackMs = 0;
      entry.action.adsrSettings.decayMs = 0;
      entry.action.adsrSettings.sustainLevel = 1.0f;
      entry.action.adsrSettings.releaseMs = 0;
    } else {
      entry.action.adsrSettings.attackMs =
          (int)mapping.getProperty("adsrAttack", MappingDefaults::ADSRAttackMs);
      entry.action.adsrSettings.decayMs =
          (int)mapping.getProperty("adsrDecay", MappingDefaults::ADSRDecayMs);
      entry.action.adsrSettings.sustainLevel =
          (float)mapping.getProperty("adsrSustain", MappingDefaults::ADSRSustain);
      entry.action.adsrSettings.releaseMs =
          (int)mapping.getProperty("adsrRelease", MappingDefaults::ADSRReleaseMs);
    }

    bool defaultResetPitch =
        (entry.action.adsrSettings.target == AdsrTarget::PitchBend ||
         entry.action.adsrSettings.target == AdsrTarget::SmartScaleBend);
    entry.action.sendReleaseValue =
        (bool)mapping.getProperty("sendReleaseValue", defaultResetPitch);
    entry.action.releaseValue =
        (int)mapping.getProperty("touchpadValueWhenOff", MappingDefaults::TouchpadValueWhenOff);
    // CC: always send value when off on release (no UI toggle)
    if (entry.action.adsrSettings.target == AdsrTarget::CC)
      entry.action.sendReleaseValue = true;

    if (isPB || isSmartBend)
      entry.touchGlideMs = juce::jlimit(0, 200,
          (int)mapping.getProperty("pitchPadTouchGlideMs", MappingDefaults::PitchPadTouchGlideMs));

    if (isSmartBend) {
      entry.smartScaleFollowGlobal =
          (bool)mapping.getProperty("smartScaleFollowGlobal", MappingDefaults::SmartScaleFollowGlobal);
      entry.smartScaleName =
          mapping.getProperty("smartScaleName", MappingDefaults::SmartScaleName).toString().trim();
      if (entry.smartScaleName.isEmpty())
        entry.smartScaleName = "Major";
    }

    if (entry.action.adsrSettings.target == AdsrTarget::CC) {
      entry.action.adsrSettings.ccNumber =
          (int)mapping.getProperty("data1", MappingDefaults::ExpressionData1);
      entry.action.adsrSettings.valueWhenOn =
          (int)mapping.getProperty("touchpadValueWhenOn", MappingDefaults::TouchpadValueWhenOn);
      entry.action.adsrSettings.valueWhenOff =
          (int)mapping.getProperty("touchpadValueWhenOff", MappingDefaults::TouchpadValueWhenOff);
      entry.action.data2 = entry.action.adsrSettings.valueWhenOn;
    } else if (entry.action.adsrSettings.target ==
               AdsrTarget::SmartScaleBend) {
      buildSmartBendLookup(entry.action, mapping, zoneMgr, settingsMgr);
      entry.action.data2 = 8192;
    } else {
      const int pbRange = settingsMgr.getPitchBendRange();
      int semitones = (int)mapping.getProperty("data2", 0);
      entry.action.data2 = juce::jlimit(-juce::jmax(1, pbRange),
                                        juce::jmax(1, pbRange), semitones);
    }
  } else {
    entry.conversionKind = TouchpadConversionKind::BoolToGate;
  }

  out.push_back(std::move(entry));
}

// Phase 51.4 / 53.5: Apply manual mappings for a single layer. targetState for
// device Pass 2 (Inherited vs Active). If touchpadMappingsOut is non-null,
// Touchpad mappings are appended there instead of writing to the grid.
// keysWrittenOut: if non-null, mark keys written by this layer (for "private
// to layer" inheritance stripping).
void compileMappingsForLayer(
    VisualGrid &vGrid, AudioGrid &aGrid, PresetManager &presetMgr,
    DeviceManager &deviceMgr, ZoneManager &zoneMgr,
    SettingsManager &settingsMgr, uintptr_t aliasHash, int layerId,
    std::vector<bool> &touchedKeys, VisualState targetState,
    std::vector<TouchpadMappingEntry> *touchpadMappingsOut,
    std::vector<bool> *keysWrittenOut = nullptr) {
  std::vector<juce::ValueTree> enabledList =
      presetMgr.getEnabledMappingsForLayer(layerId);
  if (enabledList.empty())
    return;

  // Phase 51.6: Process specific modifier keys (LShift, RShift, etc.) before
  // generic (Shift, Control, Alt) so specific mappings override expansion.
  std::vector<int> order(enabledList.size());
  std::iota(order.begin(), order.end(), 0);
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    int keyA = (int)enabledList[a].getProperty("inputKey", MappingDefaults::InputKey);
    int keyB = (int)enabledList[b].getProperty("inputKey", MappingDefaults::InputKey);
    bool specificA = isSpecificModifierKey(keyA);
    bool specificB = isSpecificModifierKey(keyB);
    if (specificA != specificB)
      return specificA; // specific first
    return a < b;
  });

  for (int i : order) {
    auto mapping = enabledList[i];
    if (!mapping.isValid() || !mapping.hasType("Mapping"))
      continue;

    const bool forceAllLayers =
        (bool)mapping.getProperty("forceAllLayers", false);
    if (forceAllLayers && layerId == 0)
      continue;

    juce::String aliasName =
        mapping.getProperty("inputAlias", "").toString().trim();
    const bool isTouchpadMapping =
        aliasName.trim().equalsIgnoreCase("Touchpad");

    // Touchpad mappings are only taken from the Touchpad tab (TouchpadMixerManager),
    // not from the preset Mappings list, to avoid duplicate or conflicting entries
    // (e.g. Finger1Up from preset + Finger1Down from Touchpad tab causing extra NOTE_ON on release).
    if (isTouchpadMapping)
      continue;

    const int inputKey = (int)mapping.getProperty("inputKey", MappingDefaults::InputKey);
    if (inputKey < 0 || inputKey > 0xFF)
      continue;

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
    applyNoteOptionsFromMapping(mapping, zoneMgr, action);

    // Phase 56.1: Expression (unified CC + Envelope)
    if (action.type == ActionType::Expression) {
      juce::String adsrTargetStr =
          mapping.getProperty("adsrTarget", MappingDefaults::AdsrTargetCC).toString().trim();
      bool useCustomEnvelope =
          (bool)mapping.getProperty("useCustomEnvelope", false);

      if (adsrTargetStr.equalsIgnoreCase("PitchBend"))
        action.adsrSettings.target = AdsrTarget::PitchBend;
      else if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend"))
        action.adsrSettings.target = AdsrTarget::SmartScaleBend;
      else
        action.adsrSettings.target = AdsrTarget::CC;

      // Custom ADSR envelope is not supported for PitchBend/SmartScaleBend
      bool isPB = action.adsrSettings.target == AdsrTarget::PitchBend;
      bool isSmartBend =
          action.adsrSettings.target == AdsrTarget::SmartScaleBend;
      action.adsrSettings.useCustomEnvelope =
          useCustomEnvelope && !isPB && !isSmartBend;

      if (!useCustomEnvelope) {
        action.adsrSettings.attackMs = 0;
        action.adsrSettings.decayMs = 0;
        action.adsrSettings.sustainLevel = 1.0f;
        action.adsrSettings.releaseMs = 0;
      } else {
        action.adsrSettings.attackMs =
            (int)mapping.getProperty("adsrAttack", MappingDefaults::ADSRAttackMs);
        action.adsrSettings.decayMs = (int)mapping.getProperty("adsrDecay", MappingDefaults::ADSRDecayMs);
        action.adsrSettings.sustainLevel =
            (float)mapping.getProperty("adsrSustain", MappingDefaults::ADSRSustain);
        action.adsrSettings.releaseMs =
            (int)mapping.getProperty("adsrRelease", MappingDefaults::ADSRReleaseMs);
      }

      if (action.adsrSettings.target == AdsrTarget::CC) {
        action.adsrSettings.ccNumber = (int)mapping.getProperty("data1", MappingDefaults::ExpressionData1);
        action.adsrSettings.valueWhenOn =
            (int)mapping.getProperty("touchpadValueWhenOn", MappingDefaults::TouchpadValueWhenOn);
        action.adsrSettings.valueWhenOff =
            (int)mapping.getProperty("touchpadValueWhenOff", MappingDefaults::TouchpadValueWhenOff);
        action.data2 = action.adsrSettings.valueWhenOn;
      } else if (action.adsrSettings.target == AdsrTarget::PitchBend) {
        // Bend (semitones); envelope uses this and returns to neutral (8192)
        const int pbRange = settingsMgr.getPitchBendRange();
        int semitones = (int)mapping.getProperty("data2", 0);
        action.data2 = juce::jlimit(-juce::jmax(1, pbRange),
                                    juce::jmax(1, pbRange), semitones);
      } else if (action.adsrSettings.target == AdsrTarget::SmartScaleBend) {
        buildSmartBendLookup(action, mapping, zoneMgr, settingsMgr);
        action.data2 = 8192; // peak from lookup per note
      }
      bool defaultResetPitch =
          (action.adsrSettings.target == AdsrTarget::PitchBend ||
           action.adsrSettings.target == AdsrTarget::SmartScaleBend);
      action.sendReleaseValue =
          (bool)mapping.getProperty("sendReleaseValue", defaultResetPitch);
      action.releaseValue = (int)mapping.getProperty("releaseValue", MappingDefaults::ReleaseValue);
    }

    // Latch Toggle: release latched notes when toggling off
    if (action.type == ActionType::Command) {
      if (action.data1 == static_cast<int>(MIDIQy::CommandID::LatchToggle)) {
        action.releaseLatchedOnLatchToggleOff =
            (bool)mapping.getProperty("releaseLatchedOnToggleOff", true);
      }

    // Transpose command: mode, modify, semitones (for set)
    if (action.type == ActionType::Command &&
        (action.data1 == static_cast<int>(MIDIQy::CommandID::Transpose) ||
         action.data1 ==
             static_cast<int>(MIDIQy::CommandID::GlobalPitchDown))) {
      juce::String modeStr =
          mapping.getProperty("transposeMode", "Global").toString();
      action.transposeLocal = modeStr.equalsIgnoreCase("Local");
      int modify = (int)mapping.getProperty("transposeModify", MappingDefaults::TransposeModify);
      if (action.data1 ==
          static_cast<int>(MIDIQy::CommandID::GlobalPitchDown)) {
        modify = 1; // Legacy: down 1 semitone
      }
      action.transposeModify = juce::jlimit(0, 4, modify);
      action.transposeSemitones =
          (int)mapping.getProperty("transposeSemitones", MappingDefaults::TransposeSemitones);
      action.transposeSemitones =
          juce::jlimit(-48, 48, action.transposeSemitones);
    }

    // Global root / scale commands
      const int cmd = action.data1;
      if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootUp) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootDown) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalRootSet)) {
        int rm = (int)mapping.getProperty("rootModify", MappingDefaults::RootModify);
        action.rootModify = juce::jlimit(0, 2, rm);
        action.rootNote =
            juce::jlimit(0, 127, (int)mapping.getProperty("rootNote", MappingDefaults::RootNote));
      }
      if (cmd == static_cast<int>(MIDIQy::CommandID::GlobalScaleNext) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalScalePrev) ||
          cmd == static_cast<int>(MIDIQy::CommandID::GlobalScaleSet)) {
        int sm = (int)mapping.getProperty("scaleModify", MappingDefaults::ScaleModify);
        action.scaleModify = juce::jlimit(0, 2, sm);
        action.scaleIndex =
            juce::jmax(0, (int)mapping.getProperty("scaleIndex", MappingDefaults::ScaleIndex));
      }

      if (cmd == static_cast<int>(
                    MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary) ||
          cmd == static_cast<int>(
                    MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle) ||
          cmd == static_cast<int>(
                    MIDIQy::CommandID::TouchpadLayoutGroupSoloSet) ||
          cmd == static_cast<int>(
                    MIDIQy::CommandID::TouchpadLayoutGroupSoloClear)) {
        action.touchpadLayoutGroupId =
            (int)mapping.getProperty("touchpadLayoutGroupId", MappingDefaults::TouchpadLayoutGroupId);
        action.touchpadSoloScope =
            juce::jlimit(0, 2, (int)mapping.getProperty("touchpadSoloScope", MappingDefaults::TouchpadSoloScope));
      }
    }

    juce::Colour color = getColorForType(action.type, settingsMgr);
    juce::String label = makeLabelForAction(action);
    juce::String sourceName =
        aliasName.isNotEmpty() ? ("Mapping: " + aliasName) : "Mapping";

    applyVisualWithModifiers(vGrid, inputKey, color, label, sourceName,
                             &touchedKeys, targetState);

    if (vGrid[(size_t)inputKey].state != VisualState::Conflict) {
      writeAudioSlot(aGrid, inputKey, action);
      markKeyWritten(inputKey, keysWrittenOut);
    }
  }
}

} // namespace

std::shared_ptr<CompiledMapContext> GridCompiler::compile(
    PresetManager &presetMgr, DeviceManager &deviceMgr, ZoneManager &zoneMgr,
    TouchpadMixerManager &touchpadMixerMgr, SettingsManager &settingsMgr) {
  // 1. Setup Context
  auto context = std::make_shared<CompiledMapContext>();

  // Collect Base-layer mappings that should apply on all layers.
  std::unordered_map<uintptr_t, std::vector<ForcedMapping>> forcedByAlias;
  collectForcedMappings(presetMgr, deviceMgr, zoneMgr, settingsMgr,
                        forcedByAlias);

  // 2. Collect touchpad mappings (one pass over all layers)
  const uintptr_t touchpadAliasHash = static_cast<uintptr_t>(
      std::hash<juce::String>{}(juce::String("Touchpad").trim()));
  for (int layerId = 0; layerId < 9; ++layerId) {
    VisualGrid dummyV;
    AudioGrid dummyA;
    std::vector<bool> touchedKeys(256, false);
    compileMappingsForLayer(dummyV, dummyA, presetMgr, deviceMgr, zoneMgr,
                            settingsMgr, touchpadAliasHash, layerId,
                            touchedKeys, VisualState::Active,
                            &context->touchpadMappings);
  }

  // 2c. Collect touchpad mappings defined in the Touchpad tab (TouchpadMixerManager).
  {
    auto touchpadMappings = touchpadMixerMgr.getTouchpadMappings();
    for (const auto &cfg : touchpadMappings) {
      if (!cfg.mapping.isValid())
        continue;
      // Use cfg.layerId as the authoritative layer for this mapping.
      int layerId = juce::jlimit(0, 8, cfg.layerId);
      compileTouchpadMappingFromValueTree(cfg.mapping, layerId, cfg.midiChannel,
                                          cfg.layoutGroupId, cfg.zIndex,
                                          cfg.regionLock,
                                          zoneMgr, settingsMgr,
                                          context->touchpadMappings, &cfg.region);
    }
  }

  // 2b. Collect touchpad mixer and drum pad strips. Sort by z-index descending
  // (higher = on top when regions overlap on same layer).
  auto layouts = touchpadMixerMgr.getLayouts();
  std::sort(layouts.begin(), layouts.end(),
            [](const TouchpadMixerConfig &a, const TouchpadMixerConfig &b) {
              return a.zIndex > b.zIndex;
            });
  for (const auto &cfg : layouts) {
    if (cfg.type == TouchpadType::Mixer) {
      TouchpadMixerEntry entry;
      entry.layerId = juce::jlimit(0, 8, cfg.layerId);
      entry.layoutGroupId = juce::jmax(0, cfg.layoutGroupId);
      entry.numFaders = juce::jlimit(1, 32, cfg.numFaders);
      entry.ccStart = juce::jlimit(0, 127, cfg.ccStart);
      entry.midiChannel = juce::jlimit(1, 16, cfg.midiChannel);
      entry.inputMin = cfg.inputMin;
      entry.inputMax = cfg.inputMax;
      float r = entry.inputMax - entry.inputMin;
      entry.invInputRange = (r > 0.0f) ? (1.0f / r) : 0.0f;
      entry.outputMin = juce::jlimit(0, 127, cfg.outputMin);
      entry.outputMax = juce::jlimit(0, 127, cfg.outputMax);
      entry.quickPrecision = cfg.quickPrecision;
      entry.absRel = cfg.absRel;
      entry.lockFree = cfg.lockFree;
      entry.muteButtonsEnabled = cfg.muteButtonsEnabled;
      entry.modeFlags =
          (cfg.quickPrecision == TouchpadMixerQuickPrecision::Quick
               ? kMixerModeUseFinger1
               : 0) |
          (cfg.lockFree == TouchpadMixerLockFree::Lock ? kMixerModeLock : 0) |
          (cfg.absRel == TouchpadMixerAbsRel::Relative ? kMixerModeRelative : 0) |
          (cfg.muteButtonsEnabled ? kMixerModeMuteButtons : 0);
      entry.effectiveYScale =
          cfg.muteButtonsEnabled ? (1.0f / kMuteButtonRegionTop) : 1.0f;
      float rL = juce::jlimit(0.0f, 0.99f, cfg.region.left);
      float rR = juce::jlimit(rL + 0.01f, 1.0f, cfg.region.right);
      float rT = juce::jlimit(0.0f, 0.99f, cfg.region.top);
      float rB = juce::jlimit(rT + 0.01f, 1.0f, cfg.region.bottom);
      entry.regionLeft = rL;
      entry.regionTop = rT;
      entry.regionRight = rR;
      entry.regionBottom = rB;
      float rW = rR - rL;
      float rH = rB - rT;
      entry.invRegionWidth = (rW > 1e-6f) ? (1.0f / rW) : 1.0f;
      entry.invRegionHeight = (rH > 1e-6f) ? (1.0f / rH) : 1.0f;
      entry.regionLock = cfg.regionLock;
      entry.modeFlags |= (cfg.regionLock ? kMixerModeRegionLock : 0);
      context->touchpadMixerStrips.push_back(entry);
      context->touchpadLayoutOrder.push_back(
          {TouchpadType::Mixer, context->touchpadMixerStrips.size() - 1});
    } else if (cfg.type == TouchpadType::DrumPad) {
      TouchpadDrumPadEntry dpEntry;
      dpEntry.layerId = juce::jlimit(0, 8, cfg.layerId);
      dpEntry.layoutGroupId = juce::jmax(0, cfg.layoutGroupId);
      dpEntry.rows = juce::jlimit(1, 8, cfg.drumPadRows);
      dpEntry.columns = juce::jlimit(1, 16, cfg.drumPadColumns);
      dpEntry.numPads = dpEntry.rows * dpEntry.columns;
      dpEntry.midiNoteStart = juce::jlimit(0, 127, cfg.drumPadMidiNoteStart);
      dpEntry.midiChannel = juce::jlimit(1, 16, cfg.midiChannel);
      dpEntry.baseVelocity = juce::jlimit(1, 127, cfg.drumPadBaseVelocity);
      dpEntry.velocityRandom =
          juce::jlimit(0, 127, cfg.drumPadVelocityRandom);
      float rL, rT, rR, rB;
      bool hasExplicitRegion =
          (cfg.region.left != 0.0f || cfg.region.top != 0.0f ||
           cfg.region.right != 1.0f || cfg.region.bottom != 1.0f);
      if (hasExplicitRegion) {
        rL = cfg.region.left;
        rT = cfg.region.top;
        rR = cfg.region.right;
        rB = cfg.region.bottom;
      } else {
        rL = juce::jlimit(0.0f, 0.5f, cfg.drumPadDeadZoneLeft);
        rT = juce::jlimit(0.0f, 0.5f, cfg.drumPadDeadZoneTop);
        rR = 1.0f - juce::jlimit(0.0f, 0.5f, cfg.drumPadDeadZoneRight);
        rB = 1.0f - juce::jlimit(0.0f, 0.5f, cfg.drumPadDeadZoneBottom);
      }
      rL = juce::jlimit(0.0f, 0.99f, rL);
      rR = juce::jlimit(rL + 0.01f, 1.0f, rR);
      rT = juce::jlimit(0.0f, 0.99f, rT);
      rB = juce::jlimit(rT + 0.01f, 1.0f, rB);
      dpEntry.regionLeft = rL;
      dpEntry.regionTop = rT;
      dpEntry.regionRight = rR;
      dpEntry.regionBottom = rB;
      float rW = rR - rL;
      float rH = rB - rT;
      dpEntry.invRegionWidth = (rW > 1e-6f) ? (1.0f / rW) : 1.0f;
      dpEntry.invRegionHeight = (rH > 1e-6f) ? (1.0f / rH) : 1.0f;
      dpEntry.regionLock = cfg.regionLock;
      dpEntry.layoutMode = cfg.drumPadLayoutMode;
      dpEntry.harmonicRowInterval = cfg.harmonicRowInterval;
      dpEntry.harmonicUseScaleFilter = cfg.harmonicUseScaleFilter;
      context->touchpadDrumPadStrips.push_back(dpEntry);
      context->touchpadLayoutOrder.push_back(
          {TouchpadType::DrumPad, context->touchpadDrumPadStrips.size() - 1});
    } else if (cfg.type == TouchpadType::ChordPad) {
      TouchpadChordPadEntry cp;
      cp.layerId = juce::jlimit(0, 8, cfg.layerId);
      cp.layoutGroupId = juce::jmax(0, cfg.layoutGroupId);
      cp.rows = juce::jlimit(1, 8, cfg.drumPadRows);
      cp.columns = juce::jlimit(1, 16, cfg.drumPadColumns);
      cp.midiChannel = juce::jlimit(1, 16, cfg.midiChannel);
      cp.baseVelocity = juce::jlimit(1, 127, cfg.drumPadBaseVelocity);
      cp.velocityRandom = juce::jlimit(0, 127, cfg.drumPadVelocityRandom);
      cp.baseRootNote = juce::jlimit(0, 127, cfg.drumPadMidiNoteStart);
      cp.presetId = juce::jmax(0, cfg.chordPadPreset);
      cp.latchMode = cfg.chordPadLatchMode;
      float rL = juce::jlimit(0.0f, 0.99f, cfg.region.left);
      float rR = juce::jlimit(rL + 0.01f, 1.0f, cfg.region.right);
      float rT = juce::jlimit(0.0f, 0.99f, cfg.region.top);
      float rB = juce::jlimit(rT + 0.01f, 1.0f, cfg.region.bottom);
      cp.regionLeft = rL;
      cp.regionTop = rT;
      cp.regionRight = rR;
      cp.regionBottom = rB;
      float rW = rR - rL;
      float rH = rB - rT;
      cp.invRegionWidth = (rW > 1e-6f) ? (1.0f / rW) : 1.0f;
      cp.invRegionHeight = (rH > 1e-6f) ? (1.0f / rH) : 1.0f;
      cp.regionLock = cfg.regionLock;
      context->touchpadChordPads.push_back(cp);
      context->touchpadLayoutOrder.push_back(
          {TouchpadType::ChordPad, context->touchpadChordPads.size() - 1});
    }
  }

  // 3. Define Helper Lambda "applyLayerToGrid"
  // Phase 53.5: targetState = Active for current layer, Inherited for lower
  // layer (device Pass 2). keysWrittenOut: optional, record keys written by
  // this layer for "private to layer" stripping.
  auto applyLayerToGrid = [&](VisualGrid &vGrid, AudioGrid &aGrid, int layerId,
                              uintptr_t aliasHash,
                              VisualState targetState = VisualState::Active,
                              std::vector<bool> *keysWrittenOut = nullptr) {
    std::vector<bool> touchedKeys(256, false);

    auto itForced = forcedByAlias.find(aliasHash);
    if (itForced != forcedByAlias.end()) {
      for (const auto &fm : itForced->second) {
        const int key = fm.inputKey;
        if (key < 0 || key >= (int)vGrid.size())
          continue;
        applyVisualWithModifiers(vGrid, key, fm.color, fm.label, fm.sourceName,
                                 &touchedKeys, targetState);
        if (vGrid[(size_t)key].state != VisualState::Conflict) {
          writeAudioSlot(aGrid, key, fm.action);
        }
      }
    }

    compileZonesForLayer(vGrid, aGrid, zoneMgr, deviceMgr, aliasHash, layerId,
                         touchedKeys, context->chordPool, targetState,
                         keysWrittenOut);

    compileMappingsForLayer(vGrid, aGrid, presetMgr, deviceMgr, zoneMgr,
                            settingsMgr, aliasHash, layerId, touchedKeys,
                            targetState, nullptr, keysWrittenOut);
  };

  // 3. PASS 1: Compile Global Stack (Vertical) – Hash 0 only
  // Layer inheritance: soloLayer, passthruInheritance, privateToLayer.
  const uintptr_t globalHash = 0;
  context->visualLookup[globalHash].resize(9);

  std::array<int, 9> effectiveBaseIndex{};
  std::array<std::vector<bool>, 9> keysWrittenByLayer;
  for (size_t i = 0; i < 9; ++i)
    keysWrittenByLayer[i].resize(256, false);

  for (int L = 0; L < 9; ++L) {
    auto layerNode = presetMgr.getLayerNode(L);
    const bool soloLayer =
        layerNode.isValid() && (bool)layerNode.getProperty("soloLayer", false);
    const bool passthruInheritance =
        layerNode.isValid() &&
        (bool)layerNode.getProperty("passthruInheritance", false);
    const bool privateToLayer =
        layerNode.isValid() &&
        (bool)layerNode.getProperty("privateToLayer", false);

    auto vGrid = std::make_shared<VisualGrid>();
    auto aGrid = std::make_shared<AudioGrid>();

    if (L == 0) {
      *vGrid = *makeVisualGrid();
      *aGrid = *makeAudioGrid();
      effectiveBaseIndex[0] = 0;
    } else if (soloLayer) {
      // Solo layer: start from empty (only this layer's content).
      *vGrid = *makeVisualGrid();
      *aGrid = *makeAudioGrid();
    } else {
      const int baseIdx = effectiveBaseIndex[(size_t)(L - 1)];
      *vGrid = *context->visualLookup[globalHash][(size_t)baseIdx];
      *aGrid = *context->globalGrids[(size_t)baseIdx];

      // If layer L-1 has "private to layer", clear slots it wrote so we don't
      // inherit them.
      layerNode = presetMgr.getLayerNode(L - 1);
      const bool prevPrivate =
          layerNode.isValid() &&
          (bool)layerNode.getProperty("privateToLayer", false);
      if (prevPrivate)
        clearSlotsForKeys(*aGrid, *vGrid, keysWrittenByLayer[(size_t)(L - 1)]);

      for (auto &slot : *vGrid) {
        if (slot.state != VisualState::Empty) {
          slot.state = VisualState::Inherited;
          slot.displayColor =
              slot.displayColor.withAlpha(static_cast<juce::uint8>(76));
        }
      }

      for (size_t keyCode = 0; keyCode < 256; ++keyCode) {
        const auto &aSlot = (*aGrid)[keyCode];
        if (aSlot.isActive && isLayerCommand(aSlot.action)) {
          (*vGrid)[keyCode].state = VisualState::Empty;
          (*vGrid)[keyCode].displayColor = juce::Colours::transparentBlack;
          (*vGrid)[keyCode].label.clear();
          (*vGrid)[keyCode].sourceName.clear();
          (*aGrid)[keyCode].isActive = false;
          (*aGrid)[keyCode].chordIndex = -1;
        }
      }
    }

    std::fill(keysWrittenByLayer[(size_t)L].begin(),
              keysWrittenByLayer[(size_t)L].end(), false);
    applyLayerToGrid(*vGrid, *aGrid, L, globalHash, VisualState::Active,
                     &keysWrittenByLayer[(size_t)L]);

    if (soloLayer || passthruInheritance)
      effectiveBaseIndex[(size_t)L] =
          L > 0 ? effectiveBaseIndex[(size_t)(L - 1)] : 0;
    else
      effectiveBaseIndex[(size_t)L] = L;

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
      // Phase 53.5: k < L = lower layer content -> Inherited (dim); k == L =
      // current layer -> Active.
      for (int k = 0; k <= L; ++k) {
        VisualState stateForPass =
            (k < L) ? VisualState::Inherited : VisualState::Active;
        applyLayerToGrid(*vGrid, *aGrid, k, devHash, stateForPass);
      }

      context->visualLookup[devHash][(size_t)L] = vGrid;
      context->deviceGrids[devHash][(size_t)L] = aGrid;
    }

    // Phase 53.9: InputProcessor looks up by hardware ID; store grids under
    // each hardware ID for this alias so processEvent finds device-specific
    // mappings.
    auto hardwareIds = deviceMgr.getHardwareForAlias(aliasName);
    for (auto hwId : hardwareIds) {
      if (hwId != 0)
        context->deviceGrids[hwId] = context->deviceGrids[devHash];
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

    std::vector<int> zoneIntervals =
        zoneMgr.getScaleIntervalsForZone(zone.get());
    const auto &keyCodes = zone->getInputKeyCodes();
    for (int keyCode : keyCodes) {
      if (keyCode < 0 || keyCode > 0xFF)
        continue;

      auto chordOpt =
          zone->getNotesForKey(keyCode, globalChrom, globalDeg, &zoneIntervals);
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
