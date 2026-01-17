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

std::pair<std::optional<MidiAction>, juce::String> ZoneManager::handleInputWithName(InputID input) {
  juce::ScopedReadLock lock(zoneLock);

  // Iterate through zones and return the first valid result with zone name
  for (const auto &zone : zones) {
    auto action = zone->processKey(input, globalChromaticTranspose, globalDegreeTranspose);
    if (action.has_value()) {
      return {action, zone->name};
    }
  }

  return {std::nullopt, ""};
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

juce::ValueTree ZoneManager::toValueTree() const {
  juce::ScopedReadLock lock(zoneLock);
  
  juce::ValueTree vt("ZoneManager");
  
  // Save global transpose
  vt.setProperty("globalChromaticTranspose", globalChromaticTranspose, nullptr);
  vt.setProperty("globalDegreeTranspose", globalDegreeTranspose, nullptr);
  
  // Save all zones as children
  for (const auto& zone : zones) {
    vt.addChild(zone->toValueTree(), -1, nullptr);
  }
  
  return vt;
}

void ZoneManager::restoreFromValueTree(const juce::ValueTree& vt) {
  if (!vt.isValid() || !vt.hasType("ZoneManager"))
    return;
  
  juce::ScopedWriteLock lock(zoneLock);
  
  // Clear existing zones
  zones.clear();
  
  // Restore global transpose
  globalChromaticTranspose = vt.getProperty("globalChromaticTranspose", 0);
  globalDegreeTranspose = vt.getProperty("globalDegreeTranspose", 0);
  
  // Restore zones
  for (int i = 0; i < vt.getNumChildren(); ++i) {
    auto zoneVT = vt.getChild(i);
    if (zoneVT.hasType("Zone")) {
      auto zone = Zone::fromValueTree(zoneVT);
      if (zone) {
        zones.push_back(zone);
      }
    }
  }
  
  sendChangeMessage();
}
