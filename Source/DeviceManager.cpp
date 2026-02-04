#include "DeviceManager.h"
#include "PresetManager.h"
#include <algorithm>
#include <set>
#include <vector>
#include <windows.h>

namespace {
// Same criteria as RawInputManager: Precision Touchpad (HID Usage Page 0x0D, Usage 0x05).
// Used so we include touchpad devices in the live list and can re-assign Touchpad alias on startup.
bool isPrecisionTouchpadHandle(HANDLE deviceHandle) {
  if (deviceHandle == nullptr)
    return false;
  RID_DEVICE_INFO deviceInfo{};
  deviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
  UINT deviceInfoSize = deviceInfo.cbSize;
  if (GetRawInputDeviceInfo(deviceHandle, RIDI_DEVICEINFO, &deviceInfo,
                            &deviceInfoSize) == static_cast<UINT>(-1))
    return false;
  return deviceInfo.dwType == RIM_TYPEHID &&
         deviceInfo.hid.usUsagePage == 0x000D &&
         deviceInfo.hid.usUsage == 0x0005;
}
} // namespace

DeviceManager::DeviceManager() {
  loadConfig();
  rebuildAliasCache();
}

DeviceManager::~DeviceManager() { saveConfig(); }

void DeviceManager::createAlias(const juce::String &name) {
  if (name.isEmpty() || aliasExists(name))
    return;

  juce::ValueTree aliasNode("Alias");
  aliasNode.setProperty("name", name, nullptr);
  globalConfig.addChild(aliasNode, -1, nullptr);

  sendChangeMessage();
  saveConfig();
  rebuildAliasCache();
}

void DeviceManager::assignHardware(const juce::String &aliasName,
                                   uintptr_t hardwareId) {
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
  hardwareNode.setProperty(
      "id", juce::String::toHexString((juce::int64)hardwareId).toUpperCase(),
      nullptr);
  aliasNode.addChild(hardwareNode, -1, nullptr);

  sendChangeMessage();
  saveConfig();
}

void DeviceManager::removeHardware(const juce::String &aliasName,
                                   uintptr_t hardwareId) {
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

void DeviceManager::removeHardwareFromAlias(const juce::String &aliasName,
                                            uintptr_t hardwareId) {
  // Remove from ValueTree (and persist)
  removeHardware(aliasName, hardwareId);

  // Phase 46.2: refresh the live unassigned list immediately
  validateConnectedDevices();
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
      rebuildAliasCache();
      return;
    }
  }
}

// Helper to convert alias name to hash (same as in
// InputProcessor/ZonePropertiesPanel)
static uintptr_t getAliasHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

void DeviceManager::renameAlias(const juce::String &oldNameIn,
                                const juce::String &newNameIn,
                                PresetManager *presetManager) {
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
    std::vector<bool>
        updateInputAlias; // Track which ones need inputAlias update

    // Pass 1: Collect
    for (int i = 0; i < mappings.getNumChildren(); ++i) {
      auto mapping = mappings.getChild(i);

      // Check if this mapping uses the old alias name (new approach)
      juce::String inputAlias =
          mapping.getProperty("inputAlias", "").toString();
      bool needsInputAliasUpdate = (inputAlias == oldName);

      // Check if this mapping uses the old alias hash (legacy deviceHash
      // property)
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

    // Pass 2: Update (Listeners fire here, but we are no longer iterating the
    // parent tree)
    for (size_t i = 0; i < mappingsToUpdate.size(); ++i) {
      auto &mapping = mappingsToUpdate[i];
      if (updateInputAlias[i]) {
        mapping.setProperty("inputAlias", newName, nullptr);
      }
      // Also update deviceHash if it matched oldHash
      juce::String hashStr = mapping.getProperty("deviceHash").toString();
      if (!hashStr.isEmpty()) {
        uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();
        if (currentHash == oldHash) {
          mapping.setProperty(
              "deviceHash",
              juce::String::toHexString((juce::int64)newHash).toUpperCase(),
              nullptr);
        }
      }
    }
  }

  // 3. Apply Name Change
  aliasNode.setProperty("name", newName, nullptr);

  // Phase 46.3: keep reverse lookup cache in sync
  rebuildAliasCache();

  // 4. Notify & Save
  sendChangeMessage();
  saveConfig();
}

juce::Array<uintptr_t>
DeviceManager::getHardwareForAlias(const juce::String &aliasName) const {
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

juce::Array<juce::uint64>
DeviceManager::getAliasesForHardware(uintptr_t hardwareId) const {
  juce::Array<juce::uint64> result;

  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (!aliasNode.hasType("Alias"))
      continue;

    juce::String aliasName = aliasNode.getProperty("name").toString();
    juce::uint64 aliasHash = static_cast<juce::uint64>(getAliasHash(aliasName));

    for (int j = 0; j < aliasNode.getNumChildren(); ++j) {
      auto hardwareNode = aliasNode.getChild(j);
      if (!hardwareNode.hasType("Hardware"))
        continue;

      uintptr_t id = static_cast<uintptr_t>(
          hardwareNode.getProperty("id").toString().getHexValue64());
      if (id == hardwareId) {
        result.add(aliasHash);
        break; // this alias already recorded for this hardware
      }
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
  // Phase 46.3: robust reverse lookup (alias hash -> name)
  auto it = aliasNameCache.find(hardwareHash);
  if (it != aliasNameCache.end())
    return it->second;

  return "Unknown";
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

  rebuildAliasCache();
}

void DeviceManager::rebuildAliasCache() {
  aliasNameCache.clear();

  // Always add Global entry
  aliasNameCache[0] = "Global (All Devices)";

  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (!aliasNode.hasType("Alias"))
      continue;

    juce::String name = aliasNode.getProperty("name").toString();
    uintptr_t hash = getAliasHash(name);
    aliasNameCache[hash] = name;
  }
}

juce::ValueTree
DeviceManager::findOrCreateAliasNode(const juce::String &aliasName) {
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

juce::StringArray DeviceManager::getEmptyAliases(
    const std::vector<uintptr_t> &requiredAliasHashes) {
  juce::StringArray emptyAliases;

  // Iterate through required alias hashes
  for (uintptr_t requiredHash : requiredAliasHashes) {
    // Skip hash 0 (Global)
    if (requiredHash == 0)
      continue;

    // Find alias name by iterating all aliases and computing their hash
    juce::String aliasName;
    for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
      auto aliasNode = globalConfig.getChild(i);
      if (!aliasNode.hasType("Alias"))
        continue;

      juce::String name = aliasNode.getProperty("name").toString();
      uintptr_t hash = getAliasHash(name);

      if (hash == requiredHash) {
        aliasName = name;
        break;
      }
    }

    // If alias doesn't exist, create it
    if (aliasName.isEmpty()) {
      // Generate a default name from hash (fallback)
      aliasName =
          "Alias_" +
          juce::String::toHexString((juce::int64)requiredHash).substring(0, 8);
      createAlias(aliasName);
    }

    // Check if alias has hardware assigned
    juce::Array<uintptr_t> hardwareIds = getHardwareForAlias(aliasName);
    if (hardwareIds.size() == 0) {
      emptyAliases.add(aliasName);
    }
  }

  return emptyAliases;
}

void DeviceManager::validateConnectedDevices() {
  // Step 1: Get list of currently live devices using GetRawInputDeviceList
  UINT numDevices = 0;
  if (GetRawInputDeviceList(nullptr, &numDevices, sizeof(RAWINPUTDEVICELIST)) !=
      0) {
    // Error getting device count
    return;
  }

  if (numDevices == 0) {
    // No devices, but we can still clean up dead handles
    numDevices = 0;
  }

  std::vector<RAWINPUTDEVICELIST> deviceList(numDevices);
  if (GetRawInputDeviceList(deviceList.data(), &numDevices,
                            sizeof(RAWINPUTDEVICELIST)) ==
      static_cast<UINT>(-1)) {
    // Error getting device list
    return;
  }

  // Step 2: Extract valid hDevice handles into a set (keyboards + precision touchpads)
  std::set<uintptr_t> liveHandles;
  std::vector<uintptr_t> touchpadHandles;
  for (const auto &device : deviceList) {
    uintptr_t handle = reinterpret_cast<uintptr_t>(device.hDevice);
    if (device.dwType == RIM_TYPEKEYBOARD) {
      liveHandles.insert(handle);
    } else if (device.dwType == RIM_TYPEHID &&
               isPrecisionTouchpadHandle(device.hDevice)) {
      liveHandles.insert(handle);
      touchpadHandles.push_back(handle);
    }
  }

  // Phase 46: rebuild the unassigned device list
  unassignedDevices.clear();

  // Step 3: Iterate through all aliases and their hardware IDs, tracking which
  // live handles are already assigned
  bool changesMade = false;
  std::set<uintptr_t> assignedHandles;

  for (int i = 0; i < globalConfig.getNumChildren(); ++i) {
    auto aliasNode = globalConfig.getChild(i);
    if (!aliasNode.hasType("Alias"))
      continue;

    juce::String aliasName = aliasNode.getProperty("name").toString();

    // Step 4: Check each hardware ID in this alias
    for (int j = aliasNode.getNumChildren() - 1; j >= 0; --j) {
      auto hardwareNode = aliasNode.getChild(j);
      if (!hardwareNode.hasType("Hardware"))
        continue;

      juce::String idStr = hardwareNode.getProperty("id").toString();
      uintptr_t id = static_cast<uintptr_t>(idStr.getHexValue64());

      // Track assigned live devices (non-zero only)
      if (id != 0 && liveHandles.find(id) != liveHandles.end())
        assignedHandles.insert(id);

      // Remove if id != 0 AND id is NOT in liveHandles (dead device cleanup)
      if (id != 0 && liveHandles.find(id) == liveHandles.end()) {
        DBG("DeviceManager: Removed dead device " +
            juce::String::toHexString((juce::int64)id) + " from Alias \"" +
            aliasName + "\"");
        aliasNode.removeChild(hardwareNode, nullptr);
        changesMade = true;
      }
    }
  }

  // Step 5: Any live handle that is not in assignedHandles is considered
  // "unassigned". We do NOT write these into the ValueTree; we just track them
  // in-memory for the UI.
  for (auto handle : liveHandles) {
    if (assignedHandles.find(handle) == assignedHandles.end()) {
      unassignedDevices.push_back(handle);
    }
  }

  // Step 6: If "Touchpad" alias exists but has no hardware (e.g. after restart,
  // saved handle was invalid), and there is exactly one precision touchpad in
  // the unassigned list, assign it so touchpad mapping works without re-mapping.
  juce::String touchpadAliasName("Touchpad");
  if (aliasExists(touchpadAliasName)) {
    juce::Array<uintptr_t> touchpadHardware = getHardwareForAlias(touchpadAliasName);
    if (touchpadHardware.isEmpty()) {
      std::vector<uintptr_t> unassignedTouchpads;
      for (uintptr_t h : touchpadHandles) {
        if (assignedHandles.find(h) == assignedHandles.end())
          unassignedTouchpads.push_back(h);
      }
      if (unassignedTouchpads.size() == 1) {
        assignHardware(touchpadAliasName, unassignedTouchpads.front());
        changesMade = true;
        assignedHandles.insert(unassignedTouchpads.front());
        unassignedDevices.erase(
            std::remove(unassignedDevices.begin(), unassignedDevices.end(),
                        unassignedTouchpads.front()),
            unassignedDevices.end());
      }
    }
  }

  // Step 7: If changes were made, send change message and save
  // Always notify UI: the unassignedDevices list can change even if no dead
  // devices were removed.
  sendChangeMessage();

  if (changesMade)
    saveConfig();
}

juce::File DeviceManager::getConfigFile() const {
  auto dir =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("MIDIQy");
  dir.createDirectory();
  return dir.getChildFile("MIDIQyConfig.xml");
}
