#pragma once
#include <JuceHeader.h>

class PresetManager {
public:
  PresetManager();
  ~PresetManager();

  void saveToFile(juce::File file);
  void loadFromFile(juce::File file);

  // Phase 41: Layer hierarchy support
  juce::ValueTree getLayersList();  // Returns "Layers" parent node
  juce::ValueTree getLayerNode(int layerIndex);  // Get or create Layer node
  void addLayer(juce::String name);  // Add new layer (finds highest ID + 1)
  void removeLayer(int layerIndex);  // Remove layer (ignores if 0)
  juce::ValueTree getMappingsListForLayer(int layerIndex);  // Get "Mappings" child of Layer

  // Legacy: Returns Layer 0's mappings for backward compatibility
  juce::ValueTree getMappingsNode();

  // Returns the root for listeners
  juce::ValueTree &getRootNode() { return rootNode; }

private:
  juce::ValueTree rootNode{"OmniKeyPreset"};
  
  // Migration helper: Convert old flat structure to new hierarchy
  void migrateToLayerHierarchy();
  
  // Helper: Find highest layer ID
  int findHighestLayerId();
};