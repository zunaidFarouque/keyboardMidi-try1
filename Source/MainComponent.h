#pragma once
#include "InputProcessor.h"
#include "LogComponent.h" // <--- NEW
#include "MappingEditorComponent.h"
#include "MidiEngine.h"
#include "PresetManager.h"
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
  MidiEngine midiEngine;
  PresetManager presetManager;
  VoiceManager voiceManager;
  InputProcessor inputProcessor;

  // UI Elements
  MappingEditorComponent mappingEditor;
  LogComponent logComponent; // <--- REPLACED TextEditor
  juce::TextButton clearButton;
  juce::ComboBox midiSelector;
  juce::TextButton saveButton;
  juce::TextButton loadButton;

  // Logic
  std::unique_ptr<RawInputManager> rawInputManager;
  bool isInputInitialized = false;

  // Helper to format the beautiful log string
  void logEvent(uintptr_t device, int keyCode, bool isDown);
  juce::String getNoteName(int noteNumber);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};