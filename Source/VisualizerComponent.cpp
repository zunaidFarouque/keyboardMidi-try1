#include "VisualizerComponent.h"
#include "InputProcessor.h"
#include "PresetManager.h"
#include "SettingsManager.h"
#include "VoiceManager.h"
#include "Zone.h"
#include "RawInputManager.h"
#include <JuceHeader.h>
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

VisualizerComponent::VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, const VoiceManager &voiceMgr, SettingsManager *settingsMgr, PresetManager *presetMgr, InputProcessor *inputProc)
    : zoneManager(zoneMgr), deviceManager(deviceMgr), voiceManager(voiceMgr), settingsManager(settingsMgr), presetManager(presetMgr), inputProcessor(inputProc) {
  if (zoneManager) {
    zoneManager->addChangeListener(this);
  }
  if (settingsManager) {
    settingsManager->addChangeListener(this);
  }
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.addListener(this);
    }
    // Also listen to root node changes (in case mappings node is recreated)
    presetManager->getRootNode().addListener(this);
  }
  vBlankAttachment = std::make_unique<juce::VBlankAttachment>(this, [this] {
    // OPTIMIZATION: Stop all processing if window is minimized
    if (auto* peer = getPeer()) {
      if (peer->isMinimised()) {
        return;
      }
    }
    
    // 1. Poll External States (VoiceManager)
    bool currentSustain = voiceManager.isSustainActive();
    
    if (currentSustain != lastSustainState) {
      lastSustainState = currentSustain;
      needsRepaint = true;
    }

    // 2. Check Dirty Flag
    if (needsRepaint.exchange(false)) { // Atomic read-and-reset
      repaint();
    }
  });
}

VisualizerComponent::~VisualizerComponent() {
  // Stop callbacks immediately
  vBlankAttachment = nullptr;
  
  // Unregister listeners
  // Note: rawInputManager listener is removed by MainComponent destructor
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
  if (settingsManager) {
    settingsManager->removeChangeListener(this);
  }
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid()) {
      mappingsNode.removeListener(this);
    }
    presetManager->getRootNode().removeListener(this);
  }
  // VoiceManager doesn't have a listener interface, it's polled.
}

void VisualizerComponent::refreshCache() {
  if (!zoneManager) {
    cacheValid = false;
    return;
  }

  int width = getWidth();
  int height = getHeight();
  
  if (width <= 0 || height <= 0) {
    cacheValid = false;
    backgroundCache = juce::Image(); // Clear invalid cache
    return;
  }

  // Create cache image matching component size
  // Use a temporary image first, then assign to ensure proper cleanup
  juce::Image newCache(juce::Image::ARGB, width, height, true);
  
  {
    juce::Graphics g(newCache);
  
  g.fillAll(juce::Colour(0xff111111)); // Background

  // --- 0. Header Bar (Transpose left, Sustain right) ---
  auto bounds = juce::Rectangle<int>(0, 0, width, height);
  auto headerRect = bounds.removeFromTop(30);
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(headerRect);

  g.setColour(juce::Colours::white);
  g.setFont(12.0f);

  // TRANSPOSE (left)
  if (zoneManager) {
    int chrom = zoneManager->getGlobalChromaticTranspose();
    int deg = zoneManager->getGlobalDegreeTranspose();
    juce::String chromStr = (chrom >= 0) ? ("+" + juce::String(chrom)) : juce::String(chrom);
    juce::String degStr = (deg >= 0) ? ("+" + juce::String(deg)) : juce::String(deg);
    juce::String transposeText = "TRANSPOSE: [Pitch: " + chromStr + "] [Scale: " + degStr + "]";
    g.drawText(transposeText, 8, 0, 280, headerRect.getHeight(), juce::Justification::centredLeft, false);
  }

  // Sustain Indicator (right) - Use last known state for cache
  juce::Colour sustainColor = lastSustainState ? juce::Colours::lime : juce::Colours::grey;
  int indicatorSize = 12;
  int indicatorX = headerRect.getRight() - 100;
  int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
  g.setColour(sustainColor);
  g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
  g.setColour(juce::Colours::white);
  g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60, indicatorSize, juce::Justification::centredLeft, false);

  // --- 1. Calculate Dynamic Scale ---
  float unitsWide = 23.0f;
  float unitsTall = 7.3f;
  float headerHeight = 30.0f;
  float availableHeight = static_cast<float>(height) - headerHeight;
  
  float scaleX = static_cast<float>(width) / unitsWide;
  float scaleY = (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
  float keySize = std::min(scaleX, scaleY) * 0.9f;
  
  float row4Bottom = 5.8f * keySize;
  float minStartY = headerHeight + 1.44f * keySize;
  float maxStartY = static_cast<float>(height) - row4Bottom;
  
  float startY = (minStartY + maxStartY) / 2.0f;
  startY = juce::jlimit(minStartY, maxStartY, startY);
  
  float totalWidth = unitsWide * keySize;
  float startX = (static_cast<float>(width) - totalWidth) / 2.0f;

  // --- 2. Iterate Keys (Draw Static State Only) ---
  const auto &layout = KeyboardLayoutUtils::getLayout();
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    const auto &geometry = pair.second;
    
    float rowOffset = (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f);
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    
    float padding = keySize * 0.1f;
    auto keyBounds = fullBounds.reduced(padding);

    // --- 3. Get Data from Engine (Static Only) ---
    int zoneCount = zoneManager->getZoneCountForKey(keyCode);
    bool hasManual = inputProcessor && inputProcessor->hasManualMappingForKey(keyCode);
    bool isConflict = (zoneCount >= 2) || (zoneCount >= 1 && hasManual);

    // A. Underlay Color: Manual Mapping first, else Zone (Phase 38)
    juce::Colour underlayColor = juce::Colours::transparentBlack;
    if (isConflict) {
      underlayColor = juce::Colours::red.withAlpha(0.7f);
    } else {
      auto manualType = (inputProcessor && settingsManager)
          ? inputProcessor->getMappingType(keyCode, 0)
          : std::optional<ActionType>{};
      if (manualType.has_value()) {
        underlayColor = settingsManager->getTypeColor(manualType.value());
      } else {
        auto zoneColor = zoneManager->getZoneColorForKey(keyCode, 0);
        if (zoneColor.has_value()) {
          underlayColor = zoneColor.value();
        } else if (deviceManager) {
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
      }
    }

    // C. Text/Label
    juce::String labelText = geometry.label;
    
    std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
    zoneInfo = findZoneForKey(keyCode, 0);
    
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
      juce::String cachedLabel = zoneInfo.first->getKeyLabel(keyCode);
      if (cachedLabel.isNotEmpty()) {
        labelText = cachedLabel;
      } else {
        uintptr_t aliasHash = zoneInfo.first->targetAliasHash;
        auto action = zoneManager->simulateInput(keyCode, aliasHash);
        
        if (action.has_value() && action->type == ActionType::Note) {
          labelText = MidiNoteUtilities::getMidiNoteName(action->data1);
        }
      }
    }

    // --- 4. Render Static Layers (Off State) ---
    
    // Layer 1: Underlay (Zone Color)
    if (!underlayColor.isTransparent()) {
      g.setColour(underlayColor.withAlpha(0.6f));
      g.fillRect(fullBounds);
    }

    // Layer 2: Key Body (Dark Grey - Off State)
    g.setColour(juce::Colour(0xff333333));
    g.fillRoundedRectangle(keyBounds, 6.0f);

    // Layer 3: Border
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(keyBounds, 6.0f, 2.0f);

    // Layer 4: Text (white for off state, red if conflict)
    juce::Colour textColor = isConflict ? juce::Colours::red : juce::Colours::white;
    g.setColour(textColor);
    g.setFont(keySize * 0.4f);
    g.drawText(labelText, keyBounds, juce::Justification::centred, false);
  }
  } // Graphics object destroyed here
  
  // Assign new cache only after Graphics is fully destroyed
  backgroundCache = std::move(newCache);
  cacheValid = true;
}

void VisualizerComponent::paint(juce::Graphics &g) {
  if (!zoneManager) {
    return; // Can't render without zone manager
  }
  
  // Check Cache Validity
  if (!cacheValid || backgroundCache.isNull() || 
      backgroundCache.getWidth() != getWidth() || 
      backgroundCache.getHeight() != getHeight()) {
    refreshCache();
  }
  
  // Safety check: ensure cache is valid before drawing
  if (backgroundCache.isNull() || !cacheValid) {
    g.fillAll(juce::Colour(0xff111111)); // Fallback background
    return;
  }
  
  // Draw Background Cache
  g.drawImageAt(backgroundCache, 0, 0);
  
  // Update Sustain Indicator (dynamic - always redraw since it changes frequently)
  bool sustainActive = voiceManager.isSustainActive();
  auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), 30);
  juce::Colour sustainColor = sustainActive ? juce::Colours::lime : juce::Colours::grey;
  int indicatorSize = 12;
  int indicatorX = headerRect.getRight() - 100;
  int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(indicatorX - 5, indicatorY - 2, 80, indicatorSize + 4); // Clear area
  g.setColour(sustainColor);
  g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
  g.setColour(juce::Colours::white);
  g.setFont(12.0f);
  g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60, indicatorSize, juce::Justification::centredLeft, false);
  
  // Check if MIDI mode is disabled and draw overlay
  bool midiModeDisabled = settingsManager && !settingsManager->isMidiModeActive();

  // --- Calculate Dynamic Scale (Same as refreshCache) ---
  float unitsWide = 23.0f;
  float unitsTall = 7.3f;
  float headerHeight = 30.0f;
  float availableHeight = static_cast<float>(getHeight()) - headerHeight;
  
  float scaleX = static_cast<float>(getWidth()) / unitsWide;
  float scaleY = (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
  float keySize = std::min(scaleX, scaleY) * 0.9f;
  
  float row4Bottom = 5.8f * keySize;
  float minStartY = headerHeight + 1.44f * keySize;
  float maxStartY = static_cast<float>(getHeight()) - row4Bottom;
  
  float startY = (minStartY + maxStartY) / 2.0f;
  startY = juce::jlimit(minStartY, maxStartY, startY);
  
  float totalWidth = unitsWide * keySize;
  float startX = (static_cast<float>(getWidth()) - totalWidth) / 2.0f;

  // Snapshot active keys under lock (RawInput may come from OS thread)
  std::set<int> activeKeysSnapshot;
  {
    juce::ScopedLock lock(keyStateLock);
    activeKeysSnapshot = activeKeys;
  }

  // Collect all keys that need dynamic rendering (active or latched)
  std::set<int> keysToRender;
  keysToRender.insert(activeKeysSnapshot.begin(), activeKeysSnapshot.end());
  
  // Also check for latched keys
  const auto &layout = KeyboardLayoutUtils::getLayout();
  for (const auto &pair : layout) {
    int keyCode = pair.first;
    if (voiceManager.isKeyLatched(keyCode)) {
      keysToRender.insert(keyCode);
    }
  }

  // --- Draw Dynamic Keys (Active/Latched) on Top of Cache ---
  for (int keyCode : keysToRender) {
    auto it = layout.find(keyCode);
    if (it == layout.end()) continue;
    
    const auto &geometry = it->second;
    
    // Calculate Position (same as refreshCache)
    float rowOffset = (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f);
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    float padding = keySize * 0.1f;
    auto keyBounds = fullBounds.reduced(padding);

    // Get key state
    bool isPressed = (activeKeysSnapshot.find(keyCode) != activeKeysSnapshot.end());
    bool isLatched = voiceManager.isKeyLatched(keyCode);
    
    // Get conflict state (needed for text color)
    int zoneCount = zoneManager->getZoneCountForKey(keyCode);
    bool hasManual = inputProcessor && inputProcessor->hasManualMappingForKey(keyCode);
    bool isConflict = (zoneCount >= 2) || (zoneCount >= 1 && hasManual);
    
    // Get label text (same logic as refreshCache)
    juce::String labelText = geometry.label;
    std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
    zoneInfo = findZoneForKey(keyCode, 0);
    
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
      juce::String cachedLabel = zoneInfo.first->getKeyLabel(keyCode);
      if (cachedLabel.isNotEmpty()) {
        labelText = cachedLabel;
      } else {
        uintptr_t aliasHash = zoneInfo.first->targetAliasHash;
        auto action = zoneManager->simulateInput(keyCode, aliasHash);
        
        if (action.has_value() && action->type == ActionType::Note) {
          labelText = MidiNoteUtilities::getMidiNoteName(action->data1);
        }
      }
    }

    // Draw Latched State (Cyan)
    if (isLatched) {
      g.setColour(juce::Colours::cyan.withAlpha(0.8f));
      g.fillRoundedRectangle(keyBounds, 6.0f);
    }

    // Draw Active State (Yellow)
    if (isPressed) {
      g.setColour(juce::Colours::yellow);
      g.fillRoundedRectangle(keyBounds, 6.0f);
    }

    // Redraw Border
    g.setColour(juce::Colours::grey);
    g.drawRoundedRectangle(keyBounds, 6.0f, 2.0f);

    // Redraw Text (Important: text must be on top of highlight)
    juce::Colour textColor = isConflict ? juce::Colours::red
        : (isPressed ? juce::Colours::black : juce::Colours::white);
    g.setColour(textColor);
    g.setFont(keySize * 0.4f);
    g.drawText(labelText, keyBounds, juce::Justification::centred, false);
  }
  
  // Draw overlay if MIDI mode is disabled
  if (midiModeDisabled) {
    // Semi-transparent black overlay
    g.setColour(juce::Colours::black.withAlpha(0.7f));
    g.fillAll();
    
    // Draw text message
    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    juce::String toggleKeyName = settingsManager ? RawInputManager::getKeyName(settingsManager->getToggleKey()) : "Scroll Lock";
    juce::String message = "MIDI MODE DISABLED\n(Press " + toggleKeyName + " to enable)";
    g.drawText(message, getLocalBounds(), juce::Justification::centred, false);
  }
}

void VisualizerComponent::resized() {
  cacheValid = false; // Invalidate cache on resize
  needsRepaint = true;
  repaint(); // Resize needs immediate repaint
}

void VisualizerComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  juce::ScopedLock lock(keyStateLock);
  if (isDown)
    activeKeys.insert(keyCode);
  else
    activeKeys.erase(keyCode);
  needsRepaint = true;
}

void VisualizerComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events
}

void VisualizerComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager || source == settingsManager) {
    cacheValid = false; // Invalidate cache on zone/transpose changes
    needsRepaint = true;
    juce::Component::SafePointer<VisualizerComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      // Repaint will be triggered by vBlank callback if needed
    });
  }
}

void VisualizerComponent::valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (parentTree.isEquivalentTo(mappingsNode) || parentTree.getParent().isEquivalentTo(mappingsNode)) {
    cacheValid = false;
    needsRepaint = true;
    juce::Component::SafePointer<VisualizerComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
      });
    });
  }
}

void VisualizerComponent::valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) {
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (parentTree.isEquivalentTo(mappingsNode) || parentTree.getParent().isEquivalentTo(mappingsNode)) {
    cacheValid = false;
    needsRepaint = true;
    juce::Component::SafePointer<VisualizerComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
      });
    });
  }
}

void VisualizerComponent::valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) {
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (treeWhosePropertyHasChanged.getParent().isEquivalentTo(mappingsNode)) {
    cacheValid = false;
    needsRepaint = true;
    juce::Component::SafePointer<VisualizerComponent> safeThis(this);
    juce::MessageManager::callAsync([safeThis] {
      if (safeThis == nullptr) return;
      juce::MessageManager::callAsync([safeThis] {
        if (safeThis == nullptr) return;
      });
    });
  }
}

void VisualizerComponent::valueTreeParentChanged(juce::ValueTree &treeWhoseParentHasChanged) {
  auto mappingsNode = presetManager ? presetManager->getMappingsNode() : juce::ValueTree();
  if (treeWhoseParentHasChanged.isEquivalentTo(mappingsNode) || treeWhoseParentHasChanged.getParent().isEquivalentTo(mappingsNode)) {
    cacheValid = false;
    needsRepaint = true;
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
