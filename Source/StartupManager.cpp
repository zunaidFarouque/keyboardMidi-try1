#include "StartupManager.h"
#include "Zone.h"
#include "ScaleUtilities.h"

StartupManager::StartupManager(PresetManager* presetMgr, DeviceManager* deviceMgr, ZoneManager* zoneMgr)
    : presetManager(presetMgr), deviceManager(deviceMgr), zoneManager(zoneMgr) {
  
  // Setup file paths
  appDataFolder = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                      .getChildFile("OmniKey");
  autoloadFile = appDataFolder.getChildFile("autoload.xml");
  
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
}

void StartupManager::initApp() {
  // Ensure app data folder exists
  appDataFolder.createDirectory();
  
  // Load DeviceManager config (global settings)
  if (deviceManager) {
    deviceManager->loadConfig();
  }
  
  // Check for autoload.xml
  if (autoloadFile.existsAsFile()) {
    // Load existing session
    if (auto xml = juce::parseXML(autoloadFile)) {
      auto sessionTree = juce::ValueTree::fromXml(*xml);
      
      if (sessionTree.isValid() && sessionTree.hasType("OmniKeySession")) {
        // Load PresetManager data
        if (presetManager) {
          auto presetNode = sessionTree.getChildWithName("OmniKeyPreset");
          if (presetNode.isValid()) {
            // Replace the root node with the loaded one
            auto& rootNode = presetManager->getRootNode();
            // Remove existing children
            while (rootNode.getNumChildren() > 0) {
              rootNode.removeChild(0, nullptr);
            }
            // Copy children from loaded preset
            for (int i = 0; i < presetNode.getNumChildren(); ++i) {
              rootNode.addChild(presetNode.getChild(i).createCopy(), -1, nullptr);
            }
            // Copy properties
            for (int i = 0; i < presetNode.getNumProperties(); ++i) {
              auto propName = presetNode.getPropertyName(i);
              rootNode.setProperty(propName, presetNode.getProperty(propName), nullptr);
            }
          }
        }
        
        // Load ZoneManager data
        if (zoneManager) {
          auto zoneMgrNode = sessionTree.getChildWithName("ZoneManager");
          if (zoneMgrNode.isValid()) {
            zoneManager->restoreFromValueTree(zoneMgrNode);
          }
        }
      }
    }
  } else {
    // No autoload file - create factory default
    createFactoryDefault();
  }
}

void StartupManager::createFactoryDefault() {
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
    for (auto& zone : zones) {
      zoneManager->removeZone(zone);
    }
  }
  
  // Device: Ensure "Master Input" alias exists (it's actually "Any / Master" with hash 0)
  // DeviceManager handles this automatically, but we can ensure it exists
  if (deviceManager && !deviceManager->aliasExists("Any / Master")) {
    // Actually, "Any / Master" is a special case (hash 0), so we don't need to create it
    // But we can create a "Master Input" alias if needed
    if (!deviceManager->aliasExists("Master Input")) {
      deviceManager->createAlias("Master Input");
    }
  }
  
  // Zone: Create "Main Keys" (C Major, Linear, Keys Q->P)
  if (zoneManager) {
    auto mainZone = std::make_shared<Zone>();
    mainZone->name = "Main Keys";
    mainZone->targetAliasHash = 0; // "Any / Master"
    mainZone->rootNote = 60; // C4
    mainZone->scaleName = "Major";
    mainZone->chromaticOffset = 0;
    mainZone->degreeOffset = 0;
    mainZone->isTransposeLocked = false;
    mainZone->layoutStrategy = Zone::LayoutStrategy::Linear;
    mainZone->gridInterval = 5;
    
    // Keys Q->P (Virtual codes: 0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50)
    // Q, W, E, R, T, Y, U, I, O, P
    mainZone->inputKeyCodes = {0x51, 0x57, 0x45, 0x52, 0x54, 0x59, 0x55, 0x49, 0x4F, 0x50};
    
    zoneManager->addZone(mainZone);
  }
  
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

void StartupManager::timerCallback() {
  performSave();
}

void StartupManager::performSave() {
  stopTimer();
  
  // Construct master XML
  juce::ValueTree sessionTree("OmniKeySession");
  
  // Add PresetManager root
  if (presetManager) {
    sessionTree.addChild(presetManager->getRootNode().createCopy(), -1, nullptr);
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

void StartupManager::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged,
                                               const juce::Identifier& property) {
  // Preset or mapping changed - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildAdded(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded) {
  // Preset or mapping added - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildRemoved(juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved,
                                            int indexFromWhichChildWasRemoved) {
  // Preset or mapping removed - trigger save
  triggerSave();
}

void StartupManager::valueTreeChildOrderChanged(juce::ValueTree& parentTreeWhoseChildrenHaveMoved,
                                                int oldIndex, int newIndex) {
  // Preset or mapping order changed - trigger save
  triggerSave();
}

void StartupManager::changeListenerCallback(juce::ChangeBroadcaster* source) {
  // Zone or Device changed - trigger save
  triggerSave();
}
