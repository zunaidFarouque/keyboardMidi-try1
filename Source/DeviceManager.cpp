#include "DeviceManager.h"
#include "PresetManager.h"
#include <vector>

DeviceManager::DeviceManager() {
  loadConfig();
}

DeviceManager::~DeviceManager() {
  saveConfig();
}

void DeviceManager::createAlias(const juce::String &name) {
  if (name.isEmpty() || aliasExists(name))
    return;

  juce::ValueTree aliasNode("Alias");
  aliasNode.setProperty("name", name, nullptr);
  globalConfig.addChild(aliasNode, -1, nullptr);

  sendChangeMessage();
  saveConfig();
}

void DeviceManager::assignHardware(const juce::String &aliasName, uintptr_t hardwareId) {
  if (!aliasExists(aliasName))
    createAlias(aliasName);

  juce::ValueTree aliasNode = findOrCreateAliasNode(aliasName);

  // Check if hardware already assigned
  for (int i = 0; i < aliasNode.getNumChildren(); ++i) {
    auto hardwareNode = aliasNode.getChild(i);
    if (hardwareNode.hasType("Hardware")) {
      uintptr_t existingId = static_cast<uintptr_t>(
          hardwareNode.getProperty("id").toString().getHexValue64());
      if (existingId == hardwareId)
        return; // Already assigned
    }
  }

  // Add new hardware node
  juce::ValueTree hardwareNode("Hardware");
  hardwareNode.setProperty("id", juce::String::toHexString((juce::int64)hardwareId).toUpperCase(), nullptr);
  aliasNode.addChild(hardwareNode, -1, nullptr);

  sendChangeMessage();
  saveConfig();
}

void DeviceManager::removeHardware(const juce::String &aliasName, uintptr_t hardwareId) {
  if (!aliasExists(aliasName))
    return;

  juce::ValueTree aliasNode = findOrCreateAliasNode(aliasName);

  for (int i = aliasNode.getNumChildren() - 1; i >= 0; --i) {
    auto hardwareNode = aliasNode.getChild(i);
    if (hardwareNode.hasType("Hardware")) {
      uintptr_t existingId = static_cast<uintptr_t>(
          hardwareNode.getProperty("id").toString().getHexValue64());
      if (existingId == hardwareId) {
        aliasNode.removeChild(hardwareNode, nullptr);
        sendChangeMessage();
        saveConfig();
        return;
      }
    }
  }
}

void DeviceManager::deleteAlias(const juce::String &aliasName) {
  if (!aliasExists(aliasName))
    return;

  // Find and remove the alias node
  for (int i = globalConfig.getNumChildren() - 1; i >= 0; --i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias") && 
        aliasNode.getProperty("name").toString() == aliasName) {
      globalConfig.removeChild(aliasNode, nullptr);
      sendChangeMessage();
      saveConfig();
      return;
    }
  }
}

// Helper to convert alias name to hash (same as in InputProcessor/ZonePropertiesPanel)
static uintptr_t getAliasHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

void DeviceManager::renameAlias(const juce::String &oldNameIn, const juce::String &newNameIn, PresetManager* presetManager) {
  // Deep copies
  const juce::String oldName = oldNameIn;
  const juce::String newName = newNameIn;
  
  if (oldName == newName || newName.isEmpty())
    return;
  
  if (!aliasExists(oldName))
    return;
  
  if (aliasExists(newName))
    return; // New name already exists
  
  // 1. Update the Alias Definition (Global Config)
  juce::ValueTree aliasNode;
  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto child = globalConfig.getChild(i);
    if (child.hasType("Alias") && 
        child.getProperty("name").toString() == oldName) {
      aliasNode = child;
      break;
    }
  }
  
  if (!aliasNode.isValid())
    return;
  
  // Calculate Hashes
  uintptr_t oldHash = getAliasHash(oldName);
  uintptr_t newHash = getAliasHash(newName);
  
  // 2. Update Mappings (Safely - Collect then Update pattern)
  if (presetManager) {
    auto mappings = presetManager->getMappingsNode();
    std::vector<juce::ValueTree> mappingsToUpdate;
    std::vector<bool> updateInputAlias; // Track which ones need inputAlias update
    
    // Pass 1: Collect
    for (int i = 0; i < mappings.getNumChildren(); ++i) {
      auto mapping = mappings.getChild(i);
      
      // Check if this mapping uses the old alias name (new approach)
      juce::String inputAlias = mapping.getProperty("inputAlias", "").toString();
      bool needsInputAliasUpdate = (inputAlias == oldName);
      
      // Check if this mapping uses the old alias hash (legacy deviceHash property)
      juce::String hashStr = mapping.getProperty("deviceHash").toString();
      bool needsHashUpdate = false;
      if (!hashStr.isEmpty()) {
        uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();
        needsHashUpdate = (currentHash == oldHash);
      }
      
      if (needsInputAliasUpdate || needsHashUpdate) {
        mappingsToUpdate.push_back(mapping);
        updateInputAlias.push_back(needsInputAliasUpdate);
      }
    }
    
    // Pass 2: Update (Listeners fire here, but we are no longer iterating the parent tree)
    for (size_t i = 0; i < mappingsToUpdate.size(); ++i) {
      auto& mapping = mappingsToUpdate[i];
      if (updateInputAlias[i]) {
        mapping.setProperty("inputAlias", newName, nullptr);
      }
      // Also update deviceHash if it matched oldHash
      juce::String hashStr = mapping.getProperty("deviceHash").toString();
      if (!hashStr.isEmpty()) {
        uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();
        if (currentHash == oldHash) {
          mapping.setProperty("deviceHash", juce::String::toHexString((juce::int64)newHash).toUpperCase(), nullptr);
        }
      }
    }
  }
  
  // 3. Apply Name Change
  aliasNode.setProperty("name", newName, nullptr);
  
  // 4. Notify & Save
  sendChangeMessage();
  saveConfig();
}

juce::Array<uintptr_t> DeviceManager::getHardwareForAlias(const juce::String &aliasName) const {
  juce::Array<uintptr_t> result;

  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias") && 
        aliasNode.getProperty("name").toString() == aliasName) {
      // Found the alias, collect all hardware IDs
      for (int j = 0; j < aliasNode.getNumChildren(); ++j) {
        auto hardwareNode = aliasNode.getChild(j);
        if (hardwareNode.hasType("Hardware")) {
          uintptr_t id = static_cast<uintptr_t>(
              hardwareNode.getProperty("id").toString().getHexValue64());
          result.add(id);
        }
      }
      break;
    }
  }

  return result;
}

juce::String DeviceManager::getAliasForHardware(uintptr_t hardwareId) const {
  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias")) {
      for (int j = 0; j < aliasNode.getNumChildren(); ++j) {
        auto hardwareNode = aliasNode.getChild(j);
        if (hardwareNode.hasType("Hardware")) {
          uintptr_t id = static_cast<uintptr_t>(
              hardwareNode.getProperty("id").toString().getHexValue64());
          if (id == hardwareId) {
            return aliasNode.getProperty("name").toString();
          }
        }
      }
    }
  }

  return "Unassigned";
}

juce::StringArray DeviceManager::getAllAliases() const {
  juce::StringArray result;

  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias")) {
      result.add(aliasNode.getProperty("name").toString());
    }
  }

  return result;
}

juce::String DeviceManager::getAliasName(uintptr_t hardwareHash) const {
  // Edge case: hash is 0 means "Any / Master"
  if (hardwareHash == 0)
    return "Any / Master";

  // Look up which alias this hardware ID belongs to
  juce::String alias = getAliasForHardware(hardwareHash);
  
  if (alias == "Unassigned")
    return "Unknown";
  
  return alias;
}

juce::StringArray DeviceManager::getAllAliasNames() const {
  // Alias for getAllAliases for consistency
  return getAllAliases();
}

bool DeviceManager::aliasExists(const juce::String &aliasName) const {
  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias") && 
        aliasNode.getProperty("name").toString() == aliasName) {
      return true;
    }
  }
  return false;
}

void DeviceManager::saveConfig() {
  auto file = getConfigFile();
  if (auto xml = globalConfig.createXml()) {
    xml->writeTo(file);
  }
}

void DeviceManager::loadConfig() {
  auto file = getConfigFile();
  if (file.existsAsFile()) {
    if (auto xml = juce::parseXML(file)) {
      globalConfig = juce::ValueTree::fromXml(*xml);
      if (!globalConfig.isValid()) {
        globalConfig = juce::ValueTree("OmniKeyConfig");
      }
    }
  } else {
    globalConfig = juce::ValueTree("OmniKeyConfig");
  }
}

juce::ValueTree DeviceManager::findOrCreateAliasNode(const juce::String &aliasName) {
  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (aliasNode.hasType("Alias") && 
        aliasNode.getProperty("name").toString() == aliasName) {
      return aliasNode;
    }
  }

  // Create new alias node
  juce::ValueTree aliasNode("Alias");
  aliasNode.setProperty("name", aliasName, nullptr);
  globalConfig.addChild(aliasNode, -1, nullptr);
  return aliasNode;
}

juce::File DeviceManager::getConfigFile() const {
  auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                 .getChildFile("OmniKey");
  dir.createDirectory();
  return dir.getChildFile("OmniKeyConfig.xml");
}
