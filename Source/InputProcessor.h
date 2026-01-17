#pragma once
#include "MappingTypes.h"
#include "PresetManager.h"
#include "VoiceManager.h"
#include <JuceHeader.h>

class InputProcessor : public juce::ValueTree::Listener {
public:
  InputProcessor(VoiceManager &voiceMgr, PresetManager &presetMgr);
  ~InputProcessor() override;

  // The main entry point for key events
  void processEvent(InputID input, bool isDown);

  // Handle continuous axis events (scroll, pointer X/Y)
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value);

  // Check if preset has pointer mappings (for smart cursor locking)
  bool hasPointerMappings();

  // NEW: Helper for the Logger to peek at what will happen
  const MidiAction *getMappingForInput(InputID input);

private:
  VoiceManager &voiceManager;
  PresetManager &presetManager;

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
};