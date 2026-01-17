#pragma once
#include "MappingTypes.h"
#include "PresetManager.h"
#include "VoiceManager.h"
#include "DeviceManager.h"
#include "ZoneManager.h"
#include <JuceHeader.h>

class ScaleLibrary;

class InputProcessor : public juce::ValueTree::Listener, public juce::ChangeListener {
public:
  InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr, DeviceManager &deviceMgr, ScaleLibrary &scaleLib);
  ~InputProcessor() override;

  // The main entry point for key events
  void processEvent(InputID input, bool isDown);

  // Handle continuous axis events (scroll, pointer X/Y)
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value);

  // Check if preset has pointer mappings (for smart cursor locking)
  bool hasPointerMappings();

  // NEW: Helper for the Logger to peek at what will happen
  const MidiAction *getMappingForInput(InputID input);

  // Simulate input and return action with source description
  std::pair<std::optional<MidiAction>, juce::String> simulateInput(uintptr_t deviceHandle, int keyCode);

  // Zone management
  ZoneManager &getZoneManager() { return zoneManager; }

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  VoiceManager &voiceManager;
  PresetManager &presetManager;
  DeviceManager &deviceManager;
  ZoneManager zoneManager;

  // Thread Safety
  juce::ReadWriteLock mapLock;

  // The Fast Lookup Cache
  std::unordered_map<InputID, MidiAction> keyMapping;

  // Current CC values for relative inputs (scroll)
  std::unordered_map<InputID, float> currentCCValues;

  // ValueTree Callbacks
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &child) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &child, int) override;
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

  // Helpers
  void rebuildMapFromTree();
  void addMappingFromTree(juce::ValueTree mappingNode);
  void removeMappingFromTree(juce::ValueTree mappingNode);
  const MidiAction *findMapping(const InputID &input);
  
  // Shared lookup logic (used by both processEvent and simulateInput)
  std::pair<std::optional<MidiAction>, juce::String> lookupAction(uintptr_t deviceHandle, int keyCode);
};