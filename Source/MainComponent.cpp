#include "MainComponent.h"
#include "ChordUtilities.h"
#include "DeviceSetupComponent.h"
#include "KeyNameUtilities.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "ScaleUtilities.h"
#include "TouchpadTypes.h"
#include "Zone.h"

// Windows header needed for cursor locking and window state checks
#include <windows.h>

MainComponent::MainComponent()
    : voiceManager(midiEngine, settingsManager),
      inputProcessor(voiceManager, presetManager, deviceManager, scaleLibrary,
                     midiEngine, settingsManager, touchpadMixerManager),
      startupManager(&presetManager, &deviceManager,
                     &inputProcessor.getZoneManager(), &touchpadMixerManager,
                     &settingsManager),
      rawInputManager(std::make_unique<RawInputManager>()),
      mainTabs(juce::TabbedButtonBar::TabsAtTop),
      visualizerContainer("Visualizer", layoutPlaceholder),
      editorContainer("Mapping / Zones", mainTabs),
      logContainer("Log", layoutPlaceholder),
      verticalBar(
          &verticalLayout, 1,
          false), // false = horizontal bar for vertical layout (drag up/down)
      horizontalBar(
          &horizontalLayout, 1,
          true), // true = vertical bar for horizontal layout (drag left/right)
      setupWizard(deviceManager, *rawInputManager) {
  // --- Restore UI: create the five content components and wire into
  // containers/tabs ---
  logComponent = std::make_unique<LogComponent>();
  visualizer = std::make_unique<VisualizerComponent>(
      &inputProcessor.getZoneManager(), &deviceManager, voiceManager,
      &settingsManager, &presetManager, &inputProcessor, &scaleLibrary);
  mappingEditor = std::make_unique<MappingEditorComponent>(
      presetManager, *rawInputManager, deviceManager, settingsManager, &touchpadMixerManager);
  zoneEditor = std::make_unique<ZoneEditorComponent>(
      &inputProcessor.getZoneManager(), &deviceManager, rawInputManager.get(),
      &scaleLibrary);
  touchpadTab = std::make_unique<TouchpadTabComponent>(&touchpadMixerManager,
                                                        &settingsManager);
  touchpadTab->onSelectionChangedForVisualizer = [this](int layoutIndex,
                                                        int layerId) {
    if (visualizer) {
      visualizer->setSelectedTouchpadLayout(layoutIndex, layerId);
      if (layoutIndex >= 0)
        visualizer->setVisualizedLayer(layerId);
    }
    if (miniWindow && settingsManager.getShowTouchpadVisualizerInMiniWindow()) {
      miniWindow->setSelectedTouchpadLayout(layoutIndex, layerId);
      if (layoutIndex >= 0)
        miniWindow->setVisualizedLayer(layerId);
    }
  };
  settingsPanel = std::make_unique<SettingsPanel>(settingsManager, midiEngine,
                                                  *rawInputManager);
  settingsPanel->onResetMiniWindowPosition = [this] {
    if (miniWindow)
      miniWindow->resetToDefaultPosition();
  };

  visualizerContainer.setContent(*visualizer);
  logContainer.setContent(*logComponent);

  // Phase 45.3: keep visualizer's layer context in sync with editor selection
  mappingEditor->onLayerChanged = [this](int layerId) {
    if (visualizer)
      visualizer->setVisualizedLayer(layerId);
    if (miniWindow && settingsManager.getShowTouchpadVisualizerInMiniWindow())
      miniWindow->setVisualizedLayer(layerId);
  };

  // Mini Status Window (before init; no listener storm)
  miniWindow = std::make_unique<MiniStatusWindow>(settingsManager,
                                                   &inputProcessor);

  // Listen to SettingsManager for MIDI mode changes
  settingsManager.addChangeListener(this);
  deviceManager.addChangeListener(this);
  rebuildTouchpadHandleCache();

  // Setup Command Manager for Undo/Redo
  commandManager.registerAllCommandsForTarget(this);
  commandManager.setFirstCommandTarget(this);

  // --- Header Controls ---
  addAndMakeVisible(midiSelector);
  midiSelector.setTextWhenNoChoicesAvailable("No MIDI Devices");
  refreshMidiDeviceList(
      false); // false = don't open driver yet (deferred below)

  // Setup selection logic – onChange will call setOutputDevice when
  // selection changes (or refresh when "Refresh devices" is chosen)
  midiSelector.onChange = [this] {
    int selectedId = midiSelector.getSelectedId();
    if (selectedId == kMidiRefreshItemId) {
      refreshMidiDeviceList(true); // true = restore selection and open device
      return;
    }
    int selectedIndex = midiSelector.getSelectedItemIndex();
    if (selectedIndex >= 0) {
      midiEngine.setOutputDevice(selectedIndex);
      juce::String deviceName = midiSelector.getItemText(selectedIndex);
      settingsManager.setLastMidiDevice(deviceName);
    }
  };

  // 3. Deferred auto-connect – wait until app/window/heap are stable before
  // opening MIDI driver
  juce::Component::SafePointer<MainComponent> weakThis(this);
  juce::Timer::callAfterDelay(200, [weakThis]() {
    if (weakThis == nullptr)
      return;
    juce::String savedName = weakThis->settingsManager.getLastMidiDevice();
    int indexToSelect = 0;
    for (int i = 0; i < weakThis->midiSelector.getNumItems(); ++i) {
      if (weakThis->midiSelector.getItemText(i) == savedName) {
        indexToSelect = i;
        break;
      }
    }
    if (weakThis->midiSelector.getNumItems() > 0) {
      weakThis->midiSelector.setSelectedItemIndex(indexToSelect,
                                                  juce::sendNotificationSync);
    }
  });

  addAndMakeVisible(saveButton);
  saveButton.setButtonText("Save Preset");
  saveButton.onClick = [this] {
    auto fc = std::make_shared<juce::FileChooser>(
        "Save Preset",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.xml");

    fc->launchAsync(
        juce::FileBrowserComponent::saveMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser &chooser) {
          auto result = chooser.getResult();
          // If the user didn't cancel (file is valid)
          if (result != juce::File()) {
            presetManager.saveToFile(result,
                                     touchpadMixerManager.toValueTree());
            if (logComponent)
              logComponent->addEntry("Saved: " + result.getFileName());
          }
        });
  };

  addAndMakeVisible(loadButton);
  loadButton.setButtonText("Load Preset");
  loadButton.onClick = [this] {
    auto fc = std::make_shared<juce::FileChooser>(
        "Load Preset",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.xml");

    fc->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [this, fc](const juce::FileChooser &chooser) {
          auto result = chooser.getResult();
          if (result.exists()) {
            presetManager.loadFromFile(result);
            touchpadMixerManager.restoreFromValueTree(
                presetManager.getTouchpadMixersNode());
            if (logComponent)
              logComponent->addEntry("Loaded: " + result.getFileName());

            // Phase 9.6: Rig Health Check
            if (settingsManager.isStudioMode()) {
              // Extract all alias hashes from enabled mappings (Layer 0)
              std::vector<uintptr_t> requiredAliasHashes;
              auto list = presetManager.getEnabledMappingsForLayer(0);

              for (const auto &mapping : list) {
                juce::String aliasName =
                    mapping.getProperty("inputAlias", "").toString();

                if (!aliasName.isEmpty() &&
                    aliasName != "Global (All Devices)" &&
                    aliasName != "Any / Master" && aliasName != "Global") {
                  // Convert alias name to hash
                  uintptr_t aliasHash = static_cast<uintptr_t>(
                      std::hash<juce::String>{}(aliasName));
                  requiredAliasHashes.push_back(aliasHash);
                }
              }

              // Check for empty aliases
              juce::StringArray emptyAliases =
                  deviceManager.getEmptyAliases(requiredAliasHashes);

              if (emptyAliases.size() > 0) {
                // Start the wizard
                setupWizard.startSequence(emptyAliases);
                setupWizard.setVisible(true);
                setupWizard.toFront(false);
              }
            }
          }
        });
  };

  addAndMakeVisible(deviceSetupButton);
  deviceSetupButton.setButtonText("Device Setup");
  deviceSetupButton.setVisible(
      settingsManager.isStudioMode()); // Hide when Studio Mode is OFF
  deviceSetupButton.onClick = [this] {
    juce::DialogWindow::LaunchOptions options;
    auto *setupComponent = new DeviceSetupComponent(
        deviceManager, *rawInputManager, &presetManager);
    options.content.setOwned(setupComponent);
    options.content->setSize(600, 400);
    options.dialogTitle = "Rig Configuration";
    options.resizable = true;
    options.useNativeTitleBar = true;
    options.launchAsync();
  };

  // --- Setup Main Tabs ---
  mainTabs.addTab("Mappings", juce::Colour(0xff2a2a2a), mappingEditor.get(),
                  false);
  mainTabs.addTab("Zones", juce::Colour(0xff2a2a2a), zoneEditor.get(), false);
  mainTabs.addTab("Touchpad", juce::Colour(0xff2a2a2a), touchpadTab.get(),
                  false);
  settingsViewport.setViewedComponent(settingsPanel.get(), false);
  settingsViewport.setScrollBarsShown(true, false); // vertical scrollbar only
  mainTabs.addTab("Settings", juce::Colour(0xff2a2a2a), &settingsViewport,
                  false);

  static constexpr int kTouchpadTabIndex = 2; // Mappings=0, Zones=1, Touchpad=2, Settings=3
  mainTabs.getTabbedButtonBar().addChangeListener(this);
  if (visualizer)
    visualizer->setTouchpadTabActive(false); // Default: Mappings tab
  visualizer->onTouchpadViewChanged = [this](int layerId, int layoutIndex) {
    if (miniWindow && settingsManager.getShowTouchpadVisualizerInMiniWindow()) {
      miniWindow->setVisualizedLayer(layerId);
      miniWindow->setSelectedTouchpadLayout(layoutIndex, layerId);
    }
  };

  // --- Add Containers ---
  addAndMakeVisible(visualizerContainer);
  addAndMakeVisible(editorContainer);
  addAndMakeVisible(logContainer);

  // --- Add Quick Setup Wizard (Phase 9.6) ---
  addAndMakeVisible(setupWizard);
  setupWizard.setVisible(false); // Hidden by default

  // --- Add Resizer Bars ---
  addAndMakeVisible(verticalBar);
  addAndMakeVisible(horizontalBar);

  // --- Setup Layout Managers ---
  // Vertical: Visualizer (150-300px) | Bar | Bottom Area (rest)
  verticalLayout.setItemLayout(0, 150, 300, 200); // Visualizer
  verticalLayout.setItemLayout(1, 4, 4, 4);       // Resizer bar
  verticalLayout.setItemLayout(2, -0.1, -1.0,
                               -0.6); // Bottom area (stretchable)

  // Horizontal: Editors | Bar | Log
  horizontalLayout.setItemLayout(
      0, -0.1, -0.9,
      -0.7); // Item 0 (Left/Editors): Min 10%, Max 90%, Preferred 70%
  horizontalLayout.setItemLayout(1, 5, 5, 5); // Item 1 (Bar): Fixed 5px width
  horizontalLayout.setItemLayout(
      2, -0.1, -0.9,
      -0.3); // Item 2 (Right/Log): Min 10%, Max 90%, Preferred 30%

  // --- Log Controls ---
  addAndMakeVisible(clearButton);
  clearButton.setButtonText("Clear Log");
  clearButton.onClick = [this] {
    if (logComponent)
      logComponent->clear();
  };

  addAndMakeVisible(performanceModeButton);
  updatePerformanceModeButtonText();
  performanceModeButton.setClickingTogglesState(true);
  performanceModeButton.onClick = [this] {
    bool enabled = performanceModeButton.getToggleState();
    if (enabled) {
      // Performance mode ON implies MIDI mode ON
      settingsManager.setMidiModeActive(true);
      // Smart Locking: Only lock if preset has pointer mappings
      if (!inputProcessor.hasPointerMappings()) {
        performanceModeButton.setToggleState(false, juce::dontSendNotification);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Performance Mode",
            "No Trackpad mappings found in this preset.\n\n"
            "Add Trackpad X or Y mappings, or a Touchpad layout, to use "
            "Performance Mode.");
        return;
      }

      // Show mini window so it's available for cursor clip (even if main window
      // is open)
      if (miniWindow) {
        miniWindow->setVisible(true);
      }

      // Lock cursor to mini window (with margin to avoid resize handles)
      // Cursor remains visible
      if (miniWindow && miniWindow->getPeer()) {
        void *hwnd = miniWindow->getPeer()->getNativeHandle();
        if (hwnd != nullptr) {
          RECT rect;
          if (GetWindowRect(static_cast<HWND>(hwnd), &rect)) {
            // Add margin to avoid resize handles near edges (40px from top,
            // 25px elsewhere)
            const int margin = 25;
            const int marginTop = 40;
            rect.left += margin;
            rect.top += marginTop;
            rect.right -= margin;
            rect.bottom -= margin;
            ClipCursor(&rect);
            if (settingsManager.getHideCursorInPerformanceMode())
              ShowCursor(FALSE);
          }
        }
      }
      updatePerformanceModeButtonText();
    } else {
      // Performance mode OFF -> performance off, MIDI stays on
      if (settingsManager.getHideCursorInPerformanceMode())
        ShowCursor(TRUE);
      ClipCursor(nullptr);
      updatePerformanceModeButtonText();
    }
  };

  setSize(800, 600);

  // --- Phase 42: SAFE INITIALIZATION SEQUENCE ---
  inputProcessor.initialize();
  mappingEditor->initialize();
  visualizer->initialize();
  settingsPanel->initialize();
  startupManager.initApp();

  // --- Input Logic ---
  rawInputManager->addListener(this);
  rawInputManager->addListener(visualizer.get());

  // Register focus target callback
  rawInputManager->setFocusTargetCallback([this]() -> void * {
    // When performance mode is on, cursor is clipped to mini window - use it
    // as focus target to avoid stealing focus back to main on every input
    if (performanceModeButton.getToggleState() && miniWindow &&
        miniWindow->getPeer()) {
      return miniWindow->getPeer()->getNativeHandle();
    }
    // Main window minimized - use mini window
    if (auto *peer = getPeer()) {
      void *hwnd = peer->getNativeHandle();
      if (hwnd != nullptr && IsIconic(static_cast<HWND>(hwnd))) {
        if (miniWindow && miniWindow->getPeer()) {
          return miniWindow->getPeer()->getNativeHandle();
        }
      }
      return hwnd;
    }
    return nullptr;
  });

  // Register device change callback for hardware hygiene
  rawInputManager->setOnDeviceChangeCallback(
      [this]() { deviceManager.validateConnectedDevices(); });

  // Note: Test zone removed - StartupManager now handles factory default zones

  startTimer(settingsManager.getWindowRefreshIntervalMs());

  // Load layout positions from DeviceManager
  loadLayoutPositions();

  // Setup visibility callbacks
  visualizerContainer.onVisibilityChanged =
      [this](DetachableContainer *container, bool hidden) { resized(); };
  editorContainer.onVisibilityChanged = [this](DetachableContainer *container,
                                               bool hidden) { resized(); };
  logContainer.onVisibilityChanged = [this](DetachableContainer *container,
                                            bool hidden) { resized(); };

  // Enable tooltips app-wide (required for setTooltip() on any component to
  // show)
  tooltipWindow = std::make_unique<juce::TooltipWindow>(this, 500);
  addChildComponent(tooltipWindow.get());
}

MainComponent::~MainComponent() {
  // 1. Stop any pending UI updates
  stopTimer();

  // 2. CRITICAL: Manually clear tabs.
  // This detaches the MappingEditor/ZoneEditor/SettingsPanel safely so the
  // TabbedComponent doesn't try to delete them (even if we set the flag to
  // false, this is safer).
  mainTabs.clearTabs();

  // 3. Close Popups
  if (miniWindow) {
    miniWindow->setVisible(false);
    miniWindow = nullptr;
  }

  // 4. Force Save
  saveLayoutPositions();
  startupManager.saveImmediate();

  // 5. Stop Input (Explicitly)
  if (rawInputManager) {
    rawInputManager->removeListener(this);
    if (visualizer)
      rawInputManager->removeListener(visualizer.get());
    rawInputManager->shutdown();
  }

  // 6. Remove listeners
  mainTabs.getTabbedButtonBar().removeChangeListener(this);
  settingsManager.removeChangeListener(this);
  deviceManager.removeChangeListener(this);

  // 7. Ensure cursor is unlocked and visible on exit
  if (performanceModeButton.getToggleState()) {
    if (settingsManager.getHideCursorInPerformanceMode())
      ShowCursor(TRUE);
    ClipCursor(nullptr);
  }
}

// --- LOGGING LOGIC (Phase 52.3: Grid-based; Phase 54.2: Studio Mode) ---
void MainComponent::logEvent(uintptr_t device, int keyCode, bool isDown) {
  if (!logComponent || !logComponent->isShowing())
    return;

  // 1. Determine Effective Device (match InputProcessor logic)
  uintptr_t effectiveDevice = device;
  if (!settingsManager.isStudioMode())
    effectiveDevice = 0;

  juce::String devStr =
      "Dev: " + juce::String::toHexString((juce::int64)device).toUpperCase();
  if (effectiveDevice == 0 && device != 0)
    devStr += " (Studio Mode OFF)";
  juce::String keyName = RawInputManager::getKeyName(keyCode);
  juce::String logLine = devStr + " | Key: " + keyName;

  auto ctx = inputProcessor.getContext();
  if (!ctx) {
    if (logComponent)
      logComponent->addEntry(logLine);
    return;
  }

  // 2. Lookup Alias Hash (for visual grid)
  juce::String aliasName = deviceManager.getAliasForHardware(effectiveDevice);
  uintptr_t viewHash = 0;
  if (aliasName.isNotEmpty() && aliasName != "Unassigned") {
    viewHash =
        static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName.trim()));
  }

  // 3. Get Active Layer
  int layer = inputProcessor.getHighestActiveLayerIndex();
  if (layer < 0)
    layer = 0;
  if (layer > 8)
    layer = 8;

  // 4. Lookup in Visual Grid using viewHash and layer
  std::shared_ptr<const VisualGrid> grid;
  auto it = ctx->visualLookup.find(viewHash);
  if (it != ctx->visualLookup.end() && layer < (int)it->second.size())
    grid = it->second[(size_t)layer];
  if (!grid) {
    auto it0 = ctx->visualLookup.find(0);
    if (it0 != ctx->visualLookup.end() && layer < (int)it0->second.size())
      grid = it0->second[(size_t)layer];
  }

  if (grid && keyCode >= 0 && keyCode < 256) {
    const auto &slot = (*grid)[(size_t)keyCode];
    if (slot.state != VisualState::Empty) {
      logLine += " -> [MIDI] " + slot.label;
      logLine += " | Source: " + slot.sourceName;
      if (slot.state == VisualState::Override)
        logLine += " [OVERRIDE]";
      else if (slot.state == VisualState::Conflict)
        logLine += " [CONFLICT]";
    }
  }

  if (logComponent)
    logComponent->addEntry(logLine);
}

// ApplicationCommandTarget implementation
void MainComponent::getAllCommands(juce::Array<juce::CommandID> &commands) {
  commands.add(juce::StandardApplicationCommandIDs::undo);
  commands.add(juce::StandardApplicationCommandIDs::redo);
}

void MainComponent::getCommandInfo(juce::CommandID commandID,
                                   juce::ApplicationCommandInfo &result) {
  if (commandID == juce::StandardApplicationCommandIDs::undo) {
    result.setInfo("Undo", "Undo last action", "Edit", 0);
    result.addDefaultKeypress('Z', juce::ModifierKeys::ctrlModifier);
    result.setActive(mappingEditor ? mappingEditor->getUndoManager().canUndo()
                                   : false);
  } else if (commandID == juce::StandardApplicationCommandIDs::redo) {
    result.setInfo("Redo", "Redo last undone action", "Edit", 0);
    result.addDefaultKeypress('Y', juce::ModifierKeys::ctrlModifier);
    result.setActive(mappingEditor ? mappingEditor->getUndoManager().canRedo()
                                   : false);
  }
}

bool MainComponent::perform(const InvocationInfo &info) {
  if (!mappingEditor)
    return false;
  if (info.commandID == juce::StandardApplicationCommandIDs::undo) {
    mappingEditor->getUndoManager().undo();
    commandManager.commandStatusChanged();
    return true;
  } else if (info.commandID == juce::StandardApplicationCommandIDs::redo) {
    mappingEditor->getUndoManager().redo();
    commandManager.commandStatusChanged();
    return true;
  }
  return false;
}

juce::ApplicationCommandTarget *MainComponent::getNextCommandTarget() {
  return nullptr;
}

void MainComponent::updatePerformanceModeButtonText() {
  int perfKey = settingsManager.getPerformanceModeKey();
  juce::String keyName = RawInputManager::getKeyName(perfKey);
  if (performanceModeButton.getToggleState()) {
    performanceModeButton.setButtonText("Performance Mode ON (" + keyName +
                                        ")");
  } else {
    performanceModeButton.setButtonText("Performance Mode (" + keyName + ")");
  }
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  static constexpr int kTouchpadTabIndex = 2; // Mappings=0, Zones=1, Touchpad=2, Settings=3
  if (source == &mainTabs.getTabbedButtonBar()) {
    int idx = mainTabs.getCurrentTabIndex();
    bool touchpadActive = (idx == kTouchpadTabIndex);
    if (visualizer)
      visualizer->setTouchpadTabActive(touchpadActive);
    if (touchpadActive && touchpadTab) {
      touchpadTab->refreshVisualizerSelection();
    } else if (visualizer) {
      int activeLayer = inputProcessor.getHighestActiveLayerIndex();
      visualizer->setVisualizedLayer(activeLayer);
      visualizer->setSelectedTouchpadLayout(-1, 0);
      if (miniWindow && settingsManager.getShowTouchpadVisualizerInMiniWindow()) {
        miniWindow->setVisualizedLayer(activeLayer);
        miniWindow->setSelectedTouchpadLayout(-1, activeLayer);
      }
    }
  } else if (source == &settingsManager) {
    // Handle MIDI mode changes
    if (!settingsManager.isMidiModeActive()) {
      // MIDI mode turned off -> go to standard: hide mini window, turn off
      // performance mode, unclip cursor
      if (miniWindow) {
        miniWindow->setVisible(false);
      }
      if (performanceModeButton.getToggleState()) {
        performanceModeButton.setToggleState(false,
                                             juce::dontSendNotification);
        if (settingsManager.getHideCursorInPerformanceMode())
          ShowCursor(TRUE);
        ClipCursor(nullptr);
        updatePerformanceModeButtonText();
      }
    } else {
      // MIDI mode turned on - show mini window if main window is minimized
      if (miniWindow) {
        if (auto *peer = getPeer()) {
          void *hwnd = peer->getNativeHandle();
          if (hwnd != nullptr && IsIconic(static_cast<HWND>(hwnd))) {
            miniWindow->setVisible(true);
          }
        }
      }
    }

    // Handle Studio Mode changes - update Device Setup button visibility
    deviceSetupButton.setVisible(settingsManager.isStudioMode());
    resized(); // Trigger re-layout of header

    // Apply window refresh rate (cap 30 FPS vs uncapped)
    stopTimer();
    startTimer(settingsManager.getWindowRefreshIntervalMs());
    if (visualizer)
      visualizer->restartTimerWithInterval(
          settingsManager.getWindowRefreshIntervalMs());
  } else if (source == &deviceManager) {
    rebuildTouchpadHandleCache();
  }
}

void MainComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                      bool isDown) {
  // Check for toggle key press (must be checked before other processing)
  if (isDown && keyCode == settingsManager.getToggleKey()) {
    settingsManager.setMidiModeActive(!settingsManager.isMidiModeActive());
    return; // Don't process this key for MIDI
  }

  // Check for Performance Mode shortcut key
  if (isDown && keyCode == settingsManager.getPerformanceModeKey()) {
    bool currentState = performanceModeButton.getToggleState();
    performanceModeButton.setToggleState(!currentState,
                                         juce::dontSendNotification);
    // Trigger the onClick handler logic
    bool enabled = !currentState;
    if (enabled) {
      // Performance mode ON implies MIDI mode ON
      settingsManager.setMidiModeActive(true);
      // Smart Locking: Only lock if preset has pointer mappings
      if (!inputProcessor.hasPointerMappings()) {
        performanceModeButton.setToggleState(false, juce::dontSendNotification);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Performance Mode",
            "No Trackpad mappings found in this preset.\n\n"
            "Add Trackpad X or Y mappings, or a Touchpad layout, to use "
            "Performance Mode.");
        return;
      }

      // Show mini window so it's available for cursor clip (even if main window
      // is open)
      if (miniWindow) {
        miniWindow->setVisible(true);
      }

      // Lock cursor to mini window (with margin to avoid resize handles)
      // Cursor remains visible
      if (miniWindow && miniWindow->getPeer()) {
        void *hwnd = miniWindow->getPeer()->getNativeHandle();
        if (hwnd != nullptr) {
          RECT rect;
          if (GetWindowRect(static_cast<HWND>(hwnd), &rect)) {
            // Add margin to avoid resize handles near edges (40px from top,
            // 25px elsewhere)
            const int margin = 25;
            const int marginTop = 40;
            rect.left += margin;
            rect.top += marginTop;
            rect.right -= margin;
            rect.bottom -= margin;
            ClipCursor(&rect);
            if (settingsManager.getHideCursorInPerformanceMode())
              ShowCursor(FALSE);
          }
        }
      }
      updatePerformanceModeButtonText();
    } else {
      // Performance mode OFF -> performance off, MIDI stays on
      if (settingsManager.getHideCursorInPerformanceMode())
        ShowCursor(TRUE);
      ClipCursor(nullptr);
      updatePerformanceModeButtonText();
    }
    return; // Don't process this key for MIDI
  }

  // Safety: Check for Escape key to unlock cursor
  if (isDown && keyCode == VK_ESCAPE &&
      performanceModeButton.getToggleState()) {
    // Performance mode OFF -> performance off, MIDI stays on
    performanceModeButton.setToggleState(false, juce::dontSendNotification);
    if (settingsManager.getHideCursorInPerformanceMode())
      ShowCursor(TRUE);
    ClipCursor(nullptr);
    updatePerformanceModeButtonText();
    return;
  }

  // Check if this is a scroll event
  bool isScrollEvent =
      (keyCode == InputTypes::ScrollUp || keyCode == InputTypes::ScrollDown);

  // For scroll events, only process and queue log if mapping exists
  if (isScrollEvent) {
    InputID id = {deviceHandle, keyCode};
    auto actionOpt = inputProcessor.getMappingForInput(id);
    if (actionOpt.has_value()) {
      {
        juce::ScopedLock lock(queueLock);
        eventQueue.push_back({deviceHandle, keyCode, isDown});
      }
      inputProcessor.processEvent(id, isDown);
    }
    return;
  }

  // For regular keys: push to log queue (no string formatting here), then
  // process MIDI
  {
    juce::ScopedLock lock(queueLock);
    eventQueue.push_back({deviceHandle, keyCode, isDown});
  }
  InputID id = {deviceHandle, keyCode};
  inputProcessor.processEvent(id, isDown);
}

void MainComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                    float value) {
  if (logComponent && logComponent->isShowing()) {
    juce::String devStr =
        "Dev: " +
        juce::String::toHexString((juce::int64)deviceHandle).toUpperCase();
    juce::String keyName = KeyNameUtilities::getKeyName(inputCode);
    juce::String keyInfo = "(" +
                           juce::String::toHexString(inputCode).toUpperCase() +
                           ") " + keyName;
    keyInfo = keyInfo.paddedRight(' ', 20);

    juce::String logLine =
        devStr + " | VAL  | " + keyInfo + " | val: " + juce::String(value, 3);

    InputID id = {deviceHandle, inputCode};
    auto actionOpt = inputProcessor.getMappingForInput(id);
    if (actionOpt.has_value()) {
      const auto &action = *actionOpt;
      if (action.type == ActionType::Expression &&
          action.adsrSettings.target == AdsrTarget::CC) {
        logLine += " -> [MIDI] CC " +
                   juce::String(action.adsrSettings.ccNumber) +
                   " | ch: " + juce::String(action.channel);
      }
    }

    logComponent->addEntry(logLine);
  }

  inputProcessor.handleAxisEvent(deviceHandle, inputCode, value);
}

void MainComponent::handleTouchpadContacts(
    uintptr_t deviceHandle, const std::vector<TouchpadContact> &contacts) {
  // Process when device is in "Touchpad" alias, or when we have touchpad
  // touchpad layouts (so MIDI is generated even without assigning Touchpad alias).
  if (cachedTouchpadHandles.count(deviceHandle) == 0 &&
      !inputProcessor.hasTouchpadLayouts())
    return;
  inputProcessor.processTouchpadContacts(deviceHandle, contacts);
  if (miniWindow && settingsManager.getShowTouchpadVisualizerInMiniWindow()) {
    int throttleMs = settingsManager.getWindowRefreshIntervalMs();
    int64_t now = juce::Time::getMillisecondCounter();
    bool throttleOk = (now - lastMiniWindowTouchpadUpdateMs >= throttleMs);
    bool liftDetected = touchpadContactsHaveLift(lastMiniWindowContacts_, contacts);
    if (throttleOk || liftDetected) {
      lastMiniWindowTouchpadUpdateMs = now;
      lastMiniWindowContacts_ = contacts;
      auto contactsCopy = contacts;
      auto deviceHandleCopy = deviceHandle;
      juce::Component::SafePointer<MainComponent> weakThis(this);
      juce::MessageManager::callAsync(
          [weakThis, contactsCopy, deviceHandleCopy]() {
            if (weakThis != nullptr && weakThis->miniWindow)
              weakThis->miniWindow->updateTouchpadContacts(contactsCopy,
                                                         deviceHandleCopy);
          });
    }
  }
}

void MainComponent::rebuildTouchpadHandleCache() {
  cachedTouchpadHandles.clear();
  auto ids = deviceManager.getHardwareForAlias("Touchpad");
  for (int i = 0; i < ids.size(); ++i)
    cachedTouchpadHandles.insert(ids[i]);
}

juce::String MainComponent::getNoteName(int noteNumber) {
  // Simple converter: 60 -> C4
  const char *notes[] = {"C",  "C#", "D",  "D#", "E",  "F",
                         "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (noteNumber / 12) - 1; // MIDI standard: 60 is C4
  int noteIndex = noteNumber % 12;

  return juce::String(notes[noteIndex]) + " " + juce::String(octave);
}

void MainComponent::refreshMidiDeviceList(bool triggerConnection) {
  juce::String savedName = settingsManager.getLastMidiDevice();

  midiSelector.clear();
  juce::StringArray names = midiEngine.getDeviceNames();
  int numDevices = names.size();
  if (numDevices > 0) {
    midiSelector.addItemList(names, 1);
  }
  midiSelector.addSeparator();
  midiSelector.addItem("Refresh MIDI device list", kMidiRefreshItemId);

  int indexToSelect = 0;
  for (int i = 0; i < numDevices; ++i) {
    if (names[i] == savedName) {
      indexToSelect = i;
      break;
    }
  }
  if (numDevices > 0) {
    auto notif = triggerConnection ? juce::sendNotificationSync
                                   : juce::dontSendNotification;
    midiSelector.setSelectedItemIndex(indexToSelect, notif);
  }
}

void MainComponent::loadLayoutPositions() {
  // Load vertical split position from DeviceManager
  // For now, use default positions; can be extended to load from config
  // verticalLayout.setItemPosition(0, 200); // Example
}

void MainComponent::saveLayoutPositions() {
  // Save vertical split position to DeviceManager config
  // Can be extended to save to globalConfig
  // int position = verticalLayout.getItemCurrentPosition(0);
}

juce::StringArray MainComponent::getMenuBarNames() {
  return {"File", "Edit", "Window", "Help"};
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex,
                                               const juce::String &menuName) {
  juce::PopupMenu result;

  if (topLevelMenuIndex == 0) {
    // File menu
    result.addItem(FileSavePreset, "Save Preset");
    result.addItem(FileLoadPreset, "Load Preset");
    result.addSeparator();
    result.addItem(FileResetEverything, "Reset Everything");
    result.addItem(FileExportVoicingReport, "Export Voicing Report");
    result.addSeparator();
    result.addItem(FileExit, "Exit");
  } else if (topLevelMenuIndex == 1) {
    // Edit menu (placeholder)
    result.addCommandItem(&commandManager,
                          juce::StandardApplicationCommandIDs::undo);
    result.addCommandItem(&commandManager,
                          juce::StandardApplicationCommandIDs::redo);
  } else if (topLevelMenuIndex == 2) {
    // Window menu - show all windows, tick when visible (not hidden)
    // Menu item shows tick when container is visible
    result.addItem(WindowShowVisualizer, "Visualizer", true,
                   !visualizerContainer.isHidden());
    result.addItem(WindowShowEditors, "Mapping / Zones", true,
                   !editorContainer.isHidden());
    result.addItem(WindowShowLog, "Event Log", true, !logContainer.isHidden());
  } else if (topLevelMenuIndex == 3) {
    // Help menu (placeholder)
    result.addItem(4, "About");
  }

  return result;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex) {
  if (topLevelMenuIndex == 0) {
    // File menu
    switch (menuItemID) {
    case FileSavePreset:
      saveButton.triggerClick();
      break;
    case FileLoadPreset:
      loadButton.triggerClick();
      break;
    case FileExportVoicingReport: {
      juce::File targetFile =
          juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
              .getChildFile("MIDIQy_Voicings.txt");
      ChordUtilities::dumpDebugReport(targetFile);
      if (logComponent)
        logComponent->addEntry("Voicing report exported to: " +
                               targetFile.getFullPathName());
      break;
    }
    case FileResetEverything:
      juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::WarningIcon, "Reset Everything",
          "This will reset all mappings and zones to factory defaults.\n\n"
          "This action cannot be undone. Continue?",
          "Reset", "Cancel", this,
          juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1) { // OK clicked
              startupManager.createFactoryDefault();
              inputProcessor.forceRebuildMappings();
              if (visualizer)
                visualizer->repaint();
              if (logComponent)
                logComponent->addEntry("Reset to factory defaults");
            }
          }));
      break;
    case FileExit:
      juce::JUCEApplication::getInstance()->systemRequestedQuit();
      break;
    }
  } else if (topLevelMenuIndex == 2) {
    // Window menu
    switch (menuItemID) {
    case WindowShowVisualizer:
      if (visualizerContainer.isHidden())
        visualizerContainer.show();
      else
        visualizerContainer.hide();
      break;
    case WindowShowEditors:
      if (editorContainer.isHidden())
        editorContainer.show();
      else
        editorContainer.hide();
      break;
    case WindowShowLog:
      if (logContainer.isHidden())
        logContainer.show();
      else
        logContainer.hide();
      break;
    }
    resized();
  } else if (topLevelMenuIndex == 3 && menuItemID == 4) {
    // Help > About
    juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon, "About MIDIQy",
        "MIDIQy\n"
        "The QWERTY Performance Engine\n\n"
        "Version 1.0.0\n"
        "By Md. Zunaid Farouque\n\n"
        "Turn the hardware you have into the instrument you need.\n\n"
        "github.com/zunaidFarouque\n"
        "Report issues & discussions on GitHub");
  }
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void MainComponent::resized() {
  auto area = getLocalBounds().reduced(4);

  // Phase 9.6: Setup wizard covers entire bounds (on top)
  setupWizard.setBounds(getLocalBounds());

  // Header (Buttons)
  auto header = area.removeFromTop(30);
  midiSelector.setBounds(header.removeFromLeft(250));
  header.removeFromLeft(4);
  saveButton.setBounds(header.removeFromLeft(100));
  header.removeFromLeft(4);
  loadButton.setBounds(header.removeFromLeft(100));
  header.removeFromLeft(4);
  deviceSetupButton.setBounds(header.removeFromLeft(110));
  header.removeFromLeft(4);
  performanceModeButton.setBounds(header.removeFromLeft(140));

  area.removeFromTop(4);

  // Use StretchableLayoutManager for vertical split: Visualizer | Bar | Bottom
  // Layout manager items: 0 = Visualizer, 1 = Bar, 2 = Bottom area
  // (placeholder)

  juce::Rectangle<int> bottomArea;

  if (!visualizerContainer.isHidden()) {
    // Include visualizer and bar in the layout (use nullptr for item 2 as
    // placeholder)
    juce::Component *verticalComps[] = {&visualizerContainer, &verticalBar,
                                        nullptr};
    verticalLayout.layOutComponents(
        verticalComps, 3, area.getX(), area.getY(), area.getWidth(),
        area.getHeight(), true,
        true); // true = vertical layout (stack top to bottom)

    // Bottom area starts after the bar
    bottomArea = area.withTop(verticalBar.getBottom());
  } else {
    // Visualizer hidden - bottom area takes full space, bar is hidden
    bottomArea = area;
  }

  // Inside bottom area, use horizontal split: Editors | Bar | Log
  juce::Array<juce::Component *> horizontalComponents;
  if (!editorContainer.isHidden())
    horizontalComponents.add(&editorContainer);
  if (!editorContainer.isHidden() && !logContainer.isHidden())
    horizontalComponents.add(&horizontalBar);
  if (!logContainer.isHidden())
    horizontalComponents.add(&logContainer);

  auto logButtonArea = bottomArea.removeFromBottom(30);
  clearButton.setBounds(logButtonArea.removeFromRight(100).reduced(2));

  if (horizontalComponents.size() > 0) {
    horizontalLayout.layOutComponents(
        horizontalComponents.getRawDataPointer(), horizontalComponents.size(),
        bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(),
        bottomArea.getHeight(), false, true);
  }

  // Viewport does not resize its viewed component: set settings panel size so
  // it paints and scrolls based on its dynamic content height.
  if (settingsPanel &&
      settingsViewport.getViewedComponent() == settingsPanel.get()) {
    int w = settingsViewport.getWidth();
    if (w > 0)
      settingsPanel->setSize(w, settingsPanel->getRequiredHeight());
  }
}

void MainComponent::timerCallback() {
  if (!isInputInitialized) {
    // Get the top-level component to ensure we have the real OS window handle
    if (auto *top = getTopLevelComponent()) {
      if (auto *peer = top->getPeer()) {
        void *hwnd = peer->getNativeHandle();
        if (hwnd != nullptr) {
          rawInputManager->initialize(hwnd, &settingsManager);
          isInputInitialized = true;
          deviceManager
              .validateConnectedDevices(); // Scan devices and re-assign
                                           // Touchpad alias if needed
          if (logComponent)
            logComponent->addEntry(
                "--- SYSTEM: Raw Input Hooked Successfully ---");
        }
      }
    }
  }

  // Main window visibility: when minimized, stop all visualization timers (zero
  // CPU)
  bool isMinimized = false;
  if (auto *top = getTopLevelComponent()) {
    if (auto *peer = top->getPeer())
      isMinimized = peer->isMinimised();
  }

  if (restoreCheckMode_) {
    // Slow poll: only restart refresh timers when window is visible again
    if (!isMinimized) {
      restoreCheckMode_ = false;
      stopTimer();
      startTimer(settingsManager.getWindowRefreshIntervalMs());
      if (visualizer)
        visualizer->restartTimerWithInterval(
            settingsManager.getWindowRefreshIntervalMs());
    }
    return;
  }
  if (isMinimized) {
    restoreCheckMode_ = true;
    stopTimer();
    if (visualizer)
      visualizer->stopTimer();
    startTimer(1000); // check once per second until restored
    return;
  }

  // 1. Swap Queue (Thread Safe)
  std::vector<PendingEvent> tempQueue;
  {
    juce::ScopedLock lock(queueLock);
    tempQueue.swap(eventQueue);
    // eventQueue is now empty, Input Thread is free to write
  }

  // 2. Process Queue (only when not minimized)
  for (const auto &ev : tempQueue) {
    logEvent(ev.device, ev.keyCode, ev.isDown);
  }
}