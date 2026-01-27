#include "InputProcessor.h"
#include "ChordUtilities.h"
#include "MappingTypes.h"
#include "ScaleLibrary.h"
#include "MidiEngine.h"
#include "SettingsManager.h"

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr,
                               DeviceManager &deviceMgr, ScaleLibrary &scaleLib, MidiEngine &midiEng, SettingsManager &settingsMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr),
      deviceManager(deviceMgr), zoneManager(scaleLib), scaleLibrary(scaleLib),
      expressionEngine(midiEng), settingsManager(settingsMgr) {
  // Add listeners
  presetManager.getRootNode().addListener(this);
  deviceManager.addChangeListener(this);
  settingsManager.addChangeListener(this);
  zoneManager.addChangeListener(this);

  // Populate map from existing tree
  rebuildMapFromTree();
}

InputProcessor::~InputProcessor() {
  // Remove listeners
  presetManager.getRootNode().removeListener(this);
  deviceManager.removeChangeListener(this);
  settingsManager.removeChangeListener(this);
  zoneManager.removeChangeListener(this);
}

void InputProcessor::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &deviceManager) {
    // Device alias configuration changed, rebuild the map
    rebuildMapFromTree();
  } else if (source == &settingsManager) {
    // Global settings changed (e.g., PB Range), rebuild the map to recalculate PB values
    rebuildMapFromTree();
  } else if (source == &zoneManager) {
    // Global scale/root changed, rebuild the map to recalculate SmartScaleBend lookup tables
    rebuildMapFromTree();
  }
}

void InputProcessor::rebuildMapFromTree() {
  juce::ScopedWriteLock lock(mapLock);
  compiledMap.clear();
  configMap.clear(); // Clear the UI map too (Phase 39.3)

  auto mappingsNode = presetManager.getMappingsNode();
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    addMappingFromTree(mappingsNode.getChild(i));
  }

  // State flush: reset Sustain/Latch, then re-evaluate SustainInverse
  voiceManager.resetPerformanceState();
  for (const auto &pair : compiledMap) {
    const MidiAction &action = pair.second;
    if (action.type == ActionType::Command &&
        action.data1 == static_cast<int>(OmniKey::CommandID::SustainInverse)) {
      voiceManager.setSustain(true);
      break;
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
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Global (All Devices)" || aliasName == "Global" ||
      aliasName == "Unassigned")
    return 0; // Hash 0 = Global (All Devices)

  // Simple hash: use std::hash on the string
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

void InputProcessor::addMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

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
    juce::String adsrTarget = mappingNode.getProperty("adsrTarget", "CC").toString();
    
    action.adsrSettings.attackMs = adsrAttack;
    action.adsrSettings.decayMs = adsrDecay;
    action.adsrSettings.sustainLevel = adsrSustain / 127.0f; // Convert 0-127 to 0.0-1.0
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
    
    // If Pitch Bend, calculate peak value from musical parameters using Global Range
    if (action.adsrSettings.target == AdsrTarget::PitchBend) {
      // Read pbShift (semitones) from mapping (default: 0)
      int pbShift = mappingNode.getProperty("pbShift", 0);
      
      // Get global range from SettingsManager
      int globalRange = settingsManager.getPitchBendRange();
      
      // Calculate target MIDI value: 8192 (center) + (shift * steps per semitone)
      double stepsPerSemitone = 8192.0 / static_cast<double>(globalRange);
      int calculatedPeak = static_cast<int>(8192.0 + (pbShift * stepsPerSemitone));
      calculatedPeak = juce::jlimit(0, 16383, calculatedPeak);
      
      // Overwrite data2 with calculated peak value (ensures consistency)
      action.data2 = calculatedPeak;
    } else if (action.adsrSettings.target == AdsrTarget::SmartScaleBend) {
      // Compile SmartScaleBend lookup table
      // Read smartStepShift (scale steps) from mapping (default: 0)
      int smartStepShift = mappingNode.getProperty("smartStepShift", 0);
      
      // Get global range from SettingsManager
      int globalRange = settingsManager.getPitchBendRange();
      
      // Get global scale intervals and root from ZoneManager
      juce::String globalScaleName = zoneManager.getGlobalScaleName();
      int globalRoot = zoneManager.getGlobalRootNote();
      std::vector<int> globalIntervals = scaleLibrary.getIntervals(globalScaleName);
      
      // Pre-compile lookup table for all 128 MIDI notes
      action.smartBendLookup.resize(128);
      double stepsPerSemitone = 8192.0 / static_cast<double>(globalRange);
      
      for (int note = 0; note < 128; ++note) {
        // Find current scale degree of this note
        int currentDegree = ScaleUtilities::findScaleDegree(note, globalRoot, globalIntervals);
        
        // Calculate target degree
        int targetDegree = currentDegree + smartStepShift;
        
        // Calculate target MIDI note
        int targetNote = ScaleUtilities::calculateMidiNote(globalRoot, globalIntervals, targetDegree);
        
        // Calculate semitone delta
        int semitoneDelta = targetNote - note;
        
        // Calculate Pitch Bend value: 8192 (center) + (delta * steps per semitone)
        int pbValue = static_cast<int>(8192.0 + (semitoneDelta * stepsPerSemitone));
        pbValue = juce::jlimit(0, 16383, pbValue);
        
        // Store in lookup table
        action.smartBendLookup[note] = pbValue;
      }
    }
  }

  // Phase 39.3: Populate both maps
  
  // Calculate alias hash for configMap (Phase 39.3)
  uintptr_t aliasHash = 0;
  if (!aliasName.isEmpty() && aliasName != "Global (All Devices)" && aliasName != "Any / Master" && aliasName != "Global") {
    aliasHash = aliasNameToHash(aliasName);
  }
  // If aliasName is empty, aliasHash remains 0 (Global)
  
  // Populate configMap (UI/Visualization) - One entry per mapping using alias hash
  InputID configInputId = {aliasHash, inputKey};
  configMap[configInputId] = action;
  
  // Compile alias into hardware IDs for compiledMap (Audio processing)
  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds =
        deviceManager.getHardwareForAlias(aliasName);

    // For each hardware ID in the alias, create a mapping entry in compiledMap
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      compiledMap[inputId] = action;
    }
    
    // Also add to compiledMap with hash 0 if this is a global mapping
    if (aliasHash == 0) {
      InputID globalInputId = {0, inputKey};
      compiledMap[globalInputId] = action;
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
        configMap[configInputId] = action;
        
        // Add to compiledMap
        juce::Array<uintptr_t> hardwareIds =
            deviceManager.getHardwareForAlias(foundAlias);
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          compiledMap[inputId] = action;
        }
        
        // Also add global entry if alias hash is 0
        if (foundAliasHash == 0) {
          InputID globalInputId = {0, inputKey};
          compiledMap[globalInputId] = action;
        }
      } else {
        // Legacy preset: hardware not assigned to any alias
        // Create a "Master Input" alias if it doesn't exist and assign this
        // hardware
        if (!deviceManager.aliasExists("Master Input")) {
          deviceManager.createAlias("Master Input");
        }
        deviceManager.assignHardware("Master Input", deviceHash);

        // Now use the alias
        uintptr_t masterAliasHash = aliasNameToHash("Master Input");
        
        // Add to configMap
        InputID configInputId = {masterAliasHash, inputKey};
        configMap[configInputId] = action;
        
        // Add to compiledMap
        juce::Array<uintptr_t> hardwareIds =
            deviceManager.getHardwareForAlias("Master Input");
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          compiledMap[inputId] = action;
        }
      }
    } else {
      // Legacy: deviceHash is 0 (Global)
      // Add to configMap with hash 0
      InputID configInputId = {0, inputKey};
      configMap[configInputId] = action;
      
      // Add to compiledMap with hash 0
      InputID globalInputId = {0, inputKey};
      compiledMap[globalInputId] = action;
    }
    // If neither aliasName nor deviceHash exists, the mapping is invalid and
    // will be silently dropped
  }
}

void InputProcessor::removeMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

  int inputKey = mappingNode.getProperty("inputKey", 0);

  // Phase 39.3: Remove from both maps
  
  // Calculate alias hash for configMap removal
  juce::String aliasName = mappingNode.getProperty("inputAlias", "").toString();
  uintptr_t aliasHash = 0;
  if (!aliasName.isEmpty() && aliasName != "Global (All Devices)" && aliasName != "Any / Master" && aliasName != "Global") {
    aliasHash = aliasNameToHash(aliasName);
  }
  
  // Remove from configMap
  InputID configInputId = {aliasHash, inputKey};
  configMap.erase(configInputId);

  // Remove from compiledMap (all hardware IDs for this alias)
  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds =
        deviceManager.getHardwareForAlias(aliasName);
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      compiledMap.erase(inputId);
    }
    
    // Also remove global entry if alias hash is 0
    if (aliasHash == 0) {
      InputID globalInputId = {0, inputKey};
      compiledMap.erase(globalInputId);
    }
  } else {
    // Fallback: legacy deviceHash
    juce::var deviceHashVar = mappingNode.getProperty("deviceHash");
    if (!deviceHashVar.isVoid() && !deviceHashVar.toString().isEmpty()) {
      uintptr_t deviceHash = parseDeviceHash(deviceHashVar);
      
      // Try to find alias for this hardware
      juce::String foundAlias = deviceManager.getAliasForHardware(deviceHash);
      if (foundAlias != "Unassigned") {
        juce::Array<uintptr_t> hardwareIds =
            deviceManager.getHardwareForAlias(foundAlias);
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          compiledMap.erase(inputId);
        }
      } else {
        // Direct hardware ID removal (legacy)
        InputID inputId = {deviceHash, inputKey};
        compiledMap.erase(inputId);
      }
    } else {
      // No alias or deviceHash - remove global entry
      InputID globalInputId = {0, inputKey};
      compiledMap.erase(globalInputId);
    }
  }
}

void InputProcessor::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {

  // Case A: The "Mappings" folder itself was added (e.g. Loading a file)
  if (childWhichHasBeenAdded.hasType("Mappings")) {
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
    return;
  }

  // Case B: A single Mapping was added to the Mappings folder
  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    addMappingFromTree(childWhichHasBeenAdded);
  }
}

void InputProcessor::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {

  // Case A: The "Mappings" folder was removed (e.g. Loading start)
  if (childWhichHasBeenRemoved.hasType("Mappings")) {
    juce::ScopedWriteLock lock(mapLock);
    compiledMap.clear();
    configMap.clear(); // Phase 39.3: Clear both maps
    return;
  }

  // Case B: A single Mapping was removed
  auto mappingsNode = presetManager.getMappingsNode();
  if (parentTree.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    removeMappingFromTree(childWhichHasBeenRemoved);
  }
}

void InputProcessor::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  auto mappingsNode = presetManager.getMappingsNode();
  auto parent = treeWhosePropertyHasChanged.getParent();
  
  // Optimization: Only rebuild if relevant properties changed
  if (property == juce::Identifier("inputKey") || 
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
      property == juce::Identifier("adsrRelease"))
  {
    if (parent.isEquivalentTo(mappingsNode) || treeWhosePropertyHasChanged.isEquivalentTo(mappingsNode))
    {
      // SAFER: Rebuild everything. 
      // The logic to "Remove Old -> Add New" is impossible here because 
      // we don't know the Old values anymore (the Tree is already updated).
      juce::ScopedWriteLock lock(mapLock);
      rebuildMapFromTree();
    }
  }
}

const MidiAction *InputProcessor::findMapping(const InputID &input) {
  // Phase 39.3: Use compiledMap for audio processing
  auto it = compiledMap.find(input);
  if (it != compiledMap.end()) {
    return &it->second;
  }

  if (input.deviceHandle != 0) {
    InputID anyDevice = {0, input.keyCode};
    it = compiledMap.find(anyDevice);
    if (it != compiledMap.end()) {
      return &it->second;
    }
  }

  return nullptr;
}

// --- THIS WAS THE MISSING FUNCTION CAUSING THE LINKER ERROR ---
const MidiAction *InputProcessor::getMappingForInput(InputID input) {
  juce::ScopedReadLock lock(mapLock);
  return findMapping(input);
}
// -------------------------------------------------------------

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

  juce::String aliasName =
      deviceManager.getAliasForHardware(effectiveDevice);
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
        if (!midiAction.smartBendLookup.empty() && lastTriggeredNote >= 0 && lastTriggeredNote < 128) {
          peakValue = midiAction.smartBendLookup[lastTriggeredNote];
        } else {
          // Fallback: use center (8192) if lookup table not available
          peakValue = 8192;
        }
      }
      
      expressionEngine.triggerEnvelope(input, midiAction.channel, midiAction.adsrSettings, peakValue);
      return;
    }

    if (midiAction.type == ActionType::Command) {
      int cmd = midiAction.data1;
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
              if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
                rhythmAnalyzer.logTap(); // Log note onset
                glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs, zone->maxGlideTimeMs);
              }
              
              if (finalNotes.size() > 1) {
                voiceManager.noteOn(input, finalNotes, finalVelocities,
                                    midiAction.channel, zone->strumSpeedMs,
                                    allowSustain, releaseMs, zone->polyphonyMode, glideSpeed);
                // Track last note (use first note of chord)
                if (!finalNotes.empty()) {
                  lastTriggeredNote = finalNotes.front();
                }
              } else {
                voiceManager.noteOn(input, finalNotes.front(),
                                    finalVelocities.front(), midiAction.channel,
                                    allowSustain, releaseMs, zone->polyphonyMode, glideSpeed);
                // Track last note
                lastTriggeredNote = finalNotes.front();
              }
            } else if (zone->playMode == Zone::PlayMode::Strum) {
              int strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50;
              
              // Calculate adaptive glide speed if enabled (Phase 26.1)
              int glideSpeed = zone->glideTimeMs; // Default to static time
              if (zone->isAdaptiveGlide && zone->polyphonyMode == PolyphonyMode::Legato) {
                rhythmAnalyzer.logTap(); // Log note onset
                glideSpeed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs, zone->maxGlideTimeMs);
              }
              
              voiceManager.handleKeyUp(lastStrumSource);
              voiceManager.noteOn(input, finalNotes, finalVelocities,
                                  midiAction.channel, strumMs, allowSustain, 0, zone->polyphonyMode, glideSpeed);
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
                                midiAction.channel, allowSustain, 0, PolyphonyMode::Poly, 50);
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

bool InputProcessor::hasManualMappingForKey(int keyCode) {
  juce::ScopedReadLock lock(mapLock);
  // Phase 39.3: Check compiledMap (used for conflict detection)
  for (const auto &p : compiledMap) {
    if (p.first.keyCode == keyCode)
      return true;
  }
  return false;
}

std::optional<ActionType> InputProcessor::getMappingType(int keyCode, uintptr_t aliasHash) {
  juce::ScopedReadLock lock(mapLock);
  // Phase 39.3: Use configMap for UI/visualizer queries
  InputID id{aliasHash, keyCode};
  auto it = configMap.find(id);
  if (it != configMap.end())
    return it->second.type;
  it = configMap.find(InputID{0, keyCode});
  if (it != configMap.end())
    return it->second.type;
  return std::nullopt;
}

int InputProcessor::getManualMappingCountForKey(int keyCode, uintptr_t aliasHash) const {
  juce::ScopedReadLock lock(mapLock);
  int count = 0;
  
  // Count mappings from ValueTree (configMap overwrites duplicates)
  auto mappingsNode = presetManager.getMappingsNode();
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    auto mapping = mappingsNode.getChild(i);
    int mappingKey = mapping.getProperty("inputKey", 0);
    if (mappingKey != keyCode)
      continue;
    
    // Get alias name from mapping
    juce::String aliasName = mapping.getProperty("inputAlias", "").toString();
    
    // Convert alias name to hash
    uintptr_t mappingAliasHash = 0;
    if (!aliasName.isEmpty() && aliasName != "Global (All Devices)" && 
        aliasName != "Any / Master" && aliasName != "Global") {
      mappingAliasHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
    }
    
    // Check if this mapping matches the requested alias hash
    if (mappingAliasHash == aliasHash) {
      count++;
    }
  }
  
  return count;
}

void InputProcessor::forceRebuildMappings() {
  juce::ScopedWriteLock lock(mapLock);
  rebuildMapFromTree();
}

SimulationResult InputProcessor::simulateInput(uintptr_t viewDeviceHash, int keyCode) {
  SimulationResult result;
  juce::ScopedReadLock lock(mapLock);
  
  // Phase 9.5: Studio Mode Check - Force Global ID if Studio Mode is OFF
  if (!settingsManager.isStudioMode()) {
    viewDeviceHash = 0; // Force Global ID
  }
  
  // viewDeviceHash is already an alias hash (0 = Global, or specific alias hash)
  // Strict 4-layer hierarchy check (Phase 39.1)
  
  // Phase 39.3: Use configMap for visualization (alias-based lookup)
  // 1. Check Specific Manual
  InputID specificInput = {viewDeviceHash, keyCode};
  auto specificMap = (viewDeviceHash != 0) ? configMap.find(specificInput) : configMap.end();
  bool hasSpecificMap = (specificMap != configMap.end());
  
  // 2. Check Global Manual
  InputID globalInput = {0, keyCode};
  auto globalMap = configMap.find(globalInput);
  bool hasGlobalMap = (globalMap != configMap.end());
  
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
    if (hasGlobalMap) {
      result.action = globalMap->second;
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
    if (hasSpecificMap) {
      result.action = specificMap->second;
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
    else if (hasGlobalMap) {
      result.action = globalMap->second;
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
  // Phase 39.3: Check compiledMap (or configMap - both should have same entries)
  for (const auto &pair : compiledMap) {
    int keyCode = pair.first.keyCode;
    if (keyCode == 0x2000 || keyCode == 0x2001) // PointerX or PointerY
      return true;
  }
  return false;
}