#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <map>
#include <vector>

class PresetManager : public juce::ChangeBroadcaster {
public:
  struct KeyboardGroup {
    int id = 0;
    juce::String name;
  };

  PresetManager();
  ~PresetManager();

  void saveToFile(juce::File file);
  /// Save preset and include optional TouchpadData tree (from
  /// TouchpadLayoutManager::toValueTree()).
  void saveToFile(juce::File file, const juce::ValueTree &touchpadDataTree);
  /// Save preset and include optional TouchpadData and ZoneManager trees
  /// (from TouchpadLayoutManager::toValueTree() and ZoneManager::toValueTree()).
  void saveToFile(juce::File file, const juce::ValueTree &touchpadDataTree,
                  const juce::ValueTree &zoneManagerTree);
  void loadFromFile(juce::File file);

  /// After load, returns the TouchpadData child if present (for
  /// TouchpadLayoutManager::restoreFromValueTree).
  juce::ValueTree getTouchpadDataNode() const;
  /// After load, returns the ZoneManager child if present (for
  /// ZoneManager::restoreFromValueTree).
  juce::ValueTree getZoneManagerNode() const;

  // Phase 41: Static 9-layer system (0=Base, 1-8=Overlays). No add/remove.
  juce::ValueTree getLayersList(); // Returns "Layers" parent node
  juce::ValueTree getLayerNode(
      int layerIndex); // Find layer by id; invalid if missing/out of bounds
  juce::ValueTree
  getMappingsListForLayer(int layerIndex); // Get "Mappings" child of Layer
  std::vector<juce::ValueTree> getEnabledMappingsForLayer(int layerIndex) const;
  void
  ensureStaticLayers(); // Ensure Layers 0-8 exist (call after load/construct)

  // Legacy: Returns Layer 0's mappings for backward compatibility
  juce::ValueTree getMappingsNode();

  // Keyboard groups (for keyboard mappings and zones; 0 = no group).
  std::vector<KeyboardGroup> getKeyboardGroups() const;
  std::map<int, juce::String> getKeyboardGroupNames() const;
  void addKeyboardGroup(int id, const juce::String &name);
  void removeKeyboardGroup(int id); // Also clears keyboardGroupId on all mappings that had this group
  void renameKeyboardGroup(int id, const juce::String &name);

  // Returns the root for listeners
  juce::ValueTree &getRootNode() { return rootNode; }

  // Phase 41.1: Silent load - true during loadFromFile; listeners skip until
  // load done
  bool getIsLoading() const { return isLoading.load(); }

  // Phase 43.2: Transaction – bulk updates (e.g. StartupManager) silence
  // listeners until end
  void beginTransaction();
  void endTransaction();

private:
  juce::ValueTree rootNode{"MIDIQyPreset"};
  std::atomic<bool> isLoading{false};

  // Migration helper: Convert old flat structure to new hierarchy
  void migrateToLayerHierarchy();
};