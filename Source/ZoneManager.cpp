#include "ZoneManager.h"
#include "Zone.h"

ZoneManager::ZoneManager() {
}

ZoneManager::~ZoneManager() {
}

void ZoneManager::addZone(std::shared_ptr<Zone> zone) {
  juce::ScopedWriteLock lock(zoneLock);
  zones.push_back(zone);
  sendChangeMessage();
}

void ZoneManager::removeZone(std::shared_ptr<Zone> zone) {
  juce::ScopedWriteLock lock(zoneLock);
  zones.erase(std::remove(zones.begin(), zones.end(), zone), zones.end());
  sendChangeMessage();
}

std::shared_ptr<Zone> ZoneManager::createDefaultZone() {
  auto zone = std::make_shared<Zone>();
  zone->name = "New Zone";
  zone->targetAliasHash = 0; // "Any / Master"
  zone->rootNote = 60; // C4
  zone->scale = ScaleUtilities::ScaleType::Major;
  zone->chromaticOffset = 0;
  zone->degreeOffset = 0;
  zone->isTransposeLocked = false;
  
  juce::ScopedWriteLock lock(zoneLock);
  zones.push_back(zone);
  sendChangeMessage();
  
  return zone;
}

std::optional<MidiAction> ZoneManager::handleInput(InputID input) {
  juce::ScopedReadLock lock(zoneLock);

  // Iterate through zones and return the first valid result
  for (const auto &zone : zones) {
    auto action = zone->processKey(input, globalChromaticTranspose, globalDegreeTranspose);
    if (action.has_value()) {
      return action;
    }
  }

  return std::nullopt;
}

void ZoneManager::setGlobalTranspose(int chromatic, int degree) {
  juce::ScopedWriteLock lock(zoneLock);
  globalChromaticTranspose = chromatic;
  globalDegreeTranspose = degree;
  sendChangeMessage();
}

std::optional<MidiAction> ZoneManager::simulateInput(int keyCode, uintptr_t aliasHash) {
  juce::ScopedReadLock lock(zoneLock);

  // Create InputID from explicit arguments
  InputID input = {aliasHash, keyCode};

  // Iterate through zones and return the first valid result
  for (const auto &zone : zones) {
    auto action = zone->processKey(input, globalChromaticTranspose, globalDegreeTranspose);
    if (action.has_value()) {
      return action;
    }
  }

  return std::nullopt;
}
