#include "MainComponent.h"
#include "RawInputManager.h"

// Note: No Windows headers here. We use raw hex codes for keys.

MainComponent::MainComponent() {
  // --- MIDI Selector Setup ---
  addAndMakeVisible(midiSelector);
  midiSelector.setTextWhenNoChoicesAvailable("No MIDI Devices");
  midiSelector.addItemList(midiEngine.getDeviceNames(), 1);

  midiSelector.onChange = [this] {
    // -1 because ComboBox IDs start at 1, but our engine expects 0-indexed list
    int index = midiSelector.getSelectedItemIndex();
    midiEngine.setOutputDevice(index);

    // Log selection
    log("MIDI Output set to: " + midiSelector.getText());
  };

  // Auto-select first device if available
  if (midiSelector.getNumItems() > 0)
    midiSelector.setSelectedItemIndex(0);

  // --- Log Console Setup ---
  logConsole.setMultiLine(true);
  logConsole.setReadOnly(true);
  logConsole.setFont({"Consolas", 14.0f, 0});
  logConsole.setColour(juce::TextEditor::backgroundColourId,
                       juce::Colour(0xff202020));
  logConsole.setColour(juce::TextEditor::textColourId,
                       juce::Colours::lightgreen);
  addAndMakeVisible(logConsole);

  // --- Clear Button Setup ---
  clearButton.setButtonText("Clear Log");
  clearButton.onClick = [this] { logConsole.clear(); };
  addAndMakeVisible(clearButton);

  setSize(600, 500); // Made slightly taller for the dropdown

  // --- Raw Input Logic ---
  rawInputManager = std::make_unique<RawInputManager>();

  rawInputManager->setCallback([this](void *dev, int key, bool down) {
    // 1. Log to screen
    juce::String state = down ? "DOWN" : "UP  ";
    juce::String ptrStr = juce::String::toHexString((juce::int64)dev);
    juce::String keyName = RawInputManager::getKeyName(key);

    log("Dev: " + ptrStr + " | " + state + " | " + keyName);

    // 2. Trigger MIDI
    handleMidiTrigger(key, down);
  });

  // Start checking for window handle
  startTimer(100);
}

MainComponent::~MainComponent() {
  stopTimer();
  rawInputManager->shutdown();
}

void MainComponent::handleMidiTrigger(int keyCode, bool isDown) {
  // Hardcoded mapping for Phase 2 testing
  // 0x51 = 'Q' Key
  // 0x57 = 'W' Key

  int noteNumber = -1;

  switch (keyCode) {
  case 0x51:
    noteNumber = 60;
    break; // Middle C
  case 0x57:
    noteNumber = 62;
    break; // D
  }

  if (noteNumber != -1) {
    if (isDown)
      midiEngine.sendNoteOn(1, noteNumber, 1.0f); // Channel 1, Velocity Max
    else
      midiEngine.sendNoteOff(1, noteNumber);
  }
}

void MainComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colours::black);
}

void MainComponent::resized() {
  auto area = getLocalBounds();

  // Top: MIDI Selector
  midiSelector.setBounds(area.removeFromTop(40).reduced(5));

  // Bottom: Clear Button
  clearButton.setBounds(area.removeFromBottom(40).reduced(5));

  // Center: Log
  logConsole.setBounds(area.reduced(5));
}

void MainComponent::timerCallback() {
  if (!isInputInitialized) {
    if (auto *peer = getPeer()) {
      void *hwnd = peer->getNativeHandle();
      if (hwnd != nullptr) {
        rawInputManager->initialize(hwnd);
        isInputInitialized = true;
        log("--- SYSTEM: Raw Input Hooked Successfully ---");
        stopTimer();
      }
    }
  }
}

void MainComponent::log(const juce::String &text) {
  juce::MessageManager::callAsync([this, text]() {
    logConsole.moveCaretToEnd();
    logConsole.insertTextAtCaret(text + "\n");
  });
}