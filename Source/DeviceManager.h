#pragma once
#include <JuceHeader.h>
#include <map>
#include <vector>

class DeviceManager : public juce::ChangeBroadcaster {
public:
  DeviceManager();
  ~DeviceManager() override;

  // Create a new alias
  void createAlias(const juce::String &name);

  // Assign a hardware ID to an alias
  void assignHardware(const juce::String &aliasName, uintptr_t hardwareId);

  // Remove a hardware ID from an alias
  void removeHardware(const juce::String &aliasName, uintptr_t hardwareId);

  // Phase 46.2: explicit remove helper that also refreshes unassigned list
  void removeHardwareFromAlias(const juce::String &aliasName,
                               uintptr_t hardwareId);

  // Delete an alias (removes the alias and all its hardware assignments)
  void deleteAlias(const juce::String &aliasName);

  // Rename an alias
  void renameAlias(const juce::String &oldName, const juce::String &newName,
                   class PresetManager *presetManager = nullptr);

  // Get all hardware IDs for an alias
  juce::Array<uintptr_t>
  getHardwareForAlias(const juce::String &aliasName) const;

  // Phase 45.7: get all alias hashes that reference a given hardware ID
  juce::Array<juce::uint64> getAliasesForHardware(uintptr_t hardwareId) const;

  // Get the alias name for a hardware ID (for UI display)
  juce::String getAliasForHardware(uintptr_t hardwareId) const;

  // Get all alias names
  juce::StringArray getAllAliases() const;

  // Get alias name for a hardware hash (for display)
  // Returns "Global" for hash 0, "Unknown" if not found
  juce::String getAliasName(uintptr_t hardwareHash) const;

  // Get all alias names (alias for getAllAliases for consistency)
  juce::StringArray getAllAliasNames() const;

  // Check if alias exists
  bool aliasExists(const juce::String &aliasName) const;

  // Validate connected devices and remove dead handles
  void validateConnectedDevices();

  // Phase 46: get list of currently connected but unassigned devices
  const std::vector<uintptr_t> &getUnassignedDevices() const {
    return unassignedDevices;
  }

  // Phase 9.6: Get aliases that have no hardware assigned
  juce::StringArray
  getEmptyAliases(const std::vector<uintptr_t> &requiredAliasHashes);

  // Save/Load configuration
  void saveConfig();
  void loadConfig();

  // Get portable data directory (next to executable)
  static juce::File getPortableDataDirectory();

private:
  juce::ValueTree globalConfig{"OmniKeyConfig"};

  // Phase 46: live devices that are not assigned to any alias
  std::vector<uintptr_t> unassignedDevices;

  // Phase 46.3: reliable reverse lookup (alias hash -> name)
  std::map<uintptr_t, juce::String> aliasNameCache;
  void rebuildAliasCache();

  // Helper to find or create alias node
  juce::ValueTree findOrCreateAliasNode(const juce::String &aliasName);

  // Get config file path
  juce::File getConfigFile() const;
};
