#pragma once
#include "MidiEngine.h" // <--- NEW
#include "RawInputManager.h"
#include <JuceHeader.h>


class MainComponent : public juce::Component, public juce::Timer {
public:
  MainComponent();
  ~MainComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void timerCallback() override;

private:
  std::unique_ptr<RawInputManager> rawInputManager;
  MidiEngine midiEngine; // <--- NEW: The MIDI Logic

  juce::TextEditor logConsole;
  juce::TextButton clearButton;
  juce::ComboBox midiSelector; // <--- NEW: The Dropdown

  bool isInputInitialized = false;

  void log(const juce::String &text);

  // Helper to separate MIDI logic from logging logic
  void handleMidiTrigger(int keyCode, bool isDown);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};