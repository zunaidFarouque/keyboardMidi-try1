#include "ZoneManager.h"
#include "Zone.h"
#include "ScaleLibrary.h"

ZoneManager::ZoneManager(ScaleLibrary& scaleLib) : scaleLibrary(scaleLib) {
}

ZoneManager::~ZoneManager() {
}

void ZoneManager::addZone(std::shared_ptr<Zone> zone) {
  // Auto-assign color if zone has default/transparent color
  if (zone->zoneColor == juce::Colours::transparentBlack || 
      zone->zoneColor.getAlpha() == 0) {
    static const juce::Colour colorPalette[] = {
      juce::Colour(0xff00CED1), // Teal
      juce::Colour(0xffFF8C00), // Orange
      juce::Colour(0xff9370DB), // Purple
      juce::Colour(0xff32CD32), // Green
      juce::Colour(0xffFF6347), // Red
      juce::Colour(0xff1E90FF), // Blue
      juce::Colour(0xffFFD700), // Gold
      juce::Colour(0xffFF69B4)  // Pink
    };
    
    int zoneIndex;
    {
      juce::ScopedReadLock readLock(zoneLock);
      zoneIndex = static_cast<int>(zones.size());
    }
    
    zone->zoneColor = colorPalette[zoneIndex % (sizeof(colorPalette) / sizeof(colorPalette[0]))];
  }
  
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
  zone->scaleName = "Major";
  zone->chromaticOffset = 0;
  zone->degreeOffset = 0;
  zone->isTransposeLocked = false;
  
  // Auto-assign color from palette
  static const juce::Colour colorPalette[] = {
    juce::Colour(0xff00CED1), // Teal
    juce::Colour(0xffFF8C00), // Orange
    juce::Colour(0xff9370DB), // Purple
    juce::Colour(0xff32CD32), // Green
    juce::Colour(0xffFF6347), // Red
    juce::Colour(0xff1E90FF), // Blue
    juce::Colour(0xffFFD700), // Gold
    juce::Colour(0xffFF69B4)  // Pink
  };
  
  int zoneIndex;
  {
    juce::ScopedReadLock readLock(zoneLock);
    zoneIndex = static_cast<int>(zones.size());
  }
  
  zone->zoneColor = colorPalette[zoneIndex % (sizeof(colorPalette) / sizeof(colorPalette[0]))];
  
  juce::ScopedWriteLock lock(zoneLock);
  zones.push_back(zone);
  sendChangeMessage();
  
  return zone;
}

std::optional<MidiAction> ZoneManager::handleInput(InputID input) {
  juce::ScopedReadLock lock(zoneLock);

  // Iterate through zones and return the first valid result
  for (const auto &zone : zones) {
    // Look up scale intervals from ScaleLibrary
    std::vector<int> intervals = scaleLibrary.getIntervals(zone->scaleName);
    auto action = zone->processKey(input, intervals, globalChromaticTranspose, globalDegreeTranspose);
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
    // Look up scale intervals from ScaleLibrary
    std::vector<int> intervals = scaleLibrary.getIntervals(zone->scaleName);
    auto action = zone->processKey(input, intervals, globalChromaticTranspose, globalDegreeTranspose);
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
    // Look up scale intervals from ScaleLibrary
    std::vector<int> intervals = scaleLibrary.getIntervals(zone->scaleName);
    auto action = zone->processKey(input, intervals, globalChromaticTranspose, globalDegreeTranspose);
    if (action.has_value()) {
      return action;
    }
  }

  return std::nullopt;
}

std::optional<juce::Colour> ZoneManager::getZoneColorForKey(int keyCode, uintptr_t aliasHash) {
  juce::ScopedReadLock lock(zoneLock);

  // Iterate through zones to find one that contains this key
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(), keyCode);
    if (it != zone->inputKeyCodes.end()) {
      // Found a zone containing this key
      if (zone->zoneColor != juce::Colours::transparentBlack && 
          zone->zoneColor.getAlpha() > 0) {
        return zone->zoneColor;
      }
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
