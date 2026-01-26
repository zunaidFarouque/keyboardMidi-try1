#include "MainComponent.h"
#include "ChordUtilities.h"
#include "DeviceSetupComponent.h"
#include "KeyNameUtilities.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "ScaleUtilities.h"
#include "Zone.h"

// Windows header needed for cursor locking
#include <windows.h>

MainComponent::MainComponent()
    : voiceManager(midiEngine, settingsManager),
      inputProcessor(voiceManager, presetManager, deviceManager, scaleLibrary, midiEngine, settingsManager),
      startupManager(&presetManager, &deviceManager,
                     &inputProcessor.getZoneManager(), &settingsManager),
      mappingEditor(presetManager, rawInputManager, deviceManager),
      mainTabs(juce::TabbedButtonBar::TabsAtTop),
      zoneEditor(&inputProcessor.getZoneManager(), &deviceManager,
                 &rawInputManager, &scaleLibrary),
      settingsPanel(settingsManager, midiEngine),
      visualizer(&inputProcessor.getZoneManager(), &deviceManager, voiceManager,
                 &presetManager, &inputProcessor),
      visualizerContainer("Visualizer", visualizer),
      editorContainer("Mapping / Zones", mainTabs),
      logContainer("Log", logComponent),
      verticalBar(
          &verticalLayout, 1,
          false), // false = horizontal bar for vertical layout (drag up/down)
      horizontalBar(
          &horizontalLayout, 1,
          true) { // true = vertical bar for horizontal layout (drag left/right)
  // Initialize application (load or create factory default)
  startupManager.initApp();

  // Setup Command Manager for Undo/Redo
  commandManager.registerAllCommandsForTarget(this);
  commandManager.setFirstCommandTarget(this);

  // --- Header Controls ---
  addAndMakeVisible(midiSelector);
  midiSelector.setTextWhenNoChoicesAvailable("No MIDI Devices");
  midiSelector.addItemList(midiEngine.getDeviceNames(), 1);
  midiSelector.onChange = [this] {
    midiEngine.setOutputDevice(midiSelector.getSelectedItemIndex());
  };
  if (midiSelector.getNumItems() > 0)
    midiSelector.setSelectedItemIndex(0);

  addAndMakeVisible(saveButton);
  saveButton.setButtonText("Save Preset");
  saveButton.onClick = [this] {
    auto fc = std::make_shared<juce::FileChooser>(
        "Save Preset",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.xml");

    fc->launchAsync(juce::FileBrowserComponent::saveMode |
                        juce::FileBrowserComponent::canSelectFiles,
                    [this, fc](const juce::FileChooser &chooser) {
                      auto result = chooser.getResult();
                      // If the user didn't cancel (file is valid)
                      if (result != juce::File()) {
                        presetManager.saveToFile(result);
                        logComponent.addEntry("Saved: " + result.getFileName());
                      }
                    });
  };

  addAndMakeVisible(loadButton);
  loadButton.setButtonText("Load Preset");
  loadButton.onClick = [this] {
    auto fc = std::make_shared<juce::FileChooser>(
        "Load Preset",
        juce::File::getSpecialLocation(juce::File::userHomeDirectory), "*.xml");

    fc->launchAsync(juce::FileBrowserComponent::openMode |
                        juce::FileBrowserComponent::canSelectFiles,
                    [this, fc](const juce::FileChooser &chooser) {
                      auto result = chooser.getResult();
                      if (result.exists()) {
                        presetManager.loadFromFile(result);
                        logComponent.addEntry("Loaded: " +
                                              result.getFileName());
                      }
                    });
  };

  addAndMakeVisible(deviceSetupButton);
  deviceSetupButton.setButtonText("Device Setup");
  deviceSetupButton.onClick = [this] {
    juce::DialogWindow::LaunchOptions options;
    auto *setupComponent =
        new DeviceSetupComponent(deviceManager, rawInputManager);
    options.content.setOwned(setupComponent);
    options.content->setSize(600, 400);
    options.dialogTitle = "Rig Configuration";
    options.resizable = true;
    options.useNativeTitleBar = true;
    options.launchAsync();
  };

  // --- Setup Main Tabs ---
  mainTabs.addTab("Mappings", juce::Colour(0xff2a2a2a), &mappingEditor, false);
  mainTabs.addTab("Zones", juce::Colour(0xff2a2a2a), &zoneEditor, false);
  mainTabs.addTab("Settings", juce::Colour(0xff2a2a2a), &settingsPanel, false);

  // --- Add Containers ---
  addAndMakeVisible(visualizerContainer);
  addAndMakeVisible(editorContainer);
  addAndMakeVisible(logContainer);

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
  horizontalLayout.setItemLayout(0, -0.1, -0.9, -0.7); // Item 0 (Left/Editors): Min 10%, Max 90%, Preferred 70%
  horizontalLayout.setItemLayout(1, 5, 5, 5);          // Item 1 (Bar): Fixed 5px width
  horizontalLayout.setItemLayout(2, -0.1, -0.9, -0.3); // Item 2 (Right/Log): Min 10%, Max 90%, Preferred 30%

  // --- Log Controls ---
  addAndMakeVisible(clearButton);
  clearButton.setButtonText("Clear Log");
  clearButton.onClick = [this] { logComponent.clear(); };

  addAndMakeVisible(performanceModeButton);
  performanceModeButton.setButtonText("Performance Mode");
  performanceModeButton.setClickingTogglesState(true);
  performanceModeButton.onClick = [this] {
    bool enabled = performanceModeButton.getToggleState();
    if (enabled) {
      // Smart Locking: Only lock if preset has pointer mappings
      if (!inputProcessor.hasPointerMappings()) {
        performanceModeButton.setToggleState(false, juce::dontSendNotification);
        juce::AlertWindow::showMessageBoxAsync(
            juce::AlertWindow::WarningIcon, "Performance Mode",
            "No Trackpad mappings found in this preset.\n\n"
            "Add Trackpad X or Y mappings to use Performance Mode.");
        return;
      }

      // Lock cursor: hide and clip to window
      ::ShowCursor(FALSE);
      if (auto *peer = getPeer()) {
        void *hwnd = peer->getNativeHandle();
        if (hwnd != nullptr) {
          RECT rect;
          if (GetWindowRect(static_cast<HWND>(hwnd), &rect)) {
            ClipCursor(&rect);
          }
        }
      }
      performanceModeButton.setButtonText("Unlock Cursor (Esc)");
    } else {
      // Unlock cursor: show and release clip
      ::ShowCursor(TRUE);
      ClipCursor(nullptr);
      performanceModeButton.setButtonText("Performance Mode");
    }
  };

  setSize(800, 600);

  // --- Input Logic ---
  rawInputManager.addListener(this);
  rawInputManager.addListener(&visualizer);

  // Note: Test zone removed - StartupManager now handles factory default zones

  startTimer(33); // 30 Hz for async log processing (Phase 21.1)

  // Load layout positions from DeviceManager
  loadLayoutPositions();

  // Setup visibility callbacks
  visualizerContainer.onVisibilityChanged =
      [this](DetachableContainer *container, bool hidden) { resized(); };
  editorContainer.onVisibilityChanged = [this](DetachableContainer *container,
                                               bool hidden) { resized(); };
  logContainer.onVisibilityChanged = [this](DetachableContainer *container,
                                            bool hidden) { resized(); };
}

MainComponent::~MainComponent() {
  // Save layout positions to DeviceManager
  saveLayoutPositions();

  // Save immediately on close (flush pending saves)
  startupManager.saveImmediate();

  stopTimer();
  rawInputManager.removeListener(this);
  rawInputManager.shutdown();

  // Ensure cursor is unlocked on exit
  if (performanceModeButton.getToggleState()) {
    ::ShowCursor(TRUE);
    ClipCursor(nullptr);
  }
}

// --- LOGGING LOGIC ---
void MainComponent::logEvent(uintptr_t device, int keyCode, bool isDown) {
  // 1. Format Input Columns
  // "Dev: [HANDLE]"
  juce::String devStr =
      "Dev: " + juce::String::toHexString((juce::int64)device).toUpperCase();

  // "DOWN" or "UP  " or "VAL" for axis events
  juce::String stateStr;
  if (keyCode == 0x2000 || keyCode == 0x2001) {
    stateStr = "VAL "; // Value for axis events
  } else {
    stateStr = isDown ? "DOWN" : "UP  ";
  }

  // "Key: Q"
  juce::String keyName = RawInputManager::getKeyName(keyCode);
  juce::String keyInfo = "Key: " + keyName;

  juce::String logLine = devStr + " | " + keyInfo;

  // 2. Simulate input to get action and source
  auto [action, source] = inputProcessor.simulateInput(device, keyCode);

  if (action.has_value()) {
    const auto &midiAction = action.value();
    logLine += " -> [MIDI] ";

    if (midiAction.type == ActionType::Note) {
      juce::String noteName =
          MidiNoteUtilities::getMidiNoteName(midiAction.data1);
      logLine +=
          "Note " + juce::String(midiAction.data1) + " (" + noteName + ")";
    } else if (midiAction.type == ActionType::CC) {
      logLine += "CC " + juce::String(midiAction.data1) +
                 " | val: " + juce::String(midiAction.data2);
    }

    // Add source description
    if (!source.isEmpty()) {
      logLine += " | Source: " + source;
    }
  }

  logComponent.addEntry(logLine);
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
    result.setActive(mappingEditor.getUndoManager().canUndo());
  } else if (commandID == juce::StandardApplicationCommandIDs::redo) {
    result.setInfo("Redo", "Redo last undone action", "Edit", 0);
    result.addDefaultKeypress('Y', juce::ModifierKeys::ctrlModifier);
    result.setActive(mappingEditor.getUndoManager().canRedo());
  }
}

bool MainComponent::perform(const InvocationInfo &info) {
  if (info.commandID == juce::StandardApplicationCommandIDs::undo) {
    mappingEditor.getUndoManager().undo();
    commandManager.commandStatusChanged();
    return true;
  } else if (info.commandID == juce::StandardApplicationCommandIDs::redo) {
    mappingEditor.getUndoManager().redo();
    commandManager.commandStatusChanged();
    return true;
  }
  return false;
}

juce::ApplicationCommandTarget *MainComponent::getNextCommandTarget() {
  return nullptr;
}

void MainComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                      bool isDown) {
  // Safety: Check for Escape key to unlock cursor
  if (isDown && keyCode == VK_ESCAPE &&
      performanceModeButton.getToggleState()) {
    performanceModeButton.setToggleState(false, juce::dontSendNotification);
    ::ShowCursor(TRUE);
    ClipCursor(nullptr);
    performanceModeButton.setButtonText("Performance Mode");
    return;
  }

  // Check if this is a scroll event
  bool isScrollEvent =
      (keyCode == InputTypes::ScrollUp || keyCode == InputTypes::ScrollDown);

  // For scroll events, only process and queue log if mapping exists
  if (isScrollEvent) {
    InputID id = {deviceHandle, keyCode};
    const MidiAction *action = inputProcessor.getMappingForInput(id);
    if (action != nullptr) {
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
  // Log axis events with value information
  juce::String devStr =
      "Dev: " +
      juce::String::toHexString((juce::int64)deviceHandle).toUpperCase();
  juce::String keyName = KeyNameUtilities::getKeyName(inputCode);
  juce::String keyInfo =
      "(" + juce::String::toHexString(inputCode).toUpperCase() + ") " + keyName;
  keyInfo = keyInfo.paddedRight(' ', 20);

  juce::String logLine =
      devStr + " | VAL  | " + keyInfo + " | val: " + juce::String(value, 3);

  // Check for MIDI mapping
  InputID id = {deviceHandle, inputCode};
  const MidiAction *action = inputProcessor.getMappingForInput(id);

  if (action != nullptr && action->type == ActionType::CC) {
    logLine += " -> [MIDI] CC " + juce::String(action->data1) +
               " | ch: " + juce::String(action->channel);
  }

  logComponent.addEntry(logLine);

  // Forward axis events to InputProcessor
  inputProcessor.handleAxisEvent(deviceHandle, inputCode, value);
}

juce::String MainComponent::getNoteName(int noteNumber) {
  // Simple converter: 60 -> C4
  const char *notes[] = {"C",  "C#", "D",  "D#", "E",  "F",
                         "F#", "G",  "G#", "A",  "A#", "B"};
  int octave = (noteNumber / 12) - 1; // MIDI standard: 60 is C4
  int noteIndex = noteNumber % 12;

  return juce::String(notes[noteIndex]) + " " + juce::String(octave);
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
              .getChildFile("OmniKey_Voicings.txt");
      ChordUtilities::dumpDebugReport(targetFile);
      logComponent.addEntry("Voicing report exported to: " +
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
              // Force InputProcessor to rebuild its keyMapping (ensures
              // conflict detection is accurate)
              inputProcessor.forceRebuildMappings();
              // Trigger visualizer repaint to update conflict indicators
              visualizer.repaint();
              logComponent.addEntry("Reset to factory defaults");
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
  }
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void MainComponent::resized() {
  auto area = getLocalBounds().reduced(4);

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
}

void MainComponent::timerCallback() {
  if (!isInputInitialized) {
    if (auto *peer = getPeer()) {
      void *hwnd = peer->getNativeHandle();
      if (hwnd != nullptr) {
        rawInputManager.initialize(hwnd);
        isInputInitialized = true;
        logComponent.addEntry("--- SYSTEM: Raw Input Hooked Successfully ---");
      }
    }
  }

  // Process log batch (Phase 21.1: fire-and-forget logging on timer, not input
  // thread)
  std::vector<PendingEvent> tempQueue;
  {
    juce::ScopedLock lock(queueLock);
    tempQueue.swap(eventQueue);
  }
  for (const auto &ev : tempQueue) {
    logEvent(ev.device, ev.keyCode, ev.isDown);
  }
}