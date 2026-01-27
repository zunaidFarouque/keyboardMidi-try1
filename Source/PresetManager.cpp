#include "PresetManager.h"

PresetManager::PresetManager() {
  // Phase 41: Create layer hierarchy if missing
  auto layersList = rootNode.getChildWithName("Layers");
  if (!layersList.isValid()) {
    layersList = juce::ValueTree("Layers");
    rootNode.addChild(layersList, -1, nullptr);
  }
  
  // Ensure Layer 0 (Base) exists
  auto layer0 = getLayerNode(0);
  if (!layer0.getChildWithName("Mappings").isValid()) {
    layer0.addChild(juce::ValueTree("Mappings"), -1, nullptr);
  }
  
  // Migration: If old flat "Mappings" exists, migrate to Layer 0
  migrateToLayerHierarchy();
}

PresetManager::~PresetManager() {}

void PresetManager::migrateToLayerHierarchy() {
  auto oldMappings = rootNode.getChildWithName("Mappings");
  if (oldMappings.isValid() && oldMappings.getNumChildren() > 0) {
    // Old structure exists, migrate to Layer 0
    auto layer0 = getLayerNode(0);
    auto layer0Mappings = layer0.getChildWithName("Mappings");
    
    if (!layer0Mappings.isValid()) {
      layer0Mappings = juce::ValueTree("Mappings");
      layer0.addChild(layer0Mappings, -1, nullptr);
    }
    
    // Copy all mappings to Layer 0
    for (int i = 0; i < oldMappings.getNumChildren(); ++i) {
      auto mapping = oldMappings.getChild(i);
      if (mapping.isValid()) {
        // Set layerID to 0 if not already set
        if (!mapping.hasProperty("layerID")) {
          mapping.setProperty("layerID", 0, nullptr);
        }
        layer0Mappings.addChild(mapping.createCopy(), -1, nullptr);
      }
    }
    
    // Remove old flat structure
    rootNode.removeChild(oldMappings, nullptr);
  }
}

void PresetManager::saveToFile(juce::File file) {
  auto xml = rootNode.createXml();
  if (xml) {
    xml->writeTo(file);
  }
}

void PresetManager::loadFromFile(juce::File file) {
  auto xml = juce::XmlDocument::parse(file);
  if (xml != nullptr) {
    auto newTree = juce::ValueTree::fromXml(*xml);
    if (newTree.isValid() && newTree.hasType(rootNode.getType())) {
      // Phase 41: Replace entire root (preserves layer hierarchy)
      rootNode = newTree.createCopy();
      
      // Ensure structure is valid after load
      auto layersList = getLayersList();
      if (!layersList.isValid()) {
        layersList = juce::ValueTree("Layers");
        rootNode.addChild(layersList, -1, nullptr);
      }
      
      // Ensure Layer 0 exists
      auto layer0 = getLayerNode(0);
      if (!layer0.getChildWithName("Mappings").isValid()) {
        layer0.addChild(juce::ValueTree("Mappings"), -1, nullptr);
      }
      
      // Migrate if old structure exists
      migrateToLayerHierarchy();
    }
  }
}

juce::ValueTree PresetManager::getLayersList() {
  auto layersList = rootNode.getChildWithName("Layers");
  if (!layersList.isValid()) {
    layersList = juce::ValueTree("Layers");
    rootNode.addChild(layersList, -1, nullptr);
  }
  return layersList;
}

juce::ValueTree PresetManager::getLayerNode(int layerIndex) {
  auto layersList = getLayersList();
  
  // Find existing layer
  for (int i = 0; i < layersList.getNumChildren(); ++i) {
    auto layer = layersList.getChild(i);
    if (layer.isValid()) {
      int id = static_cast<int>(layer.getProperty("id", -1));
      if (id == layerIndex) {
        return layer;
      }
    }
  }
  
  // Not found, create it
  juce::ValueTree layer("Layer");
  layer.setProperty("id", layerIndex, nullptr);
  layer.setProperty("name", layerIndex == 0 ? "Base" : "Layer " + juce::String(layerIndex), nullptr);
  
  // Add Mappings child
  juce::ValueTree mappings("Mappings");
  layer.addChild(mappings, -1, nullptr);
  
  layersList.addChild(layer, -1, nullptr);
  return layer;
}

void PresetManager::addLayer(juce::String name) {
  int newId = findHighestLayerId() + 1;
  juce::ValueTree layer("Layer");
  layer.setProperty("id", newId, nullptr);
  layer.setProperty("name", name.isEmpty() ? "Layer " + juce::String(newId) : name, nullptr);
  
  juce::ValueTree mappings("Mappings");
  layer.addChild(mappings, -1, nullptr);
  
  auto layersList = getLayersList();
  layersList.addChild(layer, -1, nullptr);
}

void PresetManager::removeLayer(int layerIndex) {
  if (layerIndex == 0)
    return;  // Cannot remove Base layer
  
  auto layersList = getLayersList();
  for (int i = 0; i < layersList.getNumChildren(); ++i) {
    auto layer = layersList.getChild(i);
    if (layer.isValid()) {
      int id = static_cast<int>(layer.getProperty("id", -1));
      if (id == layerIndex) {
        layersList.removeChild(layer, nullptr);
        return;
      }
    }
  }
}

juce::ValueTree PresetManager::getMappingsListForLayer(int layerIndex) {
  auto layer = getLayerNode(layerIndex);
  auto mappings = layer.getChildWithName("Mappings");
  if (!mappings.isValid()) {
    mappings = juce::ValueTree("Mappings");
    layer.addChild(mappings, -1, nullptr);
  }
  return mappings;
}

int PresetManager::findHighestLayerId() {
  int highest = -1;
  auto layersList = getLayersList();
  for (int i = 0; i < layersList.getNumChildren(); ++i) {
    auto layer = layersList.getChild(i);
    if (layer.isValid()) {
      int id = layer.getProperty("id", -1);
      if (id > highest)
        highest = id;
    }
  }
  return highest;
}

juce::ValueTree PresetManager::getMappingsNode() {
  // Legacy: Return Layer 0's mappings for backward compatibility
  return getMappingsListForLayer(0);
}