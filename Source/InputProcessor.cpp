#include "InputProcessor.h"

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr, DeviceManager &deviceMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr), deviceManager(deviceMgr) {
  // Add listeners
  presetManager.getRootNode().addListener(this);
  deviceManager.addChangeListener(this);

  // Populate map from existing tree
  rebuildMapFromTree();
}

InputProcessor::~InputProcessor() {
  // Remove listeners
  presetManager.getRootNode().removeListener(this);
  deviceManager.removeChangeListener(this);
}

void InputProcessor::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &deviceManager) {
    // Device alias configuration changed, rebuild the map
    rebuildMapFromTree();
  }
}

void InputProcessor::rebuildMapFromTree() {
  juce::ScopedWriteLock lock(mapLock);
  keyMapping.clear();

  auto mappingsNode = presetManager.getMappingsNode();
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    addMappingFromTree(mappingsNode.getChild(i));
  }
}

// Helper to parse Hex strings from XML correctly
uintptr_t parseDeviceHash(const juce::var &var) {
  if (var.isString())
    return (uintptr_t)var.toString().getHexValue64();
  return (uintptr_t)static_cast<juce::int64>(var);
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
    else if (t == "Macro")
      actionType = ActionType::Macro;
  } else if (typeVar.isInt()) {
    int t = static_cast<int>(typeVar);
    if (t == 1)
      actionType = ActionType::CC;
    if (t == 2)
      actionType = ActionType::Macro;
  }

  int channel = mappingNode.getProperty("channel", 1);
  int data1 = mappingNode.getProperty("data1", 60);
  int data2 = mappingNode.getProperty("data2", 127);

  MidiAction action = {actionType, channel, data1, data2};

  // Compile alias into hardware IDs
  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds = deviceManager.getHardwareForAlias(aliasName);
    
    // For each hardware ID in the alias, create a mapping entry
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      keyMapping[inputId] = action;
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
        juce::Array<uintptr_t> hardwareIds = deviceManager.getHardwareForAlias(foundAlias);
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          keyMapping[inputId] = action;
        }
      } else {
        // Legacy preset: hardware not assigned to any alias
        // Create a "Master Input" alias if it doesn't exist and assign this hardware
        if (!deviceManager.aliasExists("Master Input")) {
          deviceManager.createAlias("Master Input");
        }
        deviceManager.assignHardware("Master Input", deviceHash);
        
        // Now use the alias
        juce::Array<uintptr_t> hardwareIds = deviceManager.getHardwareForAlias("Master Input");
        for (uintptr_t hardwareId : hardwareIds) {
          InputID inputId = {hardwareId, inputKey};
          keyMapping[inputId] = action;
        }
      }
    }
    // If neither aliasName nor deviceHash exists, the mapping is invalid and will be silently dropped
  }
}

void InputProcessor::removeMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

  int inputKey = mappingNode.getProperty("inputKey", 0);
  
  // Remove all entries for this mapping (could be multiple if alias has multiple hardware IDs)
  juce::String aliasName = mappingNode.getProperty("inputAlias", "").toString();
  
  if (!aliasName.isEmpty()) {
    juce::Array<uintptr_t> hardwareIds = deviceManager.getHardwareForAlias(aliasName);
    for (uintptr_t hardwareId : hardwareIds) {
      InputID inputId = {hardwareId, inputKey};
      keyMapping.erase(inputId);
    }
  } else {
    // Fallback: legacy deviceHash
    uintptr_t deviceHash = parseDeviceHash(mappingNode.getProperty("deviceHash"));
    InputID inputId = {deviceHash, inputKey};
    keyMapping.erase(inputId);
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
    keyMapping.clear();
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

  if (parent.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    removeMappingFromTree(treeWhosePropertyHasChanged);
    addMappingFromTree(treeWhosePropertyHasChanged);
  } else if (treeWhosePropertyHasChanged.isEquivalentTo(mappingsNode)) {
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
  }
}

const MidiAction *InputProcessor::findMapping(const InputID &input) {
  auto it = keyMapping.find(input);
  if (it != keyMapping.end()) {
    return &it->second;
  }

  if (input.deviceHandle != 0) {
    InputID anyDevice = {0, input.keyCode};
    it = keyMapping.find(anyDevice);
    if (it != keyMapping.end()) {
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

// Helper to convert alias name to hash (simple string hash)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0; // Hash 0 = "Any / Master"
  
  // Simple hash: use std::hash on the string
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

void InputProcessor::processEvent(InputID input, bool isDown) {
  if (!isDown) {
    // Key up - just handle note off
    voiceManager.handleKeyUp(input);
    return;
  }

  // Step A: Process Input Alias (Resolve hardware ID to Alias Name via DeviceManager)
  juce::String aliasName = deviceManager.getAliasForHardware(input.deviceHandle);
  
  // Convert alias name to hash for zone matching
  // Use 0 for "Unassigned" or empty (treat as "Any / Master")
  uintptr_t aliasHash = 0;
  if (aliasName != "Unassigned" && !aliasName.isEmpty()) {
    aliasHash = aliasNameToHash(aliasName);
  }

  // Step B: Zone Priority
  // Construct InputID with the Alias Hash instead of hardware ID
  InputID aliasInputID = {aliasHash, input.keyCode};
  auto zoneAction = zoneManager.handleInput(aliasInputID);
  
  if (zoneAction.has_value()) {
    // Zone matched - trigger note and return (skip manual mapping lookup)
    const auto &action = zoneAction.value();
    if (action.type == ActionType::Note) {
      voiceManager.noteOn(input, action.data1, action.data2, action.channel);
    }
    return;
  }

  // Step C: Fallback to manual mappings
  juce::ScopedReadLock lock(mapLock);
  const MidiAction *action = findMapping(input);
  if (action != nullptr) {
    if (action->type == ActionType::Note) {
      voiceManager.noteOn(input, action->data1, action->data2, action->channel);
    }
  }
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

  int ccValue = static_cast<int>(std::round(std::clamp(currentVal, 0.0f, 127.0f)));

  // Send MIDI CC message
  voiceManager.sendCC(action->channel, action->data1, ccValue);
}

bool InputProcessor::hasPointerMappings() {
  juce::ScopedReadLock lock(mapLock);
  for (const auto &pair : keyMapping) {
    int keyCode = pair.first.keyCode;
    if (keyCode == 0x2000 || keyCode == 0x2001) // PointerX or PointerY
      return true;
  }
  return false;
}