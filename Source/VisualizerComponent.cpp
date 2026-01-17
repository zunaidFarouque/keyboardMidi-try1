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
  g.fillAll(juce::Colour(0xff111111)); // Background

  if (!zoneManager) {
    return; // Can't render without zone manager
  }

  // --- 1. Calculate Dynamic Scale ---
  // Full 104-key layout is roughly 23.0 keys wide (including numpad) and 6.5 keys tall (including function row).
  // Row -1 (function row) is at -1.2f offset, rows 0-4 are at 0-4
  // With 1.2f vertical spacing multiplier: function row at -1.44f, rows 0-4 at 0.0f to 4.8f
  // Row 4 bottom (including key height): 4.8f + 1.0f = 5.8f
  // Total span from function row top to row 4 bottom: 5.8f - (-1.44f) = 7.24f units
  float unitsWide = 23.0f;
  float unitsTall = 7.3f; // Accounts for function row at -1.2 offset + key height
  
  // Determine the maximum size a key can be while fitting in the component
  float scaleX = static_cast<float>(getWidth()) / unitsWide;
  float scaleY = static_cast<float>(getHeight()) / unitsTall;
  float keySize = std::min(scaleX, scaleY) * 0.9f; // 0.9f for margin
  
  // Calculate actual bounds
  // Function row top: startY + (-1.2f * 1.2f * keySize) = startY - 1.44f * keySize
  // Row 4 bottom: startY + (4.0f * 1.2f * keySize) + (1.0f * keySize) = startY + 5.8f * keySize
  // Total height needed: 5.8f - (-1.44f) = 7.24f * keySize
  
  // Position startY to fit everything within getHeight()
  // We want: startY - 1.44f * keySize >= 0 AND startY + 5.8f * keySize <= getHeight()
  // So: startY >= 1.44f * keySize AND startY <= getHeight() - 5.8f * keySize
  // Center it: startY = (1.44f * keySize + getHeight() - 5.8f * keySize) / 2.0f
  // Simplified: startY = (getHeight() - 4.36f * keySize) / 2.0f + 1.44f * keySize
  float totalWidth = unitsWide * keySize;
  float startX = (getWidth() - totalWidth) / 2.0f;
  
  // Calculate startY to ensure both function row and bottom row fit
  float functionRowTop = 1.44f * keySize; // Minimum Y for function row to be visible
  float row4Bottom = 5.8f * keySize; // Height from startY to row 4 bottom
  float minStartY = functionRowTop;
  float maxStartY = getHeight() - row4Bottom;
  
  // Center vertically within available space
  float startY = (minStartY + maxStartY) / 2.0f;
  
  // Clamp to ensure everything fits
  startY = juce::jlimit(minStartY, maxStartY, startY);

  // --- 2. Iterate Keys ---
  const auto &layout = KeyboardLayoutUtils::getLayout();
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    const auto &geometry = pair.second;
    
    // Calculate Position relative to start
    // Row -1 (function row) needs special handling - place it above row 0
    float rowOffset = (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f); // 1.2 multiplier for vertical spacing
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    
    // Define Inner Body (Padding)
    float padding = keySize * 0.1f; // 10% padding looks good at any size
    auto keyBounds = fullBounds.reduced(padding);

    // --- 3. Get Data from Engine ---
    // A. Zone Underlay Color
    juce::Colour underlayColor = juce::Colours::transparentBlack;
    // Check master alias (hash 0) first, then device aliases
    auto zoneColor = zoneManager->getZoneColorForKey(keyCode, 0);
    if (zoneColor.has_value()) {
      underlayColor = zoneColor.value();
    } else if (deviceManager) {
      // Try other aliases
      auto aliases = deviceManager->getAllAliasNames();
      for (const auto &aliasName : aliases) {
        uintptr_t aliasHash = aliasNameToHash(aliasName);
        zoneColor = zoneManager->getZoneColorForKey(keyCode, aliasHash);
        if (zoneColor.has_value()) {
          underlayColor = zoneColor.value();
          break;
        }
      }
    }

    // B. Key State
    bool isPressed = (activeKeys.find(keyCode) != activeKeys.end());
    
    // C. Text/Label
    juce::String labelText = geometry.label; // Default: "Q"
    
    // Find which zone this key belongs to and get the note name
    std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
    
    // Try master alias first
    zoneInfo = findZoneForKey(keyCode, 0);
    
    // If not found, try device aliases
    if (!zoneInfo.first && deviceManager) {
      auto aliases = deviceManager->getAllAliasNames();
      for (const auto &aliasName : aliases) {
        uintptr_t aliasHash = aliasNameToHash(aliasName);
        zoneInfo = findZoneForKey(keyCode, aliasHash);
        if (zoneInfo.first) {
          break;
        }
      }
    }
    
    if (zoneInfo.first) {
      uintptr_t aliasHash = zoneInfo.first->targetAliasHash;
      auto action = zoneManager->simulateInput(keyCode, aliasHash);
      
      if (action.has_value() && action->type == ActionType::Note) {
        labelText = MidiNoteUtilities::getMidiNoteName(action->data1);
      }
    }

    // --- 4. Render Layers ---
    
    // Layer 1: Underlay (Zone Color) - Sharp rectangle
    if (!underlayColor.isTransparent()) {
      g.setColour(underlayColor.withAlpha(0.6f));
      g.fillRect(fullBounds);
    }

    // Layer 2: Key Body
    g.setColour(isPressed ? juce::Colours::yellow : juce::Colour(0xff333333));
    g.fillRoundedRectangle(keyBounds, 6.0f);

    // Layer 3: Border
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(keyBounds, 6.0f, 2.0f);

    // Layer 4: Text
    g.setColour(isPressed ? juce::Colours::black : juce::Colours::white);
    g.setFont(keySize * 0.4f); // Dynamic font size
    g.drawText(labelText, keyBounds, juce::Justification::centred, false);
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
