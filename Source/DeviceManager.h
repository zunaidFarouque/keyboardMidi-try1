#pragma once
#include <JuceHeader.h>
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

  // Delete an alias (removes the alias and all its hardware assignments)
  void deleteAlias(const juce::String &aliasName);

  // Get all hardware IDs for an alias
  juce::Array<uintptr_t> getHardwareForAlias(const juce::String &aliasName) const;

  // Get the alias name for a hardware ID (for UI display)
  juce::String getAliasForHardware(uintptr_t hardwareId) const;

  // Get all alias names
  juce::StringArray getAllAliases() const;

  // Get alias name for a hardware hash (for display)
  // Returns "Any / Master" for hash 0, "Unknown" if not found
  juce::String getAliasName(uintptr_t hardwareHash) const;

  // Get all alias names (alias for getAllAliases for consistency)
  juce::StringArray getAllAliasNames() const;

  // Check if alias exists
  bool aliasExists(const juce::String &aliasName) const;

  // Save/Load configuration
  void saveConfig();
  void loadConfig();

private:
  juce::ValueTree globalConfig{"OmniKeyConfig"};

  // Helper to find or create alias node
  juce::ValueTree findOrCreateAliasNode(const juce::String &aliasName);

  // Get config file path
  juce::File getConfigFile() const;
};
