#include "InputProcessor.h"
#include "ScaleLibrary.h"
#include "ChordUtilities.h"
#include "MappingTypes.h"

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr, DeviceManager &deviceMgr, ScaleLibrary &scaleLib)
    : voiceManager(voiceMgr), presetManager(presetMgr), deviceManager(deviceMgr), zoneManager(scaleLib), scaleLibrary(scaleLib) {
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
    else if (t == "Command")
      actionType = ActionType::Command;
    else if (t == "Macro")
      actionType = ActionType::Macro;
  } else if (typeVar.isInt()) {
    int t = static_cast<int>(typeVar);
    if (t == 1)
      actionType = ActionType::CC;
    else if (t == 2)
      actionType = ActionType::Macro;
    else if (t == 3)
      actionType = ActionType::Command;
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
    // When a mapping property changes (e.g., inputKey/keyCode), we need to rebuild
    // the entire map because we can't know the old value to remove the old entries.
    // This is especially important for keyCode changes (via "learn" feature).
    juce::ScopedWriteLock lock(mapLock);
    rebuildMapFromTree();
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

// Shared lookup logic for processEvent and simulateInput
// Returns: {action, sourceDescription}
std::pair<std::optional<MidiAction>, juce::String> InputProcessor::lookupAction(uintptr_t deviceHandle, int keyCode) {
  // Step 1: Get alias name for hardware ID
  juce::String aliasName = deviceManager.getAliasForHardware(deviceHandle);
  
  // Convert alias name to hash
  uintptr_t aliasHash = 0;
  if (aliasName != "Unassigned" && !aliasName.isEmpty()) {
    aliasHash = aliasNameToHash(aliasName);
  }
  
  // Step 2: Check specific alias zones first
  if (aliasHash != 0) {
    InputID aliasInputID = {aliasHash, keyCode};
    auto [zoneAction, zoneName] = zoneManager.handleInputWithName(aliasInputID);
    if (zoneAction.has_value()) {
      return {zoneAction, "Zone: " + zoneName};
    }
  }
  
  // Step 3: Check wildcard zone (hash 0 = "Any / Master")
  InputID wildcardInputID = {0, keyCode};
  auto [zoneAction, zoneName] = zoneManager.handleInputWithName(wildcardInputID);
  if (zoneAction.has_value()) {
    return {zoneAction, "Zone: " + zoneName};
  }
  
  // Step 4: Check manual mappings (specific alias first)
  InputID input = {deviceHandle, keyCode};
  juce::ScopedReadLock lock(mapLock);
  const MidiAction *action = findMapping(input);
  if (action != nullptr) {
    return {*action, "Mapping"};
  }
  
  // Step 5: Check manual mappings (wildcard)
  if (deviceHandle != 0) {
    InputID anyDevice = {0, keyCode};
    action = findMapping(anyDevice);
    if (action != nullptr) {
      return {*action, "Mapping"};
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
  juce::String aliasName = deviceManager.getAliasForHardware(input.deviceHandle);
  uintptr_t aliasHash = (aliasName != "Unassigned" && !aliasName.isEmpty())
      ? aliasNameToHash(aliasName) : 0;
  if (aliasHash != 0) {
    auto z = zoneManager.getZoneForInput(InputID{aliasHash, input.keyCode});
    if (z) return z;
  }
  return zoneManager.getZoneForInput(InputID{0, input.keyCode});
}

void InputProcessor::processEvent(InputID input, bool isDown) {
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
    if (action.has_value() && source.startsWith("Zone: ")) {
      auto zone = getZoneForInputResolved(input);
      if (zone && zone->playMode == Zone::PlayMode::Strum) {
        juce::ScopedWriteLock lock(bufferLock);
        noteBuffer.clear();
        bufferedStrumSpeedMs = 50;
        
        // Use zone's release behavior
        bool shouldSustain = (zone->releaseBehavior == Zone::ReleaseBehavior::Sustain);
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
      return;
    }

    if (midiAction.type == ActionType::Note) {
      if (source.startsWith("Zone: ")) {
        auto zone = getZoneForInputResolved(input);
        if (zone) {
          auto chordNotes = zone->getNotesForKey(input.keyCode, 
                                                  zoneManager.getGlobalChromaticTranspose(),
                                                  zoneManager.getGlobalDegreeTranspose());
          bool allowSustain = zone->allowSustain;

          if (chordNotes.has_value() && !chordNotes->empty()) {
            // Calculate per-note velocities with ghost note scaling
            int mainVelocity = calculateVelocity(zone->baseVelocity, zone->velocityRandom);
            
            std::vector<int> finalNotes;
            std::vector<int> finalVelocities;
            finalNotes.reserve(chordNotes->size());
            finalVelocities.reserve(chordNotes->size());
            
            for (const auto& cn : *chordNotes) {
              finalNotes.push_back(cn.pitch);
              if (cn.isGhost) {
                // Ghost notes use scaled velocity
                int ghostVel = static_cast<int>(mainVelocity * zone->ghostVelocityScale);
                finalVelocities.push_back(juce::jlimit(1, 127, ghostVel));
              } else {
                finalVelocities.push_back(mainVelocity);
              }
            }
            
            if (zone->playMode == Zone::PlayMode::Direct) {
              if (finalNotes.size() > 1) {
                voiceManager.noteOn(input, finalNotes, finalVelocities, midiAction.channel, zone->strumSpeedMs, allowSustain);
              } else {
                voiceManager.noteOn(input, finalNotes.front(), finalVelocities.front(), midiAction.channel, allowSustain);
              }
            } else if (zone->playMode == Zone::PlayMode::Strum) {
              int strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50;
              voiceManager.handleKeyUp(lastStrumSource);
              voiceManager.noteOn(input, finalNotes, finalVelocities, midiAction.channel, strumMs, allowSustain);
              lastStrumSource = input;
              {
                juce::ScopedWriteLock bufferWriteLock(bufferLock);
                // Store pitches only for visualizer (backward compatibility)
                noteBuffer = finalNotes;
                bufferedStrumSpeedMs = strumMs;
              }
            }
          } else {
            // Fallback: use mapping velocity with randomization
            int vel = calculateVelocity(midiAction.data2, midiAction.velocityRandom);
            voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel, allowSustain);
          }
        } else {
          // Fallback: use mapping velocity with randomization
          int vel = calculateVelocity(midiAction.data2, midiAction.velocityRandom);
          voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel, true);
        }
      } else {
        // Manual mapping: use mapping velocity with randomization
        int vel = calculateVelocity(midiAction.data2, midiAction.velocityRandom);
        voiceManager.noteOn(input, midiAction.data1, vel, midiAction.channel, true);
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
  for (const auto& p : keyMapping) {
    if (p.first.keyCode == keyCode)
      return true;
  }
  return false;
}

void InputProcessor::forceRebuildMappings() {
  juce::ScopedWriteLock lock(mapLock);
  rebuildMapFromTree();
}

std::pair<std::optional<MidiAction>, juce::String> InputProcessor::simulateInput(uintptr_t deviceHandle, int keyCode) {
  return lookupAction(deviceHandle, keyCode);
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