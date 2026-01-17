#include "VisualizerComponent.h"
#include "Zone.h"
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

VisualizerComponent::VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr)
    : zoneManager(zoneMgr), deviceManager(deviceMgr) {
  if (zoneManager) {
    zoneManager->addChangeListener(this);
  }
}

VisualizerComponent::~VisualizerComponent() {
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
}

void VisualizerComponent::paint(juce::Graphics &g) {
  const auto &layout = KeyboardLayoutUtils::getLayout();
  auto bounds = getLocalBounds().toFloat();
  
  // Calculate key size based on available space
  float padding = 8.0f;
  float maxRowWidth = 13.0f; // Approximate maximum row width
  float keySize = (bounds.getWidth() - 2.0f * padding) / maxRowWidth;
  float maxRowHeight = 5.0f; // 5 rows (0-4)
  float availableHeight = bounds.getHeight() - 2.0f * padding;
  float calculatedKeySize = availableHeight / (maxRowHeight * 1.2f); // 1.2 for spacing
  
  // Use the smaller of the two to ensure everything fits
  keySize = juce::jmin(keySize, calculatedKeySize);

  // Layer 1: Zone Backgrounds
  if (zoneManager) {
    const auto zones = zoneManager->getZones();
    
    // Check "Master" alias (hash 0) first
    for (const auto &pair : layout) {
      int keyCode = pair.first;
      auto keyBounds = KeyboardLayoutUtils::getKeyBounds(keyCode, keySize, padding);
      
      if (isKeyInAnyZone(keyCode, 0)) { // Check Master alias
        g.setColour(juce::Colours::blue.withAlpha(0.2f));
        g.fillRoundedRectangle(keyBounds, 4.0f);
      }
    }

    // Also check all active aliases (if deviceManager is available)
    if (deviceManager) {
      auto aliases = deviceManager->getAllAliasNames();
      for (const auto &aliasName : aliases) {
        uintptr_t aliasHash = aliasNameToHash(aliasName);
        for (const auto &pair : layout) {
          int keyCode = pair.first;
          if (isKeyInAnyZone(keyCode, aliasHash)) {
            auto keyBounds = KeyboardLayoutUtils::getKeyBounds(keyCode, keySize, padding);
            g.setColour(juce::Colours::blue.withAlpha(0.2f));
            g.fillRoundedRectangle(keyBounds, 4.0f);
          }
        }
      }
    }
  }

  // Layer 2: Keys
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    auto keyBounds = KeyboardLayoutUtils::getKeyBounds(keyCode, keySize, padding);
    
    if (keyBounds.isEmpty())
      continue;

    // Fill color: Bright Yellow if active, Dark Grey otherwise
    bool isActive = activeKeys.find(keyCode) != activeKeys.end();
    g.setColour(isActive ? juce::Colours::yellow : juce::Colour(0xff444444));
    g.fillRoundedRectangle(keyBounds, 4.0f);

    // Outline
    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRoundedRectangle(keyBounds, 4.0f, 1.0f);
  }

  // Layer 3: Labels
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    const auto &geom = pair.second;
    auto keyBounds = KeyboardLayoutUtils::getKeyBounds(keyCode, keySize, padding);
    
    if (keyBounds.isEmpty())
      continue;

    // Top Left: Physical key name
    g.setColour(juce::Colours::white.withAlpha(0.6f));
    g.setFont(10.0f);
    g.drawText(geom.label, keyBounds.reduced(2.0f), juce::Justification::topLeft, false);

    // Center: Note name (if mapped)
    if (zoneManager) {
      // Try Master alias first
      uintptr_t aliasHash = 0;
      auto action = zoneManager->simulateInput(keyCode, aliasHash);
      
      if (action.has_value() && action->type == ActionType::Note) {
        int noteNumber = action->data1;
        juce::String noteName = MidiNoteUtilities::getMidiNoteName(noteNumber);
        
        // Check if this is the root note
        auto zoneInfo = findZoneForKey(keyCode, aliasHash);
        bool isRoot = zoneInfo.second;
        
        if (isRoot) {
          // Bold/Gold for root note
          g.setColour(juce::Colours::gold);
          g.setFont(14.0f);
        } else {
          g.setColour(juce::Colours::lightgreen);
          g.setFont(12.0f);
        }
        
        g.drawText(noteName, keyBounds.reduced(2.0f), juce::Justification::centred, false);
      }
    }
  }
}

void VisualizerComponent::resized() {
  repaint();
}

void VisualizerComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  if (isDown) {
    activeKeys.insert(keyCode);
  } else {
    activeKeys.erase(keyCode);
  }
  
  juce::MessageManager::callAsync([this] {
    repaint();
  });
}

void VisualizerComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events
}

void VisualizerComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager) {
    juce::MessageManager::callAsync([this] {
      repaint();
    });
  }
}

bool VisualizerComponent::isKeyInAnyZone(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return false;

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(), keyCode);
    if (it != zone->inputKeyCodes.end()) {
      return true;
    }
  }

  return false;
}

std::pair<std::shared_ptr<Zone>, bool> VisualizerComponent::findZoneForKey(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return {nullptr, false};

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(), keyCode);
    if (it != zone->inputKeyCodes.end()) {
      // Calculate the degree (same logic as Zone::processKey)
      int degree = 0;
      
      if (zone->layoutStrategy == Zone::LayoutStrategy::Linear) {
        // Linear mode: Use index
        int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
        degree = index;
      } else {
        // Grid mode: Calculate based on keyboard geometry
        const auto& layout = KeyboardLayoutUtils::getLayout();
        
        auto currentKeyIt = layout.find(keyCode);
        if (currentKeyIt == layout.end()) {
          // Fallback to Linear if key not in layout
          int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
          degree = index;
        } else {
          // Get anchor key (first key in inputKeyCodes)
          if (zone->inputKeyCodes.empty()) {
            return {nullptr, false};
          }
          
          auto anchorKeyIt = layout.find(zone->inputKeyCodes[0]);
          if (anchorKeyIt == layout.end()) {
            // Fallback to Linear if anchor not in layout
            int index = static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
            degree = index;
          } else {
            const auto& currentKey = currentKeyIt->second;
            const auto& anchorKey = anchorKeyIt->second;
            
            int deltaCol = static_cast<int>(currentKey.col) - static_cast<int>(anchorKey.col);
            int deltaRow = currentKey.row - anchorKey.row;
            
            // Degree = deltaCol + (deltaRow * gridInterval)
            degree = deltaCol + (deltaRow * zone->gridInterval);
          }
        }
      }
      
      // Calculate effective transposition
      int effDegTrans = zone->isTransposeLocked ? 0 : zoneManager->getGlobalDegreeTranspose();
      
      // Calculate final degree (same as Zone::processKey)
      int finalDegree = degree + zone->degreeOffset + effDegTrans;
      
      // Check if this is the root (finalDegree == 0)
      bool isRoot = (finalDegree == 0);
      
      return {zone, isRoot};
    }
  }

  return {nullptr, false};
}
