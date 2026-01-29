#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "GridCompiler.h"
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "ScaleLibrary.h"
#include "SettingsManager.h"

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
  if (source == &presetManager) {
    // Phase 41.1: one rebuild after silent load (no per-event rebuilds during
    // load)
    rebuildMapFromTree();
    rebuildGrid();
  } else if (source == &deviceManager) {
    rebuildMapFromTree();
    rebuildGrid();
  } else if (source == &settingsManager) {
    rebuildMapFromTree();
    rebuildGrid();
  } else if (source == &zoneManager) {
    rebuildMapFromTree();
    rebuildGrid();
  }
}

void InputProcessor::rebuildGrid() {
  auto newContext = GridCompiler::compile(presetManager, deviceManager,
                                          zoneManager, settingsManager);
  {
    juce::ScopedWriteLock sl(contextLock);
    activeContext = std::move(newContext);
  }
  // Notify listeners (Visualizer, etc.) that the grid changed.
  sendChangeMessage();
}

void InputProcessor::rebuildMapFromTree() {
  juce::ScopedWriteLock lock(mapLock);
  // Phase 41: Static 9 layers (0=Base, 1-8=Overlays). Reset to 9 empty layers.
  layers.assign(9, Layer());
  for (int i = 0; i < (int)layers.size(); ++i) {
    layers[(size_t)i].id = i;
    layers[(size_t)i].name = (i == 0 ? "Base" : ("Layer " + juce::String(i)));
    layers[(size_t)i].isLatched = false;
    layers[(size_t)i].isMomentary = false;
  }

  // Rebuild invalidates any in-flight held-key state; drop it.
  currentlyHeldKeys.clear();

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
    // Phase 40.1: Persisted layer activation maps to latched state.
    layers[id].isLatched = (bool)layerNode.getProperty("isActive", id == 0);

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
  // Phase 50.5: Check compiled context for SustainInverse commands
  {
    juce::ScopedReadLock ctxLock(contextLock);
    auto ctx = activeContext;
    if (ctx) {
      // Check all layers in global grid for SustainInverse
      for (int layerIdx = 0; layerIdx < 9; ++layerIdx) {
        auto grid = ctx->globalGrids[(size_t)layerIdx];
        if (!grid)
          continue;
        for (const auto &slot : *grid) {
          if (slot.isActive && slot.action.type == ActionType::Command &&
              slot.action.data1 ==
                  static_cast<int>(OmniKey::CommandID::SustainInverse)) {
            voiceManager.setSustain(true);
            return; // Early exit
          }
        }
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
  // Phase 50.5: Grid compiler reads from managers (synced with ValueTree)
  // Just trigger a rebuild - no need to manually populate maps
  rebuildGrid();
  return;

#if 0
  // OLD CODE BELOW (kept for reference, but never executed):
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

    // Phase 47.2: debug compiler path
    DBG("Compiler: Alias '" + aliasName + "' has " +
        juce::String(hardwareIds.size()) + " devices.");

    // For each hardware ID in the alias, create a mapping entry in compiledMap
    for (uintptr_t hardwareId : hardwareIds) {
      DBG("Compiler: Adding { " +
          juce::String::toHexString((juce::int64)hardwareId) + ", " +
          juce::String(inputKey) + " }");
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
#endif
}

void InputProcessor::removeMappingFromTree(juce::ValueTree mappingNode) {
  // Phase 50.5: Grid compiler reads from managers (synced with ValueTree)
  // Just trigger a rebuild - no need to manually remove from maps
  rebuildGrid();
  return;

#if 0
  // OLD CODE BELOW (kept for reference, but never executed):
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
#endif
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
    rebuildGrid();
    return;
  }

  if (childWhichHasBeenAdded.hasType("Layer")) {
    // Layer node was added
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    rebuildGrid();
    return;
  }

  // Case A: The "Mappings" folder itself was added (e.g. Loading a file)
  if (childWhichHasBeenAdded.hasType("Mappings")) {
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    rebuildGrid();
    return;
  }

  // Case B: A single Mapping was added to a Layer's Mappings folder
  if (parentTree.hasType("Mappings")) {
    // Check if parent is a Layer's Mappings
    auto layerNode = parentTree.getParent();
    if (layerNode.isValid() && layerNode.hasType("Layer")) {
      juce::ScopedWriteLock lock(mapLock);
      addMappingFromTree(childWhichHasBeenAdded);
      rebuildGrid();
      return;
    }
  }

  // Legacy: A single Mapping was added to the flat Mappings folder
  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    addMappingFromTree(childWhichHasBeenAdded);
    rebuildGrid();
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
    rebuildGrid();
    return;
  }

  // Case A: The "Mappings" folder was removed (e.g. Loading start)
  if (childWhichHasBeenRemoved.hasType("Mappings")) {
    rebuildMapFromTree(); // Phase 41: Keeps static 9 layers (takes mapLock)
    rebuildGrid();
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
    rebuildGrid();
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
        // Phase 40.1: treat persisted isActive as latched state
        layers[layerId].isLatched =
            (bool)treeWhosePropertyHasChanged.getProperty("isActive",
                                                          layerId == 0);
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
      rebuildGrid(); // Phase 50.5: Only rebuild grid
    }
  }
}

const MidiAction *InputProcessor::findMapping(const InputID &input) {
  // Phase 50.5: Deprecated - use grid-based lookup instead
  // This function is kept for backward compatibility but should not be used
  static MidiAction dummyAction;
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return nullptr;

  int genericKey = getGenericKey(input.keyCode);
  uintptr_t effectiveDevice = input.deviceHandle;
  if (!settingsManager.isStudioMode())
    effectiveDevice = 0;

  juce::ScopedReadLock layerLock(mapLock);
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
      continue;

    std::shared_ptr<const AudioGrid> grid;
    if (effectiveDevice != 0) {
      auto it = ctx->deviceGrids.find(effectiveDevice);
      if (it != ctx->deviceGrids.end() && it->second[(size_t)i]) {
        grid = it->second[(size_t)i];
      }
    }
    if (!grid) {
      grid = ctx->globalGrids[(size_t)i];
    }

    if (!grid)
      continue;

    const auto &slot = (*grid)[(size_t)input.keyCode];
    if (slot.isActive) {
      dummyAction = slot.action;
      return &dummyAction;
    }

    if (genericKey != 0) {
      const auto &genSlot = (*grid)[(size_t)genericKey];
      if (genSlot.isActive) {
        dummyAction = genSlot.action;
        return &dummyAction;
      }
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
// Phase 50.5: Deprecated - processEvent now uses grid directly
// Returns: {action, sourceDescription}
std::pair<std::optional<MidiAction>, juce::String>
InputProcessor::lookupAction(uintptr_t deviceHandle, int keyCode) {
  // Phase 50.5: This function is deprecated - processEvent uses grid directly
  // Kept for backward compatibility but should not be used
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return {std::nullopt, ""};

  uintptr_t effectiveDevice = deviceHandle;
  if (!settingsManager.isStudioMode())
    effectiveDevice = 0;

  int genericKey = getGenericKey(keyCode);
  if (keyCode < 0 || keyCode > 0xFF)
    return {std::nullopt, ""};

  juce::ScopedReadLock lock(mapLock);
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
      continue;

    std::shared_ptr<const AudioGrid> grid;
    if (effectiveDevice != 0) {
      auto it = ctx->deviceGrids.find(effectiveDevice);
      if (it != ctx->deviceGrids.end() && it->second[(size_t)i]) {
        grid = it->second[(size_t)i];
      }
    }
    if (!grid) {
      grid = ctx->globalGrids[(size_t)i];
    }

    if (!grid)
      continue;

    const auto &slot = (*grid)[(size_t)keyCode];
    if (slot.isActive) {
      // Check if it's a zone by looking at visual grid
      auto visIt = ctx->visualLookup.find(0);
      if (visIt != ctx->visualLookup.end() && i < (int)visIt->second.size() &&
          visIt->second[(size_t)i]) {
        const auto &visSlot = (*visIt->second[(size_t)i])[(size_t)keyCode];
        juce::String source =
            visSlot.sourceName.startsWith("Zone: ") ? "Zone: " : "Mapping";
        return {slot.action, source};
      }
      return {slot.action, "Mapping"};
    }

    if (genericKey != 0) {
      const auto &genSlot = (*grid)[(size_t)genericKey];
      if (genSlot.isActive) {
        return {genSlot.action, "Mapping"};
      }
    }
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
  // Phase 49: zones are layered. Search active layers from top down.
  for (int i = (int)layers.size() - 1; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
      continue;

    if (aliasHash != 0) {
      auto z =
          zoneManager.getZoneForInput(InputID{aliasHash, input.keyCode}, i);
      if (z)
        return z;
    }

    auto z = zoneManager.getZoneForInput(InputID{0, input.keyCode}, i);
    if (z)
      return z;
  }

  return nullptr;
}

// Phase 40.1: Robust layer state machine.
// Phase 50.5: Recompute momentary layers using grid-based lookup.
bool InputProcessor::updateLayerState() {
  juce::ScopedWriteLock lock(mapLock);

  std::vector<bool> prevMomentary;
  prevMomentary.reserve(layers.size());
  for (const auto &layer : layers)
    prevMomentary.push_back(layer.isMomentary);

  // 1) Reset momentary
  for (auto &layer : layers)
    layer.isMomentary = false;

  // 2) Get context for grid lookup
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx) {
    // No context - can't update layer state
    return false;
  }

  // 3) Iterative resolution (chains like A->B->C)
  bool changed = true;
  int pass = 0;
  while (changed && pass < 5) {
    changed = false;

    for (const auto &held : currentlyHeldKeys) {
      const int genericKey = getGenericKey(held.keyCode);
      uintptr_t effectiveDevice = held.deviceHandle;
      if (!settingsManager.isStudioMode())
        effectiveDevice = 0;

      // Find the highest-priority mapping for this key among currently active
      // layers. Only LayerMomentary affects momentary state.
      for (int i = 8; i >= 0; --i) {
        if (!layers[(size_t)i].isActive())
          continue;

        // Try device grid, then global grid
        std::shared_ptr<const AudioGrid> grid;
        if (effectiveDevice != 0) {
          auto it = ctx->deviceGrids.find(effectiveDevice);
          if (it != ctx->deviceGrids.end() && it->second[(size_t)i]) {
            grid = it->second[(size_t)i];
          }
        }
        if (!grid) {
          grid = ctx->globalGrids[(size_t)i];
        }

        if (!grid)
          continue;

        // Check specific key, then generic key fallback
        const auto &slot = (*grid)[(size_t)held.keyCode];
        bool found = slot.isActive;
        const MidiAction *actionPtr = &slot.action;

        if (!found && genericKey != 0) {
          const auto &genSlot = (*grid)[(size_t)genericKey];
          if (genSlot.isActive) {
            found = true;
            actionPtr = &genSlot.action;
          }
        }

        if (found && actionPtr->type == ActionType::Command &&
            actionPtr->data1 ==
                static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
          int targetLayer =
              juce::jlimit(0, (int)layers.size() - 1, actionPtr->data2);
          if (!layers[(size_t)targetLayer].isMomentary) {
            layers[(size_t)targetLayer].isMomentary = true;
            changed = true;
          }
          break; // key handled at this layer
        }
      }
    }

    ++pass;
  }

  bool anyDiff = false;
  for (size_t i = 0; i < layers.size() && i < prevMomentary.size(); ++i) {
    if (layers[i].isMomentary != prevMomentary[i]) {
      anyDiff = true;
      break;
    }
  }
  return anyDiff;
}

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

  if (updateLayerState())
    sendChangeMessage();

  // Phase 50.5: Grid-based lookup (O(1) per layer)
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx) {
    // No compiled context yet - can't process
    return;
  }

  // Determine effective device (Studio Mode check)
  uintptr_t effectiveDevice = input.deviceHandle;
  if (!settingsManager.isStudioMode()) {
    effectiveDevice = 0; // Force Global ID
  }

  const int keyCode = input.keyCode;
  if (keyCode < 0 || keyCode > 0xFF)
    return;

  // Layer Loop: Iterate from 8 down to 0 (top-down priority)
  juce::ScopedReadLock layerLock(mapLock);
  for (int layerIdx = 8; layerIdx >= 0; --layerIdx) {
    if (!layers[(size_t)layerIdx].isActive())
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
      zone = zoneManager.getZoneForInput(InputID{aliasHash, keyCode}, layerIdx);
      if (!zone) {
        zone = zoneManager.getZoneForInput(InputID{0, keyCode}, layerIdx);
      }
    }

    if (!isDown) {
      // Key-up handling
      if (midiAction.type == ActionType::Command) {
        int cmd = midiAction.data1;
        if (cmd == static_cast<int>(OmniKey::CommandID::SustainMomentary))
          voiceManager.setSustain(false);
        else if (cmd == static_cast<int>(OmniKey::CommandID::SustainInverse))
          voiceManager.setSustain(true);
      }
      if (midiAction.type == ActionType::Envelope) {
        expressionEngine.releaseEnvelope(input);
        return;
      }
      if (zone && zone->playMode == Zone::PlayMode::Strum) {
        juce::ScopedWriteLock lock(bufferLock);
        noteBuffer.clear();
        bufferedStrumSpeedMs = 50;
        bool shouldSustain =
            (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain);
        voiceManager.handleKeyUp(input, zone->releaseDurationMs, shouldSustain);
      } else {
        voiceManager.handleKeyUp(input);
      }
      return; // Stop searching lower layers
    }

    // Key-down handling
    if (midiAction.type == ActionType::Envelope) {
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
        return; // Handled by updateLayerState()
      }
      if (cmd == static_cast<int>(OmniKey::CommandID::LayerToggle)) {
        layerId = juce::jlimit(0, (int)layers.size() - 1, layerId);
        if (layerId >= 0 && layerId < (int)layers.size()) {
          juce::ScopedWriteLock wl(mapLock);
          layers[(size_t)layerId].isLatched =
              !layers[(size_t)layerId].isLatched;
        }
        updateLayerState();
        sendChangeMessage();
        return;
      }
      if (cmd == static_cast<int>(OmniKey::CommandID::LayerSolo)) {
        layerId = juce::jlimit(0, (int)layers.size() - 1, layerId);
        if (layerId >= 0 && layerId < (int)layers.size()) {
          juce::ScopedWriteLock wl(mapLock);
          for (size_t i = 0; i < layers.size(); ++i)
            layers[i].isLatched = ((int)i == layerId);
        }
        updateLayerState();
        sendChangeMessage();
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

    if (midiAction.type == ActionType::CC) {
      int vel = calculateVelocity(midiAction.data2, midiAction.velocityRandom);
      voiceManager.sendCC(midiAction.channel, midiAction.data1, vel);
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
            velocities.push_back(calculateVelocity(a.data2, a.velocityRandom));
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
          voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel,
                              true, 0, PolyphonyMode::Poly, 50);
          lastTriggeredNote = midiAction.data1;
        }
      }
      return; // Stop searching lower layers
    }
  }
  // No match found in any active layer
}

std::vector<int> InputProcessor::getBufferedNotes() {
  juce::ScopedReadLock lock(bufferLock);
  return noteBuffer;
}

std::shared_ptr<const CompiledMapContext> InputProcessor::getContext() const {
  juce::ScopedReadLock rl(contextLock);
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
  juce::ScopedReadLock lock(mapLock);
  for (const auto &layer : layers) {
    if (layer.isActive()) {
      auto name = layer.name;
      if (name.isEmpty())
        name = "Layer " + juce::String(layer.id);
      result.add(name);
    }
  }
  return result;
}

bool InputProcessor::hasManualMappingForKey(int keyCode) {
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return false;

  juce::ScopedReadLock lock(mapLock);
  // Phase 50.5: Check active layers' grids (skip zones - check visualLookup
  // sourceName)
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
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
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return std::nullopt;

  juce::ScopedReadLock lock(mapLock);
  // Phase 50.5: Check active layers' grids from top down
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
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
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return 0;

  juce::ScopedReadLock lock(mapLock);
  int count = 0;
  // Phase 50.5: Count layers that have a manual mapping (not zones)
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
  juce::ScopedWriteLock lock(mapLock);
  // Phase 50.5: Only rebuild grid (rebuildMapFromTree removed)
  rebuildGrid();
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash,
                                               int keyCode) {
  SimulationResult result;
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx) {
    return result; // No compiled context
  }

  juce::ScopedReadLock lock(mapLock);

  // Phase 9.5: Studio Mode Check - Force Global ID if Studio Mode is OFF
  if (!settingsManager.isStudioMode()) {
    viewDeviceHash = 0; // Force Global ID
  }

  // Phase 50.5: Use visual grid for simulation (reads from CompiledMapContext)
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

  // Search layers from top to bottom (8 -> 0)
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
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
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx) {
    return result; // No compiled context
  }

  juce::ScopedReadLock lock(mapLock);

  // Phase 9.5: Studio Mode Check - Force Global ID if Studio Mode is OFF
  if (!settingsManager.isStudioMode()) {
    viewDeviceHash = 0; // Force Global ID
  }

  // Clamp target layer into valid range
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
  juce::ScopedReadLock ctxLock(contextLock);
  auto ctx = activeContext;
  if (!ctx)
    return false;

  juce::ScopedReadLock lock(mapLock);
  // Phase 50.5: Check active layers' grids for pointer mappings
  for (int i = 8; i >= 0; --i) {
    if (!layers[(size_t)i].isActive())
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