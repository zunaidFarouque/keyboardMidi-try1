#pragma once
#include <JuceHeader.h>

class PresetManager {
public:
  PresetManager();
  ~PresetManager();

  void saveToFile(juce::File file);
  void loadFromFile(juce::File file);

  // Returns the node containing the list of mappings
  juce::ValueTree getMappingsNode();

  // Returns the root for listeners
  juce::ValueTree &getRootNode() { return rootNode; }

private:
  juce::ValueTree rootNode{"OmniKeyPreset"};
};