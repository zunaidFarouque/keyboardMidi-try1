#include "MainComponent.h"
#include "KeyNameUtilities.h"
#include "MappingTypes.h"
#include "DeviceSetupComponent.h"

// Windows header needed for cursor locking
#include <windows.h>

MainComponent::MainComponent()
    : voiceManager(midiEngine), 
      inputProcessor(voiceManager, presetManager, deviceManager),
      mappingEditor(presetManager, rawInputManager, deviceManager) {
  // Load DeviceManager config BEFORE loading preset (if any)
  // This ensures aliases are available when preset loads
  
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
    auto* setupComponent = new DeviceSetupComponent(deviceManager, rawInputManager);
    options.content.setOwned(setupComponent);
    options.content->setSize(600, 400);
    options.dialogTitle = "Rig Configuration";
    options.resizable = true;
    options.useNativeTitleBar = true;
    options.launchAsync();
  };

  // --- Editors ---
  addAndMakeVisible(mappingEditor);
  addAndMakeVisible(logComponent); // NEW LOG COMPONENT

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

  startTimer(100);
}

MainComponent::~MainComponent() {
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

  // "( 81) Q"
  juce::String keyName = RawInputManager::getKeyName(keyCode);
  juce::String keyInfo =
      "(" + juce::String(keyCode).paddedLeft(' ', 3) + ") " + keyName;

  // Pad the Key info so arrows align
  keyInfo = keyInfo.paddedRight(' ', 12);

  juce::String logLine = devStr + " | " + stateStr + " | " + keyInfo;

  // 2. Check for MIDI Mapping (Peek into Processor)
  InputID id = {device, keyCode};
  const MidiAction *action = inputProcessor.getMappingForInput(id);

  if (action != nullptr) {
    logLine += " -> [MIDI] ";

    if (action->type == ActionType::Note) {
      juce::String noteState = isDown ? "Note On " : "Note Off";
      juce::String noteName = getNoteName(action->data1); // e.g., "C 4"
      juce::String vel = "vel: " + juce::String(action->data2);
      juce::String ch = "ch: " + juce::String(action->channel);

      // Format: "Note On | 60 (C 4) | vel: 127 | ch: 1"
      logLine += noteState + " | " + juce::String(action->data1) + " (" +
                 noteName + ") | " + vel + " | " + ch;
    } else if (action->type == ActionType::CC) {
      logLine += "CC " + juce::String(action->data1) +
                 " | val: " + juce::String(action->data2);
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
  if (isDown && keyCode == VK_ESCAPE && performanceModeButton.getToggleState()) {
    performanceModeButton.setToggleState(false, juce::dontSendNotification);
    ::ShowCursor(TRUE);
    ClipCursor(nullptr);
    performanceModeButton.setButtonText("Performance Mode");
    return;
  }

  // Check if this is a scroll event
  bool isScrollEvent = (keyCode == InputTypes::ScrollUp || keyCode == InputTypes::ScrollDown);
  
  // For scroll events, only process if there's a mapping
  if (isScrollEvent) {
    InputID id = {deviceHandle, keyCode};
    const MidiAction *action = inputProcessor.getMappingForInput(id);
    
    // Only log and process if mapping exists
    if (action != nullptr) {
      logEvent(deviceHandle, keyCode, isDown);
      inputProcessor.processEvent(id, isDown);
    }
    return;
  }

  // For regular keys, always log and process
  // 1. Log Visuals
  logEvent(deviceHandle, keyCode, isDown);

  // 2. Process Logic
  InputID id = {deviceHandle, keyCode};
  inputProcessor.processEvent(id, isDown);
}

void MainComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                    float value) {
  // Log axis events with value information
  juce::String devStr =
      "Dev: " + juce::String::toHexString((juce::int64)deviceHandle).toUpperCase();
  juce::String keyName = KeyNameUtilities::getKeyName(inputCode);
  juce::String keyInfo = "(" + juce::String::toHexString(inputCode).toUpperCase() + ") " + keyName;
  keyInfo = keyInfo.paddedRight(' ', 20);
  
  juce::String logLine = devStr + " | VAL  | " + keyInfo + " | val: " + 
                         juce::String(value, 3);
  
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

  // Split remaining vertical space
  // Top 50% = Mapping Editor
  // Bottom 50% = Log

  auto topArea = area.removeFromTop(area.getHeight() / 2);
  mappingEditor.setBounds(topArea);

  area.removeFromTop(4); // Gap

  // Log Section
  clearButton.setBounds(area.removeFromBottom(30));
  logComponent.setBounds(area);
}

void MainComponent::timerCallback() {
  if (!isInputInitialized) {
    if (auto *peer = getPeer()) {
      void *hwnd = peer->getNativeHandle();
      if (hwnd != nullptr) {
        rawInputManager.initialize(hwnd);
        isInputInitialized = true;
        logComponent.addEntry("--- SYSTEM: Raw Input Hooked Successfully ---");
        stopTimer();
      }
    }
  }
}