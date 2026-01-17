#include "PresetManager.h"

PresetManager::PresetManager() {
  // Create initial structure if missing
  if (!rootNode.getChildWithName("Mappings").isValid()) {
    rootNode.addChild(juce::ValueTree("Mappings"), -1, nullptr);
  }
}

PresetManager::~PresetManager() {}

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
      // 1. Get the new Mappings node from the file
      auto newMappings = newTree.getChildWithName("Mappings");

      // 2. Get the old Mappings node
      auto oldMappings = rootNode.getChildWithName("Mappings");

      // 3. Swap them (Atomic-ish operation)
      // If the file has mappings, we replace our current ones
      if (newMappings.isValid()) {
        if (oldMappings.isValid()) {
          // If we have an old node, remove it
          rootNode.removeChild(oldMappings, nullptr);
        }

        // Add the new one (Listener will catch this and rebuild map)
        rootNode.addChild(newMappings.createCopy(), -1, nullptr);
      }
    }
  }
}

juce::ValueTree PresetManager::getMappingsNode() {
  // FIX: Just return the node. Do NOT add it if missing.
  // This prevents the infinite loop.
  return rootNode.getChildWithName("Mappings");
}