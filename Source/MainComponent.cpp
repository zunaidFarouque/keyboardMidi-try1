#include "MainComponent.h"

// No Windows headers needed!

MainComponent::MainComponent()
    : voiceManager(midiEngine), inputProcessor(voiceManager, presetManager),
      mappingEditor(presetManager) {
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

  // --- Editors ---
  addAndMakeVisible(mappingEditor);
  addAndMakeVisible(logComponent); // NEW LOG COMPONENT

  addAndMakeVisible(clearButton);
  clearButton.setButtonText("Clear Log");
  clearButton.onClick = [this] { logComponent.clear(); };

  setSize(800, 600);

  // --- Input Logic ---
  rawInputManager = std::make_unique<RawInputManager>();

  rawInputManager->setCallback([this](void *dev, int key, bool down) {
    // 1. Cast handle safely
    uintptr_t handle = (uintptr_t)dev;

    // 2. Log Visuals
    logEvent(handle, key, down);

    // 3. Process Logic
    InputID id = {handle, key};
    inputProcessor.processEvent(id, down);
  });

  startTimer(100);
}

MainComponent::~MainComponent() {
  stopTimer();
  rawInputManager->shutdown();
}

// --- LOGGING LOGIC ---
void MainComponent::logEvent(uintptr_t device, int keyCode, bool isDown) {
  // 1. Format Input Columns
  // "Dev: [HANDLE]"
  juce::String devStr =
      "Dev: " + juce::String::toHexString((juce::int64)device).toUpperCase();

  // "DOWN" or "UP  " (Padded)
  juce::String stateStr = isDown ? "DOWN" : "UP  ";

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
  header.removeFromLeft(10);
  saveButton.setBounds(header.removeFromLeft(100));
  header.removeFromLeft(10);
  loadButton.setBounds(header.removeFromLeft(100));

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
        rawInputManager->initialize(hwnd);
        isInputInitialized = true;
        logComponent.addEntry("--- SYSTEM: Raw Input Hooked Successfully ---");
        stopTimer();
      }
    }
  }
}