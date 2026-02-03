#pragma once
#include "MappingTypes.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

class ScaleLibrary;

class ZoneManager : public juce::ChangeBroadcaster {
public:
  ZoneManager(ScaleLibrary &scaleLib);
  ~ZoneManager() override;

  // Add a zone to the collection
  void addZone(std::shared_ptr<Zone> zone);

  // Remove a zone from the collection
  void removeZone(std::shared_ptr<Zone> zone);

  // Create a default zone
  std::shared_ptr<Zone> createDefaultZone();

  // Get all zones (thread-safe read)
  std::vector<std::shared_ptr<Zone>> getZones() const {
    juce::ScopedReadLock lock(zoneLock);
    return zones;
  }

  // Handle input and return MIDI action if a zone matches
  std::optional<MidiAction> handleInput(InputID input, int layerIndex);

  // Handle input and return MIDI action with zone name if a zone matches
  std::pair<std::optional<MidiAction>, juce::String>
  handleInputWithName(InputID input, int layerIndex);

  // Get the zone that matches an input (for accessing zone properties like
  // chordType)
  std::shared_ptr<Zone> getZoneForInput(InputID input, int layerIndex);

  // Simulate input (for visualization) - takes explicit arguments
  std::optional<MidiAction> simulateInput(int keyCode, uintptr_t aliasHash,
                                          int layerIndex);

  // Get zone color for a specific key (for visualization)
  std::optional<juce::Colour>
  getZoneColorForKey(int keyCode, uintptr_t aliasHash, int layerIndex);

  // Number of zones that contain this key (for conflict detection in
  // visualizer)
  int getZoneCountForKey(int keyCode) const;

  // Number of zones that contain this key for a specific alias hash
  // (Phase 39.7)
  int getZoneCountForKey(int keyCode, uintptr_t aliasHash) const;

  // Set global transpose values
  void setGlobalTranspose(int chromatic, int degree);

  // Get global transpose values
  int getGlobalChromaticTranspose() const { return globalChromaticTranspose; }
  int getGlobalDegreeTranspose() const { return globalDegreeTranspose; }

  // Global scale and root (for zone inheritance)
  void setGlobalScale(juce::String name);
  void setGlobalRoot(int root);
  juce::String getGlobalScaleName() const { return globalScaleName; }
  int getGlobalRootNote() const { return globalRootNote; }
  // Intervals for global scale (for SmartScaleBend compilation)
  std::vector<int> getGlobalScaleIntervals() const;

  // Rebuild the lookup table (call when zones or their keys change)
  void rebuildLookupTable();

  // Serialization
  juce::ValueTree toValueTree() const;
  void restoreFromValueTree(const juce::ValueTree &vt);

private:
  void rebuildZoneCache(Zone *zone);

  ScaleLibrary &scaleLibrary;
  mutable juce::ReadWriteLock zoneLock;
  std::vector<std::shared_ptr<Zone>> zones;
  int globalChromaticTranspose = 0;
  int globalDegreeTranspose = 0;
  juce::String globalScaleName = "Major";
  int globalRootNote = 60;

  // Phase 49: lookup tables per layer (0..8)
  std::vector<std::unordered_map<InputID, Zone *>> layerLookupTables;
};
