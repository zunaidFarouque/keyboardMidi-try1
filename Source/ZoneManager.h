#pragma once
#include "Zone.h"
#include "MappingTypes.h"
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <optional>

class ZoneManager : public juce::ChangeBroadcaster {
public:
  ZoneManager();
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
  std::optional<MidiAction> handleInput(InputID input);

  // Simulate input (for visualization) - takes explicit arguments
  std::optional<MidiAction> simulateInput(int keyCode, uintptr_t aliasHash);

  // Set global transpose values
  void setGlobalTranspose(int chromatic, int degree);

  // Get global transpose values
  int getGlobalChromaticTranspose() const { return globalChromaticTranspose; }
  int getGlobalDegreeTranspose() const { return globalDegreeTranspose; }

private:
  mutable juce::ReadWriteLock zoneLock;
  std::vector<std::shared_ptr<Zone>> zones;
  int globalChromaticTranspose = 0;
  int globalDegreeTranspose = 0;
};
