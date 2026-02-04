#include "StartupManager.h"
#include "ScaleUtilities.h"
#include "SettingsManager.h"
#include "Zone.h"

StartupManager::StartupManager(PresetManager *presetMgr,
                               DeviceManager *deviceMgr, ZoneManager *zoneMgr,
                               SettingsManager *settingsMgr)
    : presetManager(presetMgr), deviceManager(deviceMgr), zoneManager(zoneMgr),
      settingsManager(settingsMgr) {

  // Setup file paths
  appDataFolder =
      juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
          .getChildFile("MIDIQy");
  autoloadFile = appDataFolder.getChildFile("autoload.xml");
  settingsFile = appDataFolder.getChildFile("settings.xml");

  // Listen to changes
  if (presetManager) {
    presetManager->getRootNode().addListener(this);
  }
  if (deviceManager) {
    deviceManager->addChangeListener(this);
  }
  if (zoneManager) {
    zoneManager->addChangeListener(this);
  }
  if (settingsManager) {
    settingsManager->addChangeListener(this);
  }
}

StartupManager::~StartupManager() {
  stopTimer();

  // Remove listeners
  if (presetManager) {
    presetManager->getRootNode().removeListener(this);
  }
  if (deviceManager) {
    deviceManager->removeChangeListener(this);
  }
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
  if (settingsManager) {
    settingsManager->removeChangeListener(this);
  }
}

void StartupManager::initApp() {
  // Ensure app data folder exists
  appDataFolder.createDirectory();

  // 1. Load Settings (global) – loadFromXml validates pitchBendRange etc.
  if (settingsManager) {
    settingsManager->loadFromXml(settingsFile);
  }

  // Load DeviceManager config (global settings)
  if (deviceManager) {
    deviceManager->loadConfig();
  }

  // 2. Load Preset (autoload) with fail-safe – Phase 43.2: silence listeners
  // during bulk update
  if (presetManager)
    presetManager->beginTransaction();

  bool loadSuccess = false;
  if (autoloadFile.existsAsFile()) {
    if (auto xml = juce::parseXML(autoloadFile)) {
      auto sessionTree = juce::ValueTree::fromXml(*xml);
      if (sessionTree.isValid() && sessionTree.hasType("OmniKeySession")) {
        if (presetManager) {
          auto presetNode = sessionTree.getChildWithName("OmniKeyPreset");
          if (presetNode.isValid()) {
            auto &rootNode = presetManager->getRootNode();
            while (rootNode.getNumChildren() > 0) {
              rootNode.removeChild(0, nullptr);
            }
            for (int i = 0; i < presetNode.getNumChildren(); ++i) {
              rootNode.addChild(presetNode.getChild(i).createCopy(), -1,
                                nullptr);
            }
            for (int i = 0; i < presetNode.getNumProperties(); ++i) {
              auto propName = presetNode.getPropertyName(i);
              rootNode.setProperty(propName, presetNode.getProperty(propName),
                                   nullptr);
            }
            if (presetManager->getLayersList().getNumChildren() > 0) {
              loadSuccess = true;
            }
          }
        }
        if (loadSuccess && zoneManager) {
          auto zoneMgrNode = sessionTree.getChildWithName("ZoneManager");
          if (zoneMgrNode.isValid()) {
            zoneManager->restoreFromValueTree(zoneMgrNode);
          }
        }
      }
    }
  }
  if (!loadSuccess) {
    DBG("StartupManager: Autoload missing or corrupt. Creating defaults.");
    createFactoryDefault();
  }

  if (presetManager)
    presetManager->endTransaction();
}

void StartupManager::createFactoryDefault() {
  // Phase 43.2: If not already in a transaction (e.g. called from "Reset
  // Everything"), start one
  bool wasLoading = presetManager ? presetManager->getIsLoading() : false;
  if (presetManager && !wasLoading)
    presetManager->beginTransaction();

  // Clear all mappings
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      while (mappingsNode.getNumChildren() > 0) {
        mappingsNode.removeChild(0, nullptr);
      }
    }
  }

  // Clear all zones
  if (zoneManager) {
    auto zones = zoneManager->getZones();
    for (auto &zone : zones) {
      zoneManager->removeZone(zone);
    }
  }

  // Phase 46.1: Removed "Master Input" auto-creation.
  // DeviceManager now starts with zero aliases. Users must explicitly create
  // aliases and assign hardware via Device Setup.

  // Zone: Create "Main Keys" (C Major, Linear, Keys Q->P)
  if (zoneManager) {
    auto mainZone = std::make_shared<Zone>();
    mainZone->name = "Main Keys";
    mainZone->targetAliasHash = 0; // Global (All Devices)
    mainZone->rootNote = 60;       // C4
    mainZone->scaleName = "Major";
    mainZone->chromaticOffset = 0;
    mainZone->degreeOffset = 0;
    mainZone->ignoreGlobalTranspose = false;
    mainZone->layoutStrategy = Zone::LayoutStrategy::Linear;
    mainZone->gridInterval = 5;

    // Keys Q->P (Virtual codes: 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49,
    // 0x4F, 0x50) Q, W, E, R, T, Y, U, I, O, P
    mainZone->inputKeyCodes = {0x51, 0x57, 0x45, 0x52, 0x54,
                               0x59, 0x55, 0x49, 0x4F, 0x50};

    zoneManager->addZone(mainZone);
  }

  if (presetManager && !wasLoading)
    presetManager->endTransaction();

  // Save immediately
  saveImmediate();
}

void StartupManager::triggerSave() {
  // Start 2-second timer (debounce)
  startTimer(2000);
}

void StartupManager::saveImmediate() {
  stopTimer();
  performSave();
}

void StartupManager::timerCallback() { performSave(); }

void StartupManager::performSave() {
  stopTimer();

  // Save SettingsManager to separate file
  if (settingsManager) {
    settingsManager->saveToXml(settingsFile);
  }

  // Construct master XML
  juce::ValueTree sessionTree("OmniKeySession");

  // Add PresetManager root
  if (presetManager) {
    sessionTree.addChild(presetManager->getRootNode().createCopy(), -1,
                         nullptr);
  }

  // Add ZoneManager tree
  if (zoneManager) {
    sessionTree.addChild(zoneManager->toValueTree(), -1, nullptr);
  }

  // Write to autoload.xml
  if (auto xml = sessionTree.createXml()) {
    xml->writeTo(autoloadFile);
  }
}

void StartupManager::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  // Preset or mapping changed - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  // Preset or mapping added - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {
  // Preset or mapping removed - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildOrderChanged(
    juce::ValueTree &parentTreeWhoseChildrenHaveMoved, int oldIndex,
    int newIndex) {
  // Preset or mapping order changed - trigger save
  triggerSave();
}

void StartupManager::changeListenerCallback(juce::ChangeBroadcaster *source) {
  // Zone or Device changed - trigger save
  triggerSave();
}
