#include "ZoneManager.h"
#include "ScaleLibrary.h"
#include "Zone.h"

ZoneManager::ZoneManager(ScaleLibrary &scaleLib) : scaleLibrary(scaleLib) {
  layerLookupTables.resize(9);
}

ZoneManager::~ZoneManager() {}

std::vector<int> ZoneManager::getGlobalScaleIntervals() const {
  return scaleLibrary.getIntervals(globalScaleName);
}

void ZoneManager::rebuildZoneCache(Zone *zone) {
  std::vector<int> intervals = zone->usesGlobalScale()
                                   ? scaleLibrary.getIntervals(globalScaleName)
                                   : scaleLibrary.getIntervals(zone->scaleName);
  int root = zone->usesGlobalRoot()
                 ? (globalRootNote + 12 * zone->globalRootOctaveOffset)
                 : zone->rootNote;
  zone->rebuildCache(intervals, root);
}

void ZoneManager::rebuildLookupTable() {
  juce::ScopedWriteLock lock(zoneLock);

  // Clear existing lookup tables
  for (auto &m : layerLookupTables)
    m.clear();

  // Build lookup table from all zones
  for (const auto &zone : zones) {
    int layerId = juce::jlimit(0, 8, zone->layerID);
    const auto &keyCodes = zone->getInputKeyCodes();
    for (int keyCode : keyCodes) {
      InputID id{zone->targetAliasHash, keyCode};
      // Overwrites previous entries, giving priority to later zones
      layerLookupTables[(size_t)layerId][id] = zone;
    }
  }
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

    zone->zoneColor = colorPalette[zoneIndex % (sizeof(colorPalette) /
                                                sizeof(colorPalette[0]))];
  }

  // Rebuild cache for the zone (use global scale/root if zone flags set)
  rebuildZoneCache(zone.get());

  juce::ScopedWriteLock lock(zoneLock);
  zones.push_back(zone);
  rebuildLookupTable(); // Rebuild lookup table after adding zone
  sendChangeMessage();
}

void ZoneManager::removeZone(std::shared_ptr<Zone> zone) {
  juce::ScopedWriteLock lock(zoneLock);
  zones.erase(std::remove(zones.begin(), zones.end(), zone), zones.end());
  rebuildLookupTable(); // Rebuild lookup table after removing zone
  sendChangeMessage();
}

std::shared_ptr<Zone> ZoneManager::createDefaultZone() {
  auto zone = std::make_shared<Zone>();
  zone->name = "New Zone";
  zone->targetAliasHash = 0; // Global (All Devices)
  zone->rootNote = 60;       // C4
  zone->scaleName = "Major";
  zone->chromaticOffset = 0;
  zone->degreeOffset = 0;
  zone->ignoreGlobalTranspose = false;

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

  zone->zoneColor = colorPalette[zoneIndex % (sizeof(colorPalette) /
                                              sizeof(colorPalette[0]))];

  // Rebuild cache for new zone (use global scale/root if zone flags set)
  rebuildZoneCache(zone.get());

  juce::ScopedWriteLock lock(zoneLock);
  zones.push_back(zone);
  rebuildLookupTable(); // Rebuild lookup table after adding zone
  sendChangeMessage();

  return zone;
}

std::optional<MidiAction> ZoneManager::handleInput(InputID input,
                                                   int layerIndex) {
  juce::ScopedReadLock lock(zoneLock);

  if (layerIndex < 0 || layerIndex >= (int)layerLookupTables.size())
    return std::nullopt;

  // Use lookup table for instant access
  auto &table = layerLookupTables[(size_t)layerIndex];
  auto it = table.find(input);
  if (it == table.end())
    return std::nullopt;

  return it->second->processKey(input, globalChromaticTranspose,
                                globalDegreeTranspose);
}

std::pair<std::optional<MidiAction>, juce::String>
ZoneManager::handleInputWithName(InputID input, int layerIndex) {
  juce::ScopedReadLock lock(zoneLock);

  if (layerIndex < 0 || layerIndex >= (int)layerLookupTables.size())
    return {std::nullopt, ""};

  auto &table = layerLookupTables[(size_t)layerIndex];
  auto it = table.find(input);
  if (it == table.end()) {
    return {std::nullopt, ""};
  }

  auto zone = it->second;
  auto action =
      zone->processKey(input, globalChromaticTranspose, globalDegreeTranspose);
  if (action.has_value()) {
    return {action, zone->name};
  }

  return {std::nullopt, ""};
}

void ZoneManager::setGlobalTranspose(int chromatic, int degree) {
  juce::ScopedWriteLock lock(zoneLock);
  globalChromaticTranspose = chromatic;
  globalDegreeTranspose = degree;
  sendChangeMessage();
}

void ZoneManager::setGlobalScale(juce::String name) {
  juce::ScopedWriteLock lock(zoneLock);
  globalScaleName = name;
  for (const auto &zone : zones) {
    if (zone->usesGlobalScale())
      rebuildZoneCache(zone.get());
  }
  rebuildLookupTable();
  sendChangeMessage();
}

void ZoneManager::setGlobalRoot(int root) {
  juce::ScopedWriteLock lock(zoneLock);
  globalRootNote = root;
  for (const auto &zone : zones) {
    if (zone->usesGlobalRoot())
      rebuildZoneCache(zone.get());
  }
  rebuildLookupTable();
  sendChangeMessage();
}

std::optional<MidiAction>
ZoneManager::simulateInput(int keyCode, uintptr_t aliasHash, int layerIndex) {
  juce::ScopedReadLock lock(zoneLock);

  // Create InputID from explicit arguments
  InputID input = {aliasHash, keyCode};

  if (layerIndex < 0 || layerIndex >= (int)layerLookupTables.size())
    return std::nullopt;

  auto &table = layerLookupTables[(size_t)layerIndex];
  auto it = table.find(input);
  if (it == table.end())
    return std::nullopt;

  return it->second->processKey(input, globalChromaticTranspose,
                                globalDegreeTranspose);
}

std::shared_ptr<Zone> ZoneManager::getZoneForInput(InputID input,
                                                   int layerIndex) {
  juce::ScopedReadLock lock(zoneLock);

  if (layerIndex < 0 || layerIndex >= (int)layerLookupTables.size())
    return nullptr;

  auto &table = layerLookupTables[(size_t)layerIndex];
  auto it = table.find(input);
  if (it == table.end())
    return nullptr;

  return it->second;
}

std::optional<juce::Colour> ZoneManager::getZoneColorForKey(int keyCode,
                                                            uintptr_t aliasHash,
                                                            int layerIndex) {
  // Use the exact same lookup logic as handleInput()
  // This ensures that if it plays, it paints.

  juce::ScopedReadLock sl(zoneLock);

  if (layerIndex < 0 || layerIndex >= (int)layerLookupTables.size())
    return std::nullopt;

  InputID id{aliasHash, keyCode};
  auto &table = layerLookupTables[(size_t)layerIndex];

  auto it = table.find(id);
  if (it != table.end()) {
    // Found the active zone for this key/device
    if (it->second)
      return it->second->zoneColor;
  }

  return std::nullopt;
}

int ZoneManager::getZoneCountForKey(int keyCode) const {
  juce::ScopedReadLock lock(zoneLock);
  int n = 0;
  for (const auto &z : zones) {
    if (std::find(z->inputKeyCodes.begin(), z->inputKeyCodes.end(), keyCode) !=
        z->inputKeyCodes.end())
      ++n;
  }
  return n;
}

int ZoneManager::getZoneCountForKey(int keyCode, uintptr_t aliasHash) const {
  juce::ScopedReadLock lock(zoneLock);
  int count = 0;

  // Iterate zones vector (NOT lookup table, because lookup table only stores
  // the winner)
  for (const auto &zone : zones) {
    // Check if targetAliasHash matches
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if inputKeyCodes contains this keyCode
    if (std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(),
                  keyCode) != zone->inputKeyCodes.end()) {
      count++;
    }
  }

  return count;
}

juce::ValueTree ZoneManager::toValueTree() const {
  juce::ScopedReadLock lock(zoneLock);

  juce::ValueTree vt("ZoneManager");

  // Save global transpose
  vt.setProperty("globalChromaticTranspose", globalChromaticTranspose, nullptr);
  vt.setProperty("globalDegreeTranspose", globalDegreeTranspose, nullptr);
  vt.setProperty("globalScaleName", globalScaleName, nullptr);
  vt.setProperty("globalRootNote", globalRootNote, nullptr);

  // Save all zones as children
  for (const auto &zone : zones) {
    vt.addChild(zone->toValueTree(), -1, nullptr);
  }

  return vt;
}

void ZoneManager::restoreFromValueTree(const juce::ValueTree &vt) {
  if (!vt.isValid() || !vt.hasType("ZoneManager"))
    return;

  juce::ScopedWriteLock lock(zoneLock);

  // Clear existing zones
  zones.clear();

  // Restore global transpose
  globalChromaticTranspose = vt.getProperty("globalChromaticTranspose", 0);
  globalDegreeTranspose = vt.getProperty("globalDegreeTranspose", 0);
  globalScaleName = vt.getProperty("globalScaleName", "Major").toString();
  globalRootNote = vt.getProperty("globalRootNote", 60);

  // Restore zones
  for (int i = 0; i < vt.getNumChildren(); ++i) {
    auto zoneVT = vt.getChild(i);
    if (zoneVT.hasType("Zone")) {
      auto zone = Zone::fromValueTree(zoneVT);
      if (zone) {
        rebuildZoneCache(zone.get());
        zones.push_back(zone);
      }
    }
  }

  rebuildLookupTable(); // Rebuild lookup table after restoring zones
  sendChangeMessage();
}
