#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "ScaleLibrary.h"
#include "SettingsManager.h"

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr,
                               DeviceManager &deviceMgr, ScaleLibrary &scaleLib,
                               MidiEngine &midiEng,
                               SettingsManager &settingsMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr),
      deviceManager(deviceMgr), zoneManager(scaleLib), scaleLibrary(scaleLib),
      expressionEngine(midiEng), settingsManager(settingsMgr) {
  // Phase 42: No addListener here – done in initialize() after graph is built
  // Phase 41: Static 9 layers (0=Base, 1-8=Overlays)
  layers.resize(9);
}

void InputProcessor::initialize() {
  presetManager.getRootNode().addListener(this);
  presetManager.getLayersList().addListener(this);
  presetManager.addChangeListener(this);
  deviceManager.addChangeListener(this);
  settingsManager.addChangeListener(this);
  zoneManager.addChangeListener(this);
  rebuildMapFromTree();
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
  if (source == &presetManager) {
    // Phase 41.1: one rebuild after silent load (no per-event rebuilds during
    // load)
    rebuildMapFromTree();
  } else if (source == &deviceManager) {
    rebuildMapFromTree();
  } else if (source == &settingsManager) {
    rebuildMapFromTree();
  } else if (source == &zoneManager) {
    rebuildMapFromTree();
  }
}

void InputProcessor::rebuildMapFromTree() {
  juce::ScopedWriteLock lock(mapLock);
  // Phase 41: Static 9 layers (0=Base, 1-8=Overlays). Reset to 9 empty layers.
  layers.assign(9, Layer());
  layers[0].isActive = true;

  auto layersList = presetManager.getLayersList();
  const int maxLayer = static_cast<int>(layers.size());

  for (int layerIdx = 0; layerIdx < layersList.getNumChildren(); ++layerIdx) {
    auto layerNode = layersList.getChild(layerIdx);
    if (!layerNode.isValid() || !layerNode.hasType("Layer"))
      continue;

    if (!layerNode.hasProperty("id"))
      continue;
    int id = static_cast<int>(layerNode.getProperty("id", -1));

    // Phase 41.2: Bounds check – never touch layers[id] if out of range
    if (id < 0 || id >= maxLayer) {
      DBG("InputProcessor: Skipping invalid Layer ID: " + juce::String(id));
      continue;
    }

    layers[id].id = id;
    layers[id].name =
        layerNode.getProperty("name", "Layer " + juce::String(id)).toString();
    layers[id].isActive = layerNode.getProperty("isActive", id == 0);

    auto mappingsNode = layerNode.getChildWithName("Mappings");
    if (mappingsNode.isValid()) {
      for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
        auto mapping = mappingsNode.getChild(i);
        if (mapping.isValid()) {
          if (!mapping.hasProperty("layerID"))
            mapping.setProperty("layerID", id, nullptr);
          addMappingFromTree(mapping);
        }
      }
    }
  }

  // State flush: reset Sustain/Latch, then re-evaluate SustainInverse
  voiceManager.resetPerformanceState();
  for (const auto &layer : layers) {
    for (const auto &pair : layer.compiledMap) {
      const MidiAction &action = pair.second;
      if (action.type == ActionType::Command &&
          action.data1 ==
              static_cast<int>(OmniKey::CommandID::SustainInverse)) {
        voiceManager.setSustain(true);
        break;
      }
    }
  }
}

// Helper to parse Hex strings from XML correctly
uintptr_t parseDeviceHash(const juce::var &var) {
  if (var.isString())
    return (uintptr_t)var.toString().getHexValue64();
  return (uintptr_t)static_cast<juce::int64>(var);
}

// Helper to convert alias name to hash (simple string hash)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  const juce::String trimmed = aliasName.trim();
  if (trimmed.isEmpty() || trimmed.equalsIgnoreCase("Any / Master") ||
      trimmed.equalsIgnoreCase("Global (All Devices)") ||
      trimmed.equalsIgnoreCase("Global") ||
      trimmed.equalsIgnoreCase("Unassigned"))
    return 0; // Hash 0 = Global (All Devices)

  // Simple hash: use std::hash on the string
  return static_cast<uintptr_t>(std::hash<juce::String>{}(trimmed));
}

void InputProcessor::addMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

  int layerID = mappingNode.getProperty("layerID", 0);
  layerID = juce::jlimit(0, 8, layerID);
  // Phase 41: Static 9 layers; no creation
  if (layerID >= (int)layers.size())
    return;
  Layer &targetLayer = layers[layerID];

  int inputKey = mappingNode.getProperty("inputKey", 0);

  // Get alias name from mapping (new approach)
  juce::String aliasName = mappingNode.getProperty("inputAlias", "").toString();

  // Parse Type
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

  int channel = mappingNode.getProperty("channel", 1);
  int data1 = mappingNode.getProperty("data1", 60);
  int data2 = mappingNode.getProperty("data2", 127);
  int velocityRandom = mappingNode.getProperty("velRandom", 0);

  MidiAction action;
  action.type = actionType;
  action.channel = channel;
  action.data1 = data1;
  action.data2 = data2;
  action.velocityRandom = velocityRandom;

  // Load ADSR settings if type is Envelope
  if (actionType == ActionType::Envelope) {
    // Read ADSR properties (convert from UI format to internal format)
    int adsrAttack = mappingNode.getProperty("adsrAttack", 50);
    int adsrDecay = mappingNode.getProperty("adsrDecay", 0);
    int adsrSustain = mappingNode.getProperty("adsrSustain", 127); // 0-127
    int adsrRelease = mappingNode.getProperty("adsrRelease", 50);
    juce::String adsrTarget =
        mappingNode.getProperty("adsrTarget", "CC").toString();

    action.adsrSettings.attackMs = adsrAttack;
    action.adsrSettings.decayMs = adsrDecay;
    action.adsrSettings.sustainLevel =
        adsrSustain / 127.0f; // Convert 0-127 to 0.0-1.0
    action.adsrSettings.releaseMs = adsrRelease;

    // Set target type
    if (adsrTarget == "PitchBend") {
      action.adsrSettings.target = AdsrTarget::PitchBend;
    } else if (adsrTarget == "SmartScaleBend") {
      action.adsrSettings.target = AdsrTarget::SmartScaleBend;
    } else {
      action.adsrSettings.target = AdsrTarget::CC;
    }
    action.adsrSettings.ccNumber = data1; // Use data1 for CC number

    // If Pitch Bend, calculate peak value from musical parameters using Global
    // Range
    if (action.adsrSettings.target == AdsrTarget::PitchBend) {
      // Read pbShift (semitones) from mapping (default: 0)
      int pbShift = mappingNode.getProperty("pbShift", 0);

      // Get global range from SettingsManager (Phase 43: prevent div/0)
      int globalRange = settingsManager.getPitchBendRange();
      if (globalRange < 1)
        globalRange = 12;

      // Calculate target MIDI value: 8192 (center) + (shift * steps per
      // semitone)
      double stepsPerSemitone = 8192.0 / static_cast<double>(globalRange);
      int calculatedPeak =
          static_cast<int>(8192.0 + (pbShift * stepsPerSemitone));
      calculatedPeak = juce::jlimit(0, 16383, calculatedPeak);

      // Overwrite data2 with calculated peak value (ensures consistency)
      action.data2 = calculatedPeak;
    } else if (action.adsrSettings.target == AdsrTarget::SmartScaleBend) {
      // Compile SmartScaleBend lookup table
      // Read smartStepShift (scale steps) from mapping (default: 0)
      int smartStepShift = mappingNode.getProperty("smartStepShift", 0);

      // Get global range from SettingsManager (Phase 43: prevent div/0)
      int globalRange = settingsManager.getPitchBendRange();
      if (globalRange < 1)
        globalRange = 12;

      // Get global scale intervals and root from ZoneManager
      juce::String globalScaleName = zoneManager.getGlobalScaleName();
      int globalRoot = zoneManager.getGlobalRootNote();
      std::vector<int> globalIntervals =
          scaleLibrary.getIntervals(globalScaleName);

      // Pre-compile lookup table for all 128 MIDI notes
      action.smartBendLookup.resize(128);
      double stepsPerSemitone = 8192.0 / static_cast<double>(globalRange);

      for (int note = 0; note < 128; ++note) {
        // Find current scale degree of this note
        int currentDegree =
            ScaleUtilities::findScaleDegree(note, globalRoot, globalIntervals);

        // Calculate target degree
        int targetDegree = currentDegree + smartStepShift;

        // Calculate target MIDI note
        int targetNote = ScaleUtilities::calculateMidiNote(
            globalRoot, globalIntervals, targetDegree);

        // Calculate semitone delta
        int semitoneDelta = targetNote - note;

        // Calculate Pitch Bend value: 8192 (center) + (delta * steps per
        // semitone)
        int pbValue =
            static_cast<int>(8192.0 + (semitoneDelta * stepsPerSemitone));
        pbValue = juce::jlimit(0, 16383, pbValue);

        // Store in lookup table
        action.smartBendLookup[note] = pbValue;
      }
    }
  }

  // Phase 39.3: Populate both maps

  // Calculate alias hash for configMap (Phase 39.3)
  // Phase 47: be robust about "Global" naming variants.
  uintptr_t aliasHash = aliasNameToHash(aliasName);

  // Populate configMap (UI/Visualization) - One entry per mapping using alias
  // hash
  InputID configInputId = {aliasHash, inputKey};
  targetLayer.configMap[configInputId] = action;

  // Compile alias into hardware IDs for compiledMap (Audio processing)
  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds =
        deviceManager.getHardwareForAlias(aliasName);

    // For each hardware ID in the alias, create a mapping entry in compiledMap
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      targetLayer.compiledMap[inputId] = action;
    }

    // Also add to compiledMap with hash 0 if this is a global mapping
    // Phase 47: Ensure Global mappings are compiled even if hardware list is
    // empty.
    if (aliasHash == 0) {
      InputID globalInputId = {0, inputKey};
      targetLayer.compiledMap[globalInputId] = action;
    }
  } else {
    // Fallback: support legacy deviceHash property for backward compatibility
    // Check if deviceHash exists (legacy preset)
    juce::var deviceHashVar = mappingNode.getProperty("deviceHash");
    if (!deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty()) {
      uintptr_t deviceHash = parseDeviceHash(deviceHashVar);

      // Try to find an alias for this hardware ID
      juce::String foundAlias = deviceManager.getAliasForHardware(deviceHash);

      if (foundAlias != "Unassigned") {
        // Hardware is already assigned to an alias, use that alias
        uintptr_t foundAliasHash = aliasNameToHash(foundAlias);

        // Add to configMap
        InputID configInputId = {foundAliasHash, inputKey};
        targetLayer.configMap[configInputId] = action;

        // Add to compiledMap
        juce::Array<uintptr_t> hardwareIds =
            deviceManager.getHardwareForAlias(foundAlias);
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          targetLayer.compiledMap[inputId] = action;
        }

        // Also add global entry if alias hash is 0
        if (foundAliasHash == 0) {
          InputID globalInputId = {0, inputKey};
          targetLayer.compiledMap[globalInputId] = action;
        }
      } else {
        // Phase 46.1: Legacy preset with unassigned hardware.
        // Do NOT auto-create "Master Input" or assign hardware to any alias.
        // Use the hardware hash directly so the mapping still works, but keep
        // the hardware unassigned (visible in Device Setup's "[ Unassigned
        // Devices ]"). Add to configMap using the hardware hash directly (not
        // an alias hash)
        InputID configInputId = {deviceHash, inputKey};
        targetLayer.configMap[configInputId] = action;

        // Add to compiledMap using the hardware hash directly
        InputID inputId = {deviceHash, inputKey};
        targetLayer.compiledMap[inputId] = action;
      }
    } else {
      // Legacy: deviceHash is 0 (Global)
      // Add to configMap with hash 0
      InputID configInputId = {0, inputKey};
      targetLayer.configMap[configInputId] = action;

      // Add to compiledMap with hash 0
      InputID globalInputId = {0, inputKey};
      targetLayer.compiledMap[globalInputId] = action;
    }
    // If neither aliasName nor deviceHash exists, the mapping is invalid and
    // will be silently dropped
  }
}

void InputProcessor::removeMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

  int layerID = mappingNode.getProperty("layerID", 0);
  layerID = juce::jlimit(0, 8, layerID);
  if (layerID >= (int)layers.size())
    return;
  Layer &targetLayer = layers[layerID];

  int inputKey = mappingNode.getProperty("inputKey", 0);

  // Phase 40: Remove from target layer's maps

  juce::String aliasName = mappingNode.getProperty("inputAlias", "").toString();
  uintptr_t aliasHash = 0;
  if (!aliasName.isEmpty() && aliasName != "Global (All Devices)" &&
      aliasName != "Any / Master" && aliasName != "Global") {
    aliasHash = aliasNameToHash(aliasName);
  }

  InputID configInputId = {aliasHash, inputKey};
  targetLayer.configMap.erase(configInputId);

  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds =
        deviceManager.getHardwareForAlias(aliasName);
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      targetLayer.compiledMap.erase(inputId);
    }

    if (aliasHash == 0) {
      InputID globalInputId = {0, inputKey};
      targetLayer.compiledMap.erase(globalInputId);
    }
  } else {
    juce::var deviceHashVar = mappingNode.getProperty("deviceHash");
    if (!deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty()) {
      uintptr_t deviceHash = parseDeviceHash(deviceHashVar);

      juce::String foundAlias = deviceManager.getAliasForHardware(deviceHash);
      if (foundAlias != "Unassigned") {
        juce::Array<uintptr_t> hardwareIds =
            deviceManager.getHardwareForAlias(foundAlias);
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          targetLayer.compiledMap.erase(inputId);
        }
      } else {
        InputID inputId = {deviceHash, inputKey};
        targetLayer.compiledMap.erase(inputId);
      }
    } else {
      InputID globalInputId = {0, inputKey};
      targetLayer.compiledMap.erase(globalInputId);
    }
  }
}

void InputProcessor::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  if (presetManager.getIsLoading())
    return;

  // Phase 41: Handle layer hierarchy changes
  if (parentTree.hasType("Layers")) {
    // A new Layer was added
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    return;
  }

  if (childWhichHasBeenAdded.hasType("Layer")) {
    // Layer node was added
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    return;
  }

  // Case A: The "Mappings" folder itself was added (e.g. Loading a file)
  if (childWhichHasBeenAdded.hasType("Mappings")) {
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    return;
  }

  // Case B: A single Mapping was added to a Layer's Mappings folder
  if (parentTree.hasType("Mappings")) {
    // Check if parent is a Layer's Mappings
    auto layerNode = parentTree.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      juce::ScopedWriteLock lock(mapLock);
      addMappingFromTree(childWhichHasBeenAdded);
      return;
    }
  }

  // Legacy: A single Mapping was added to the flat Mappings folder
  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    addMappingFromTree(childWhichHasBeenAdded);
  }
}

void InputProcessor::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {
  if (presetManager.getIsLoading())
    return;

  // Phase 41: Handle layer removal
  if (parentTree.hasType("Layers")) {
    // A Layer was removed
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    return;
  }

  // Case A: The "Mappings" folder was removed (e.g. Loading start)
  if (childWhichHasBeenRemoved.hasType("Mappings")) {
    rebuildMapFromTree(); // Phase 41: Keeps static 9 layers (takes mapLock)
    return;
  }

  // Case B: A single Mapping was removed from a Layer's Mappings
  if (parentTree.hasType("Mappings")) {
    auto layerNode = parentTree.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      juce::ScopedWriteLock lock(mapLock);
      removeMappingFromTree(childWhichHasBeenRemoved);
      return;
    }
  }

  // Legacy: A single Mapping was removed from flat Mappings
  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    removeMappingFromTree(childWhichHasBeenRemoved);
  }
}

void InputProcessor::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  if (presetManager.getIsLoading())
    return;

  // Phase 41: Handle layer property changes (name, isActive)
  if (treeWhosePropertyHasChanged.hasType("Layer")) {
    if (property == juce::Identifier("name") ||
        property == juce::Identifier("isActive")) {
      juce::ScopedWriteLock lock(mapLock);
      int layerId = treeWhosePropertyHasChanged.getProperty("id", -1);
      if (layerId >= 0 && layerId < (int)layers.size()) {
        layers[layerId].name =
            treeWhosePropertyHasChanged
                .getProperty("name", "Layer " + juce::String(layerId))
                .toString();
        layers[layerId].isActive =
            treeWhosePropertyHasChanged.getProperty("isActive", layerId == 0);
      }
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
      property == juce::Identifier("adsrRelease")) {
    if (isLayerMapping || parent.isEquivalentTo(mappingsNode) ||
        treeWhosePropertyHasChanged.isEquivalentTo(mappingsNode)) {
      // SAFER: Rebuild everything.
      // The logic to "Remove Old -> Add New" is impossible here because
      // we don't know the Old values anymore (the Tree is already updated).
      juce::ScopedWriteLock lock(mapLock);
      rebuildMapFromTree();
    }
  }
}

const MidiAction *InputProcessor::findMapping(const InputID &input) {
  // Phase 40: Check active layers from top (highest index) down to 0
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i].isActive)
      continue;
    auto &cm = layers[i].compiledMap;
    auto it = cm.find(input);
    if (it != cm.end())
      return &it->second;
    if (input.deviceHandle != 0) {
      InputID anyDevice = {0, input.keyCode};
      it = cm.find(anyDevice);
      if (it != cm.end())
        return &it->second;
    }
  }
  return nullptr;
}

std::optional<MidiAction> InputProcessor::getMappingForInput(InputID input) {
  juce::ScopedReadLock lock(mapLock);
  const MidiAction *ptr = findMapping(input);
  if (ptr != nullptr)
    return *ptr;
  return std::nullopt;
}

// Shared lookup logic for processEvent and simulateInput
// Returns: {action, sourceDescription}
std::pair<std::optional<MidiAction>, juce::String>
InputProcessor::lookupAction(uintptr_t deviceHandle, int keyCode) {
  // LOGIC CHANGE: Studio Mode Check - Force Global ID if Studio Mode is OFF
  uintptr_t effectiveDevice = deviceHandle;
  if (!settingsManager.isStudioMode()) {
    effectiveDevice = 0; // Force Global ID
  }

  // Step 1: Get alias name for hardware ID
  juce::String aliasName = deviceManager.getAliasForHardware(effectiveDevice);

  // Convert alias name to hash
  uintptr_t aliasHash = 0;
  if (aliasName != "Unassigned" && !aliasName.isEmpty()) {
    aliasHash = aliasNameToHash(aliasName);
  }

  // Correct priority order: Manual mappings have priority over zones
  // Step 2: Check manual mappings (specific alias/hardware first)
  InputID input = {effectiveDevice, keyCode};
  juce::ScopedReadLock lock(mapLock);
  const MidiAction *action = findMapping(input);
  if (action != nullptr) {
    return {*action, "Mapping"};
  }

  // Step 3: Check manual mappings (global/wildcard)
  if (effectiveDevice != 0) {
    InputID anyDevice = {0, keyCode};
    action = findMapping(anyDevice);
    if (action != nullptr) {
      return {*action, "Mapping"};
    }
  }

  // Step 4: Check specific alias zones (after manual mappings)
  if (aliasHash != 0) {
    InputID aliasInputID = {aliasHash, keyCode};
    auto [zoneAction, zoneName] = zoneManager.handleInputWithName(aliasInputID);
    if (zoneAction.has_value()) {
      return {zoneAction, "Zone: " + zoneName};
    }
  }

  // Step 5: Check wildcard zone (hash 0 = Global, lowest priority)
  InputID wildcardInputID = {0, keyCode};
  auto [zoneAction, zoneName] =
      zoneManager.handleInputWithName(wildcardInputID);
  if (zoneAction.has_value()) {
    return {zoneAction, "Zone: " + zoneName};
  }

  return {std::nullopt, ""};
}

// Resolve zone using same InputID as lookupAction: zoneLookupTable is keyed by
// (targetAliasHash, keyCode), not (deviceHandle, keyCode). For "Any" zones
// targetAliasHash=0. We must try (aliasHash, keyCode) and (0, keyCode).
int InputProcessor::calculateVelocity(int base, int range) {
  if (range <= 0)
    return base;

  // Generate random delta in range [-range, +range]
  int delta = random.nextInt(range * 2 + 1) - range;

  // Clamp result between 1 and 127
  return juce::jlimit(1, 127, base + delta);
}

std::shared_ptr<Zone> InputProcessor::getZoneForInputResolved(InputID input) {
  // LOGIC CHANGE: Studio Mode Check - Force Global ID if Studio Mode is OFF
  uintptr_t effectiveDevice = input.deviceHandle;
  if (!settingsManager.isStudioMode()) {
    effectiveDevice = 0; // Force Global ID
  }

  juce::String aliasName = deviceManager.getAliasForHardware(effectiveDevice);
  uintptr_t aliasHash = (aliasName != "Unassigned" && !aliasName.isEmpty())
                            ? aliasNameToHash(aliasName)
                            : 0;
  if (aliasHash != 0) {
    auto z = zoneManager.getZoneForInput(InputID{aliasHash, input.keyCode});
    if (z)
      return z;
  }
  return zoneManager.getZoneForInput(InputID{0, input.keyCode});
}

void InputProcessor::processEvent(InputID input, bool isDown) {
  // Gate: If MIDI mode is not active, don't generate MIDI
  if (!settingsManager.isMidiModeActive()) {
    return;
  }

  if (!isDown) {
    auto [action, source] = lookupAction(input.deviceHandle, input.keyCode);
    // Phase 40: LayerMomentary key-up = turn layer off
    if (action.has_value() && action->type == ActionType::Command &&
        action->data1 == static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
      int layerId = juce::jlimit(0, (int)layers.size() - 1, action->data2);
      if (layerId >= 0 && layerId < (int)layers.size()) {
        juce::ScopedWriteLock wl(mapLock);
        layers[layerId].isActive = false;
      }
      return;
    }
    // Command key-up: SustainMomentary=Off, SustainInverse=On
    if (action.has_value() && action->type == ActionType::Command) {
      int cmd = action->data1;
      if (cmd == static_cast<int>(OmniKey::CommandID::SustainMomentary))
        voiceManager.setSustain(false);
      else if (cmd == static_cast<int>(OmniKey::CommandID::SustainInverse))
        voiceManager.setSustain(true);
    }
    // Envelope key-up: release envelope
    if (action.has_value() && action->type == ActionType::Envelope) {
      expressionEngine.releaseEnvelope(input);
      return;
    }
    if (action.has_value() && source.startsWith("Zone: ")) {
      auto zone = getZoneForInputResolved(input);
      if (zone && zone->playMode == Zone::PlayMode::Strum) {
        juce::ScopedWriteLock lock(bufferLock);
        noteBuffer.clear();
        bufferedStrumSpeedMs = 50;

        // Use zone's release behavior
        bool shouldSustain =
            (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain);
        voiceManager.handleKeyUp(input, zone->releaseDurationMs, shouldSustain);
      } else {
        voiceManager.handleKeyUp(input);
      }
    } else {
      voiceManager.handleKeyUp(input);
    }
    return;
  }

  // Use shared lookup logic
  auto [action, source] = lookupAction(input.deviceHandle, input.keyCode);

  if (action.has_value()) {
    const auto &midiAction = action.value();

    if (midiAction.type == ActionType::Envelope) {
      // Determine peak value based on target type
      int peakValue = midiAction.data2;

      if (midiAction.adsrSettings.target == AdsrTarget::SmartScaleBend) {
        // Use pre-compiled lookup table based on last triggered note
        if (!midiAction.smartBendLookup.empty() && lastTriggeredNote >= 0 &&
            lastTriggeredNote < 128) {
          peakValue = midiAction.smartBendLookup[lastTriggeredNote];
        } else {
          // Fallback: use center (8192) if lookup table not available
          peakValue = 8192;
        }
      }

      expressionEngine.triggerEnvelope(input, midiAction.channel,
                                       midiAction.adsrSettings, peakValue);
      return;
    }

    if (midiAction.type == ActionType::Command) {
      int cmd = midiAction.data1;
      int layerId = midiAction.data2; // For layer commands, data2 = layer ID
      // Phase 40: Layer commands (consume key, update layer state)
      if (cmd == static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
        layerId = juce::jlimit(0, (int)layers.size() - 1, layerId);
        if (layerId >= 0 && layerId < (int)layers.size()) {
          juce::ScopedWriteLock wl(mapLock);
          layers[layerId].isActive = isDown;
        }
        return;
      }
      if (cmd == static_cast<int>(OmniKey::CommandID::LayerToggle)) {
        layerId = juce::jlimit(0, (int)layers.size() - 1, layerId);
        if (layerId >= 0 && layerId < (int)layers.size()) {
          juce::ScopedWriteLock wl(mapLock);
          layers[layerId].isActive = !layers[layerId].isActive;
        }
        return;
      }
      if (cmd == static_cast<int>(OmniKey::CommandID::LayerSolo)) {
        layerId = juce::jlimit(0, (int)layers.size() - 1, layerId);
        if (layerId >= 0 && layerId < (int)layers.size()) {
          juce::ScopedWriteLock wl(mapLock);
          for (size_t i = 0; i < layers.size(); ++i)
            layers[i].isActive = (i == (size_t)layerId);
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
      } else if (cmd == static_cast<int>(OmniKey::CommandID::GlobalPitchDown)) {
        int chrom = zoneManager.getGlobalChromaticTranspose();
        int deg = zoneManager.getGlobalDegreeTranspose();
        zoneManager.setGlobalTranspose(chrom - 1, deg);
      } else if (cmd == static_cast<int>(OmniKey::CommandID::GlobalModeUp)) {
        int chrom = zoneManager.getGlobalChromaticTranspose();
        int deg = zoneManager.getGlobalDegreeTranspose();
        zoneManager.setGlobalTranspose(chrom, deg + 1);
      } else if (cmd == static_cast<int>(OmniKey::CommandID::GlobalModeDown)) {
        int chrom = zoneManager.getGlobalChromaticTranspose();
        int deg = zoneManager.getGlobalDegreeTranspose();
        zoneManager.setGlobalTranspose(chrom, deg - 1);
      }
      return;
    }

    if (midiAction.type == ActionType::Note) {
      if (source.startsWith("Zone: ")) {
        auto zone = getZoneForInputResolved(input);
        if (zone) {
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
                // Ghost notes use scaled velocity
                int ghostVel =
                    static_cast<int>(mainVelocity * zone->ghostVelocityScale);
                finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
              } else {
                finalVelocities.push_back(mainVelocity);
              }
            }

            if (zone->playMode == Zone::PlayMode::Direct) {
              int releaseMs = zone->releaseDurationMs;

              // Calculate adaptive glide speed if enabled (Phase 26.1)
              int glideSpeed = zone->glideTimeMs; // Default to static time
              if (zone->isAdaptiveGlide &&
                  zone->polyphonyMode == PolyphonyMode::Legato) {
                rhythmAnalyzer.logTap(); // Log note onset
                glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(
                    zone->glideTimeMs, zone->maxGlideTimeMs);
              }

              if (finalNotes.size() > 1) {
                voiceManager.noteOn(input, finalNotes, finalVelocities,
                                    midiAction.channel, zone->strumSpeedMs,
                                    allowSustain, releaseMs,
                                    zone->polyphonyMode, glideSpeed);
                // Track last note (use first note of chord)
                if (!finalNotes.empty()) {
                  lastTriggeredNote = finalNotes.front();
                }
              } else {
                voiceManager.noteOn(input, finalNotes.front(),
                                    finalVelocities.front(), midiAction.channel,
                                    allowSustain, releaseMs,
                                    zone->polyphonyMode, glideSpeed);
                // Track last note
                lastTriggeredNote = finalNotes.front();
              }
            } else if (zone->playMode == Zone::PlayMode::Strum) {
              int strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50;

              // Calculate adaptive glide speed if enabled (Phase 26.1)
              int glideSpeed = zone->glideTimeMs; // Default to static time
              if (zone->isAdaptiveGlide &&
                  zone->polyphonyMode == PolyphonyMode::Legato) {
                rhythmAnalyzer.logTap(); // Log note onset
                glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(
                    zone->glideTimeMs, zone->maxGlideTimeMs);
              }

              voiceManager.handleKeyUp(lastStrumSource);
              voiceManager.noteOn(input, finalNotes, finalVelocities,
                                  midiAction.channel, strumMs, allowSustain, 0,
                                  zone->polyphonyMode, glideSpeed);
              lastStrumSource = input;
              // Track last note (use first note of chord for strum)
              if (!finalNotes.empty()) {
                lastTriggeredNote = finalNotes.front();
              }
              {
                juce::ScopedWriteLock bufferWriteLock(bufferLock);
                // Store pitches only for visualizer (backward compatibility)
                noteBuffer = finalNotes;
                bufferedStrumSpeedMs = strumMs;
              }
            }
          } else {
            // Fallback: use mapping velocity with randomization
            int vel =
                calculateVelocity(midiAction.data2, midiAction.velocityRandom);
            voiceManager.noteOn(input, midiAction.data1, vel,
                                midiAction.channel, allowSustain, 0,
                                PolyphonyMode::Poly, 50);
            // Track last note
            lastTriggeredNote = midiAction.data1;
          }
        } else {
          // Fallback: use mapping velocity with randomization
          int vel =
              calculateVelocity(midiAction.data2, midiAction.velocityRandom);
          voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel,
                              true, 0, PolyphonyMode::Poly, 50);
          // Track last note
          lastTriggeredNote = midiAction.data1;
        }
      } else {
        // Manual mapping: use mapping velocity with randomization
        int vel =
            calculateVelocity(midiAction.data2, midiAction.velocityRandom);
        voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel,
                            true, 0, PolyphonyMode::Poly, 50);
        // Track last note
        lastTriggeredNote = midiAction.data1;
      }
    }
  }
}

std::vector<int> InputProcessor::getBufferedNotes() {
  juce::ScopedReadLock lock(bufferLock);
  return noteBuffer;
}

juce::StringArray InputProcessor::getActiveLayerNames() {
  juce::StringArray result;
  juce::ScopedReadLock lock(mapLock);
  for (const auto &layer : layers) {
    if (layer.isActive) {
      auto name = layer.name;
      if (name.isEmpty())
        name = "Layer " + juce::String(layer.id);
      result.add(name);
    }
  }
  return result;
}

bool InputProcessor::hasManualMappingForKey(int keyCode) {
  juce::ScopedReadLock lock(mapLock);
  // Phase 40: Check active layers' compiledMaps
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i].isActive)
      continue;
    for (const auto &p : layers[i].compiledMap) {
      if (p.first.keyCode == keyCode)
        return true;
    }
  }
  return false;
}

std::optional<ActionType> InputProcessor::getMappingType(int keyCode,
                                                         uintptr_t aliasHash) {
  juce::ScopedReadLock lock(mapLock);
  // Phase 40: Check active layers' configMaps from top down
  InputID id{aliasHash, keyCode};
  InputID globalId{0, keyCode};
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i].isActive)
      continue;
    auto it = layers[i].configMap.find(id);
    if (it != layers[i].configMap.end())
      return it->second.type;
    it = layers[i].configMap.find(globalId);
    if (it != layers[i].configMap.end())
      return it->second.type;
  }
  return std::nullopt;
}

int InputProcessor::getManualMappingCountForKey(int keyCode,
                                                uintptr_t aliasHash) const {
  juce::ScopedReadLock lock(mapLock);
  int count = 0;
  // Phase 40: Count layers that have a manual mapping for (keyCode, aliasHash)
  InputID key = {aliasHash, keyCode};
  InputID globalKey = {0, keyCode};
  for (const auto &layer : layers) {
    if (layer.configMap.find(key) != layer.configMap.end())
      count++;
    else if (aliasHash != 0 &&
             layer.configMap.find(globalKey) != layer.configMap.end())
      count++;
  }
  return count;
}

void InputProcessor::forceRebuildMappings() {
  juce::ScopedWriteLock lock(mapLock);
  rebuildMapFromTree();
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash,
                                               int keyCode) {
  SimulationResult result;
  juce::ScopedReadLock lock(mapLock);

  // Phase 9.5: Studio Mode Check - Force Global ID if Studio Mode is OFF
  if (!settingsManager.isStudioMode()) {
    viewDeviceHash = 0; // Force Global ID
  }

  // Phase 40: Iterate active layers from top down, use configMap per layer
  InputID specificInput = {viewDeviceHash, keyCode};
  InputID globalInput = {0, keyCode};
  bool hasSpecificMap = false;
  bool hasGlobalMap = false;
  std::optional<MidiAction> manualAction;
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i].isActive)
      continue;
    auto &cm = layers[i].configMap;
    if (viewDeviceHash != 0) {
      auto it = cm.find(specificInput);
      if (it != cm.end()) {
        hasSpecificMap = true;
        manualAction = it->second;
        break;
      }
      it = cm.find(globalInput);
      if (it != cm.end()) {
        hasGlobalMap = true;
        manualAction = it->second;
        break;
      }
    } else {
      auto it = cm.find(globalInput);
      if (it != cm.end()) {
        hasGlobalMap = true;
        manualAction = it->second;
        break;
      }
    }
  }

  // 3. Check Specific Zone
  std::optional<MidiAction> specificZone;
  if (viewDeviceHash != 0) {
    auto zoneResult = zoneManager.handleInput(specificInput);
    if (zoneResult.has_value()) {
      specificZone = zoneResult;
    }
  }

  // 4. Check Global Zone
  auto globalZone = zoneManager.handleInput(globalInput);

  // --- LOGIC TREE (Phase 39.1) ---

  if (viewDeviceHash == 0) {
    // MASTER VIEW: Only check Global (strict isolation)
    if (hasGlobalMap && manualAction.has_value()) {
      result.action = manualAction;
      result.sourceName = "Mapping";
      result.isZone = false;

      // Check for multiple global mappings (Mapping vs Mapping conflict)
      if (getManualMappingCountForKey(keyCode, 0) > 1) {
        result.state = VisualState::Conflict;
        result.sourceName = "Conflict: Multiple Mappings";
      } else {
        result.state = VisualState::Active;
      }
    } else if (globalZone.has_value()) {
      result.action = globalZone;
      result.state = VisualState::Active;
      result.sourceName = "Zone";
      result.isZone = true;
    }
    // else: Empty state (default)
  } else {
    // SPECIFIC VIEW

    // A. Specific Manual (Highest Priority)
    if (hasSpecificMap && manualAction.has_value()) {
      result.action = manualAction;
      result.sourceName = "Mapping";
      result.isZone = false;

      // Phase 39.8: Manual vs Zone Conflict Detection
      // CRITICAL CHECK: Manual vs Zone Collision

      // 1. Same Layer Collision (Local vs Local) -> Red Conflict
      if (specificZone.has_value()) {
        result.state = VisualState::Conflict;
        result.sourceName = "Conflict: Mapping + Zone";
      }
      // 2. Multiple Manual Mappings (Mapping vs Mapping) -> Red Conflict
      else if (getManualMappingCountForKey(keyCode, viewDeviceHash) > 1) {
        result.state = VisualState::Conflict;
        result.sourceName = "Conflict: Multiple Mappings";
      }
      // 3. Hierarchy Override (Local vs Global) -> Orange Override
      else if (hasGlobalMap || globalZone.has_value()) {
        result.state = VisualState::Override;
      }
      // 4. Clean
      else {
        result.state = VisualState::Active;
      }
    }
    // B. Global Manual (Inheritance)
    else if (hasGlobalMap && manualAction.has_value()) {
      result.action = manualAction;
      result.sourceName = "Mapping";
      result.isZone = false;

      // Check for multiple global mappings (Mapping vs Mapping conflict)
      if (getManualMappingCountForKey(keyCode, 0) > 1) {
        result.state = VisualState::Conflict;
        result.sourceName = "Conflict: Multiple Mappings";
      } else {
        result.state = VisualState::Inherited;
      }
    }
    // C. Specific Zone
    else if (specificZone.has_value()) {
      result.action = specificZone;
      result.sourceName = "Zone";
      result.isZone = true;
      if (globalZone.has_value()) {
        result.state = VisualState::Override;
      } else {
        result.state = VisualState::Active;
      }
    }
    // D. Global Zone (Inheritance)
    else if (globalZone.has_value()) {
      result.action = globalZone;
      result.state = VisualState::Inherited;
      result.sourceName = "Zone";
      result.isZone = true;
    }
    // else: Empty state (default)
  }

  // Phase 39.7: Conflict Detection
  // Check for zone overlaps if result is a zone
  if (result.isZone && result.state != VisualState::Empty) {
    int zoneCount = zoneManager.getZoneCountForKey(keyCode, viewDeviceHash);
    if (zoneCount > 1) {
      result.state = VisualState::Conflict;
    }
  }

  // Update legacy fields for backward compatibility
  result.updateLegacyFields();

  return result;
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash,
                                               int keyCode, int targetLayerId) {
  SimulationResult result;
  juce::ScopedReadLock lock(mapLock);

  // Phase 9.5: Studio Mode Check - Force Global ID if Studio Mode is OFF
  if (!settingsManager.isStudioMode()) {
    viewDeviceHash = 0; // Force Global ID
  }

  if (layers.empty()) {
    return result;
  }

  // Clamp target layer into valid range
  if (targetLayerId < 0 || targetLayerId >= (int)layers.size())
    targetLayerId = (int)layers.size() - 1;

  InputID specificInput{viewDeviceHash, keyCode};
  InputID globalInput{0, keyCode};

  // Search from targetLayerId down to 0, ignoring isActive so we can see
  // what would happen in this editing context.
  for (int i = targetLayerId; i >= 0; --i) {
    auto &cm = layers[i].configMap;
    if (viewDeviceHash != 0) {
      // 1) Specific device mapping
      if (auto it = cm.find(specificInput); it != cm.end()) {
        result.action = it->second;
        result.sourceName = "Mapping";
        result.isZone = false;
        result.state =
            (i == targetLayerId ? VisualState::Active : VisualState::Inherited);
        return result;
      }
      // 2) Global mapping (0) for this key
      if (auto it = cm.find(globalInput); it != cm.end()) {
        result.action = it->second;
        result.sourceName = "Mapping";
        result.isZone = false;
        // When editing a specific device, global mapping is inherited unless
        // we are explicitly viewing the global layer
        if (i == targetLayerId && viewDeviceHash == 0)
          result.state = VisualState::Active;
        else
          result.state = VisualState::Inherited;
        return result;
      }
    } else {
      // Studio/global view: only consider global mappings
      if (auto it = cm.find(globalInput); it != cm.end()) {
        result.action = it->second;
        result.sourceName = "Mapping";
        result.isZone = false;
        result.state =
            (i == targetLayerId ? VisualState::Active : VisualState::Inherited);
        return result;
      }
    }
  }

  // Fallback: consult zones using the same inputs
  std::optional<MidiAction> zoneAction;
  if (viewDeviceHash != 0) {
    zoneAction = zoneManager.handleInput(specificInput);
    if (!zoneAction.has_value())
      zoneAction = zoneManager.handleInput(globalInput);
  } else {
    zoneAction = zoneManager.handleInput(globalInput);
  }

  if (zoneAction.has_value()) {
    result.action = zoneAction;
    result.isZone = true;
    result.sourceName = "Zone";
    result.state = VisualState::Active;
  }

  return result;
}

void InputProcessor::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                     float value) {
  juce::ScopedWriteLock lock(mapLock);

  InputID input = {deviceHandle, inputCode};
  const MidiAction *action = findMapping(input);

  if (action == nullptr || action->type != ActionType::CC)
    return;

  float currentVal = 0.0f;
  // Note: Scroll is now handled as discrete key events (ScrollUp/ScrollDown)
  // so this method only handles pointer X/Y axis events
  bool isRelative = false; // Pointer events are absolute

  if (isRelative) {
    // Relative (Scroll): accumulate value
    auto it = currentCCValues.find(input);
    if (it != currentCCValues.end()) {
      currentVal = it->second;
    }
    // Sensitivity multiplier (adjust as needed)
    const float sensitivity = 1.0f;
    currentVal = std::clamp(currentVal + (value * sensitivity), 0.0f, 127.0f);
    currentCCValues[input] = currentVal;
  } else {
    // Absolute (Pointer): map 0.0-1.0 to 0-127
    currentVal = value * 127.0f;
  }

  int ccValue =
      static_cast<int>(std::round(std::clamp(currentVal, 0.0f, 127.0f)));

  // Send MIDI CC message
  voiceManager.sendCC(action->channel, action->data1, ccValue);
}

bool InputProcessor::hasPointerMappings() {
  juce::ScopedReadLock lock(mapLock);
  // Phase 40: Check active layers' compiledMaps
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[i].isActive)
      continue;
    for (const auto &pair : layers[i].compiledMap) {
      int keyCode = pair.first.keyCode;
      if (keyCode == 0x2000 || keyCode == 0x2001) // PointerX or PointerY
        return true;
    }
  }
  return false;
}