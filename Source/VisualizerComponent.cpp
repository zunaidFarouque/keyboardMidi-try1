#include "VisualizerComponent.h"
#include "InputProcessor.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include "VoiceManager.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

VisualizerComponent::VisualizerComponent(ZoneManager *zoneMgr,
                                         DeviceManager *deviceMgr,
                                         const VoiceManager &voiceMgr,
                                         SettingsManager *settingsMgr,
                                         PresetManager *presetMgr,
                                         InputProcessor *inputProc)
    : zoneManager(zoneMgr), deviceManager(deviceMgr), voiceManager(voiceMgr),
      settingsManager(settingsMgr), presetManager(presetMgr),
      inputProcessor(inputProc) {
  // Phase 42: Listeners moved to initialize() – no addListener here

  // Setup View Selector (Phase 39)
  addAndMakeVisible(viewSelector);
  viewSelector.onChange = [this] { onViewSelectorChanged(); };
  updateViewSelector();
  // Ensure selector is on top to receive mouse events
  viewSelector.toFront(false);

  // Hide view selector if Studio Mode is OFF (Phase 9.5)
  if (settingsManager) {
    viewSelector.setVisible(settingsManager->isStudioMode());
    if (!settingsManager->isStudioMode()) {
      // Lock to Global view
      currentViewHash = 0;
      viewSelector.setSelectedId(1, juce::dontSendNotification);
    }
  }

  // Initial positioning (will be updated in resized(), but set initial bounds)
  viewSelector.setBounds(0, 0, 200, 25);
  vBlankAttachment = std::make_unique<juce::VBlankAttachment>(this, [this] {
    // OPTIMIZATION: Stop all processing if window is minimized
    if (auto *peer = getPeer()) {
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

void VisualizerComponent::setVisualizedLayer(int layerId) {
  // Clamp to non-negative; InputProcessor further clamps to valid range
  if (layerId < 0)
    layerId = 0;
  currentVisualizedLayer = layerId;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::initialize() {
  if (zoneManager)
    zoneManager->addChangeListener(this);
  if (settingsManager)
    settingsManager->addChangeListener(this);
  if (presetManager) {
    auto mappingsNode = presetManager->getMappingsNode();
    if (mappingsNode.isValid())
      mappingsNode.addListener(this);
    presetManager->getRootNode().addListener(this);
  }
  if (deviceManager)
    deviceManager->addChangeListener(this);
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
  if (deviceManager) {
    deviceManager->removeChangeListener(this);
  }
  // VoiceManager doesn't have a listener interface, it's polled.
}

void VisualizerComponent::refreshCache() {
  // SAFETY: Prevent creating 0x0 images or drawing into invalid bounds
  if (getWidth() <= 0 || getHeight() <= 0) {
    cacheValid = false;
    backgroundCache = juce::Image();
    return;
  }
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
      juce::String chromStr =
          (chrom >= 0) ? ("+" + juce::String(chrom)) : juce::String(chrom);
      juce::String degStr =
          (deg >= 0) ? ("+" + juce::String(deg)) : juce::String(deg);
      juce::String transposeText =
          "TRANSPOSE: [Pitch: " + chromStr + "] [Scale: " + degStr + "]";
      g.drawText(transposeText, 8, 0, 280, headerRect.getHeight(),
                 juce::Justification::centredLeft, false);
    }

    // Sustain Indicator (right) - Use last known state for cache
    juce::Colour sustainColor =
        lastSustainState ? juce::Colours::lime : juce::Colours::grey;
    int indicatorSize = 12;
    int indicatorX = headerRect.getRight() - 100;
    int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
    g.setColour(sustainColor);
    g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
    g.setColour(juce::Colours::white);
    g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60,
               indicatorSize, juce::Justification::centredLeft, false);

    // --- 1. Calculate Dynamic Scale ---
    float unitsWide = 23.0f;
    float unitsTall = 7.3f;
    float headerHeight = 30.0f;
    float availableHeight = static_cast<float>(height) - headerHeight;

    float scaleX = static_cast<float>(width) / unitsWide;
    float scaleY =
        (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
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

      float rowOffset =
          (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
      float x = startX + (geometry.col * keySize);
      float y = startY + (rowOffset * keySize * 1.2f);
      float w = geometry.width * keySize;
      float h = geometry.height * keySize;

      juce::Rectangle<float> fullBounds(x, y, w, h);

      float padding = keySize * 0.1f;
      auto keyBounds = fullBounds.reduced(padding);

      // --- 3. Get Data from Engine using SimulationResult (Phase 39.1/39.5)
      // ---
      SimulationResult simResult;
      juce::Colour underlayColor = juce::Colours::transparentBlack;
      juce::Colour borderColor = juce::Colours::grey;
      float borderWidth = 1.0f;
      float alpha = 1.0f;

      if (inputProcessor && settingsManager) {
        // Phase 9.5: When Studio Mode is OFF, simulateInput will force
        // viewDeviceHash to 0 So we can always call it, but use currentViewHash
        // (which is 0 when Studio Mode is OFF)
        simResult = inputProcessor->simulateInput(currentViewHash, keyCode,
                                                  currentVisualizedLayer);

        // Phase 39.5: Fix color determination logic
        // Note: When Studio Mode is OFF, simulateInput forces viewDeviceHash to
        // 0, so this works correctly
        if (simResult.state != VisualState::Empty) {
          // 1. Determine Base Color
          if (simResult.isZone) {
            // Phase 47: color lookup should not depend on VisualState.
            // Try current view first, then fall back to Global.
            auto zColor =
                zoneManager->getZoneColorForKey(keyCode, currentViewHash);

            if (!zColor.has_value() && currentViewHash != 0) {
              zColor = zoneManager->getZoneColorForKey(keyCode, 0);
            }

            if (zColor.has_value())
              underlayColor = zColor.value();
          } else if (simResult.action.has_value()) {
            // Manual Mapping: use type color
            underlayColor =
                settingsManager->getTypeColor(simResult.action->type);
          }

          // 2. Determine Alpha / Style
          if (simResult.state == VisualState::Conflict) {
            // Phase 39.7/39.8: Conflict state (zone overlaps or Manual vs Zone
            // collision) Force Red - overwrites base color
            underlayColor = juce::Colours::red;
            borderColor = juce::Colours::red;
            alpha = 1.0f;
            borderWidth = 2.0f;
          } else if (simResult.state == VisualState::Inherited) {
            alpha = 0.3f; // Dim for inherited
            borderColor = juce::Colours::darkgrey;
            borderWidth = 1.0f;
          } else if (simResult.state == VisualState::Override) {
            alpha = 1.0f;
            borderColor = juce::Colours::orange; // Highlight override
            borderWidth = 2.0f;
          } else if (simResult.state == VisualState::Active) {
            alpha = 1.0f;
            borderColor = juce::Colours::lightgrey;
            borderWidth = 1.0f;
          }

          // Apply Alpha
          underlayColor = underlayColor.withAlpha(alpha);
        }
      } else {
        // Fallback for non-Studio Mode (use old logic)
        int zoneCount = zoneManager->getZoneCountForKey(keyCode);
        bool hasManual =
            inputProcessor && inputProcessor->hasManualMappingForKey(keyCode);
        bool isConflict = (zoneCount >= 2) || (zoneCount >= 1 && hasManual);

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
            }
          }
        }
      }

      // C. Text/Label - Use simResult which already has correct hierarchy
      juce::String labelText = geometry.label;

      if (simResult.action.has_value()) {
        if (simResult.action->type == ActionType::Note) {
          labelText =
              MidiNoteUtilities::getMidiNoteName(simResult.action->data1);
        }
      } else {
        // Fallback: Try to find zone label if no action found
        // Check current view hash first, then global
        std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
        if (currentViewHash != 0) {
          zoneInfo = findZoneForKey(keyCode, currentViewHash);
        }
        if (!zoneInfo.first) {
          zoneInfo = findZoneForKey(keyCode, 0);
        }

        if (zoneInfo.first) {
          juce::String cachedLabel = zoneInfo.first->getKeyLabel(keyCode);
          if (cachedLabel.isNotEmpty()) {
            labelText = cachedLabel;
          }
        }
      }

      // --- 4. Render Static Layers (Off State) ---

      // Layer 1: Underlay (Zone Color) - Phase 39.5: Alpha already applied
      if (!underlayColor.isTransparent()) {
        g.setColour(underlayColor);
        g.fillRect(fullBounds);
      }

      // Layer 2: Key Body (Dark Grey - Off State)
      g.setColour(juce::Colour(0xff333333));
      g.fillRoundedRectangle(keyBounds, 6.0f);

      // Layer 3: Border (Phase 39.1: Use VisualState)
      g.setColour(borderColor);
      g.drawRoundedRectangle(keyBounds, 6.0f, borderWidth);

      // Layer 4: Text (white for normal/override, red only for conflicts)
      juce::Colour textColor = (simResult.state == VisualState::Conflict)
                                   ? juce::Colours::red
                                   : juce::Colours::white;
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
  // SAFETY: Don't draw or touch cache when bounds are empty
  if (getWidth() <= 0 || getHeight() <= 0)
    return;
  if (!zoneManager)
    return;

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

  // Update Sustain Indicator (dynamic - always redraw since it changes
  // frequently)
  bool sustainActive = voiceManager.isSustainActive();
  auto headerRect = juce::Rectangle<int>(0, 0, getWidth(), 30);
  juce::Colour sustainColor =
      sustainActive ? juce::Colours::lime : juce::Colours::grey;
  int indicatorSize = 12;
  int indicatorX = headerRect.getRight() - 100;
  int indicatorY = headerRect.getCentreY() - indicatorSize / 2;
  g.setColour(juce::Colour(0xff222222));
  g.fillRect(indicatorX - 5, indicatorY - 2, 80,
             indicatorSize + 4); // Clear area
  g.setColour(sustainColor);
  g.fillEllipse(indicatorX, indicatorY, indicatorSize, indicatorSize);
  g.setColour(juce::Colours::white);
  g.setFont(12.0f);
  g.drawText("SUSTAIN", indicatorX + indicatorSize + 5, indicatorY, 60,
             indicatorSize, juce::Justification::centredLeft, false);

  // Phase 45: Active Layers HUD (uses InputProcessor state via ReadLock)
  if (inputProcessor) {
    juce::StringArray activeLayers = inputProcessor->getActiveLayerNames();
    if (activeLayers.size() > 0) {
      juce::String joined = activeLayers.joinIntoString(" | ");
      g.setColour(juce::Colours::cyan);
      g.setFont(12.0f);
      auto layersBounds =
          headerRect.withLeft(300).reduced(4, 4); // to the right of TRANSPOSE
      g.drawFittedText("LAYERS: " + joined, layersBounds,
                       juce::Justification::centredLeft, 1);
    }
  }

  // Check if MIDI mode is disabled and draw overlay
  bool midiModeDisabled =
      settingsManager && !settingsManager->isMidiModeActive();

  // --- Calculate Dynamic Scale (Same as refreshCache) ---
  float unitsWide = 23.0f;
  float unitsTall = 7.3f;
  float headerHeight = 30.0f;
  float availableHeight = static_cast<float>(getHeight()) - headerHeight;

  float scaleX = static_cast<float>(getWidth()) / unitsWide;
  float scaleY =
      (availableHeight > 0.0f) ? (availableHeight / unitsTall) : scaleX;
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
    if (it == layout.end())
      continue;

    const auto &geometry = it->second;

    // Calculate Position (same as refreshCache)
    float rowOffset =
        (geometry.row == -1) ? -1.2f : static_cast<float>(geometry.row);
    float x = startX + (geometry.col * keySize);
    float y = startY + (rowOffset * keySize * 1.2f);
    float w = geometry.width * keySize;
    float h = geometry.height * keySize;

    juce::Rectangle<float> fullBounds(x, y, w, h);
    float padding = keySize * 0.1f;
    auto keyBounds = fullBounds.reduced(padding);

    // Get key state
    bool isPressed =
        (activeKeysSnapshot.find(keyCode) != activeKeysSnapshot.end());
    bool isLatched = voiceManager.isKeyLatched(keyCode);

    // Get simulation result for dynamic rendering (Phase 39.1)
    SimulationResult dynamicSimResult;
    if (inputProcessor && settingsManager) {
      // Phase 9.5: When Studio Mode is OFF, simulateInput will force
      // viewDeviceHash to 0
      dynamicSimResult = inputProcessor->simulateInput(currentViewHash, keyCode,
                                                       currentVisualizedLayer);
    }

    // Get label text - Use dynamicSimResult which already has correct hierarchy
    juce::String labelText = geometry.label;

    if (dynamicSimResult.action.has_value()) {
      if (dynamicSimResult.action->type == ActionType::Note) {
        labelText =
            MidiNoteUtilities::getMidiNoteName(dynamicSimResult.action->data1);
      }
    } else {
      // Fallback: Try to find zone label if no action found
      // Check current view hash first, then global
      std::pair<std::shared_ptr<Zone>, bool> zoneInfo{nullptr, false};
      if (currentViewHash != 0) {
        zoneInfo = findZoneForKey(keyCode, currentViewHash);
      }
      if (!zoneInfo.first) {
        zoneInfo = findZoneForKey(keyCode, 0);
      }

      if (zoneInfo.first) {
        juce::String cachedLabel = zoneInfo.first->getKeyLabel(keyCode);
        if (cachedLabel.isNotEmpty()) {
          labelText = cachedLabel;
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

    // Redraw Border (Phase 39.1: Use VisualState)
    juce::Colour dynamicBorderColor = juce::Colours::grey;
    float dynamicBorderWidth = 1.0f;
    if (inputProcessor && settingsManager) {
      if (dynamicSimResult.state == VisualState::Override) {
        dynamicBorderColor = juce::Colours::orange;
        dynamicBorderWidth = 2.0f;
      } else if (dynamicSimResult.state == VisualState::Active) {
        dynamicBorderColor = juce::Colours::lightgrey;
        dynamicBorderWidth = 1.0f;
      } else if (dynamicSimResult.state == VisualState::Inherited) {
        dynamicBorderColor = juce::Colours::darkgrey;
        dynamicBorderWidth = 1.0f;
      }
    }
    g.setColour(dynamicBorderColor);
    g.drawRoundedRectangle(keyBounds, 6.0f, dynamicBorderWidth);

    // Redraw Text (Important: text must be on top of highlight)
    juce::Colour textColor;
    if (dynamicSimResult.state == VisualState::Override) {
      textColor = juce::Colours::red;
    } else if (isPressed) {
      textColor = juce::Colours::black;
    } else {
      textColor = juce::Colours::white;
    }
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
    juce::String toggleKeyName =
        settingsManager
            ? RawInputManager::getKeyName(settingsManager->getToggleKey())
            : "Scroll Lock";
    juce::String message =
        "MIDI MODE DISABLED\n(Press " + toggleKeyName + " to enable)";
    g.drawText(message, getLocalBounds(), juce::Justification::centred, false);
  }
}

void VisualizerComponent::resized() {
  // Position View Selector (Phase 39) - Below status bar
  if (viewSelector.isVisible()) {
    int headerHeight = 30; // Status bar height
    int selectorWidth = 200;
    int selectorHeight = 25;
    int margin = 10;
    int selectorY = headerHeight + margin; // Position below status bar
    viewSelector.setBounds(getWidth() - selectorWidth - margin, selectorY,
                           selectorWidth, selectorHeight);
  }

  cacheValid = false; // Invalidate cache on resize
  needsRepaint = true;
  repaint(); // Resize needs immediate repaint
}

void VisualizerComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                            bool isDown) {
  juce::ScopedLock lock(keyStateLock);
  if (isDown)
    activeKeys.insert(keyCode);
  else
    activeKeys.erase(keyCode);
  needsRepaint = true;
}

void VisualizerComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                          float value) {
  // Ignore axis events
}

void VisualizerComponent::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == zoneManager || source == settingsManager) {
    cacheValid = false;
    needsRepaint = true;
    // Update view selector visibility based on Studio Mode (Phase 9.5)
    if (source == settingsManager && settingsManager) {
      bool studioMode = settingsManager->isStudioMode();
      viewSelector.setVisible(studioMode);
      updateViewSelector();
      resized();
      viewSelector.toFront(false);
    }
  } else if (source == deviceManager) {
    // Device alias configuration changed, refresh view selector
    updateViewSelector();
  }
}

void VisualizerComponent::updateViewSelector() {
  // Phase 9.5: Studio Mode Check
  if (settingsManager && !settingsManager->isStudioMode()) {
    // Studio Mode OFF: Only show Global, disable selector
    viewSelector.clear(juce::dontSendNotification);
    viewHashes.clear();
    viewSelector.addItem("Global (All Devices)", 1);
    viewHashes.push_back(0);
    viewSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    viewSelector.setEnabled(false);
    currentViewHash = 0;
    return;
  }

  // Studio Mode ON: Enable selector and populate normally
  viewSelector.setEnabled(true);

  // Phase 39.2: Save current selection using index
  uintptr_t currentHash = 0;
  int currentIndex = viewSelector.getSelectedItemIndex();
  if (currentIndex >= 0 && currentIndex < static_cast<int>(viewHashes.size())) {
    currentHash = viewHashes[currentIndex];
  }

  // Clear
  viewSelector.clear(juce::dontSendNotification);
  viewHashes.clear();

  if (deviceManager == nullptr) {
    // Add only Global option
    viewSelector.addItem("Global (All Devices)", 1);
    viewHashes.push_back(0);
    viewSelector.setSelectedItemIndex(0, juce::dontSendNotification);
    currentViewHash = 0;
    return;
  }

  // Add "Global (All Devices)" option (Index 0)
  viewSelector.addItem("Global (All Devices)", 1);
  viewHashes.push_back(0);

  // Add all aliases (Phase 39.2: Store full 64-bit hashes)
  auto aliases = deviceManager->getAllAliasNames();
  for (int i = 0; i < aliases.size(); ++i) {
    uintptr_t aliasHash = aliasNameToHash(aliases[i]);
    viewSelector.addItem(aliases[i], i + 2); // Start IDs from 2
    viewHashes.push_back(aliasHash);
  }

  // Restore selection if possible (Phase 39.2: Use index-based lookup)
  int restoreIndex = 0;
  for (size_t i = 0; i < viewHashes.size(); ++i) {
    if (viewHashes[i] == currentHash) {
      restoreIndex = static_cast<int>(i);
      break;
    }
  }
  viewSelector.setSelectedItemIndex(restoreIndex, juce::dontSendNotification);
  currentViewHash = viewHashes[restoreIndex];
}

void VisualizerComponent::onViewSelectorChanged() {
  // Phase 39.2: Use index-based lookup to get full 64-bit hash
  int index = viewSelector.getSelectedItemIndex();
  if (index >= 0 && index < static_cast<int>(viewHashes.size())) {
    currentViewHash = viewHashes[index];
  } else {
    currentViewHash = 0;
  }

  // Invalidate cache and repaint
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int indexFromWhichChildWasRemoved) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreePropertyChanged(
    juce::ValueTree &treeWhosePropertyHasChanged,
    const juce::Identifier &property) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

void VisualizerComponent::valueTreeParentChanged(
    juce::ValueTree &treeWhoseParentHasChanged) {
  if (!presetManager || presetManager->getIsLoading())
    return;
  cacheValid = false;
  needsRepaint = true;
}

bool VisualizerComponent::isKeyInAnyZone(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return false;

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(),
                        keyCode);
    if (it != zone->inputKeyCodes.end()) {
      return true;
    }
  }

  return false;
}

std::pair<std::shared_ptr<Zone>, bool>
VisualizerComponent::findZoneForKey(int keyCode, uintptr_t aliasHash) {
  if (!zoneManager)
    return {nullptr, false};

  const auto zones = zoneManager->getZones();
  for (const auto &zone : zones) {
    if (zone->targetAliasHash != aliasHash)
      continue;

    // Check if key is in this zone's inputKeyCodes
    auto it = std::find(zone->inputKeyCodes.begin(), zone->inputKeyCodes.end(),
                        keyCode);
    if (it != zone->inputKeyCodes.end()) {
      // Calculate the degree (same logic as Zone::processKey)
      int degree = 0;

      if (zone->layoutStrategy == Zone::LayoutStrategy::Linear) {
        // Linear mode: Use index
        int index =
            static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
        degree = index;
      } else {
        // Grid mode: Calculate based on keyboard geometry
        const auto &layout = KeyboardLayoutUtils::getLayout();

        auto currentKeyIt = layout.find(keyCode);
        if (currentKeyIt == layout.end()) {
          // Fallback to Linear if key not in layout
          int index =
              static_cast<int>(std::distance(zone->inputKeyCodes.begin(), it));
          degree = index;
        } else {
          // Get anchor key (first key in inputKeyCodes)
          if (zone->inputKeyCodes.empty()) {
            return {nullptr, false};
          }

          auto anchorKeyIt = layout.find(zone->inputKeyCodes[0]);
          if (anchorKeyIt == layout.end()) {
            // Fallback to Linear if anchor not in layout
            int index = static_cast<int>(
                std::distance(zone->inputKeyCodes.begin(), it));
            degree = index;
          } else {
            const auto &currentKey = currentKeyIt->second;
            const auto &anchorKey = anchorKeyIt->second;

            int deltaCol = static_cast<int>(currentKey.col) -
                           static_cast<int>(anchorKey.col);
            int deltaRow = currentKey.row - anchorKey.row;

            // Degree = deltaCol + (deltaRow * gridInterval)
            degree = deltaCol + (deltaRow * zone->gridInterval);
          }
        }
      }

      // Calculate effective transposition
      int effDegTrans =
          zone->isTransposeLocked ? 0 : zoneManager->getGlobalDegreeTranspose();

      // Calculate final degree (same as Zone::processKey)
      int finalDegree = degree + zone->degreeOffset + effDegTrans;

      // Check if this is the root (finalDegree == 0)
      bool isRoot = (finalDegree == 0);

      return {zone, isRoot};
    }
  }

  return {nullptr, false};
}
