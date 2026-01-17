#include "InputProcessor.h"

InputProcessor::InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr)
    : voiceManager(voiceMgr), presetManager(presetMgr) {
  // Add listener to the root node
  presetManager.getRootNode().addListener(this);

  // Populate map from existing tree
  rebuildMapFromTree();
}

InputProcessor::~InputProcessor() {
  // Remove listener
  presetManager.getRootNode().removeListener(this);
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
  uintptr_t deviceHash = parseDeviceHash(mappingNode.getProperty("deviceHash"));

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

  InputID inputId = {deviceHash, inputKey};
  MidiAction action = {actionType, channel, data1, data2};

  keyMapping[inputId] = action;
}

void InputProcessor::removeMappingFromTree(juce::ValueTree mappingNode) {
  if (!mappingNode.isValid())
    return;

  int inputKey = mappingNode.getProperty("inputKey", 0);
  uintptr_t deviceHash = parseDeviceHash(mappingNode.getProperty("deviceHash"));

  InputID inputId = {deviceHash, inputKey};
  keyMapping.erase(inputId);
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

void InputProcessor::processEvent(InputID input, bool isDown) {
  juce::ScopedReadLock lock(mapLock);

  if (isDown) {
    const MidiAction *action = findMapping(input);
    if (action != nullptr) {
      if (action->type == ActionType::Note) {
        voiceManager.noteOn(input, action->data1, action->data2,
                            action->channel);
      }
    }
  } else {
    voiceManager.handleKeyUp(input);
  }
}