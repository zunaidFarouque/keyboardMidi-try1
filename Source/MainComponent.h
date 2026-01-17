#pragma once
#include "InputProcessor.h"
#include "LogComponent.h" // <--- NEW
#include "MappingEditorComponent.h"
#include "MidiEngine.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "DeviceManager.h"

#include <JuceHeader.h>

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::ApplicationCommandTarget,
                      public RawInputManager::Listener {
public:
  MainComponent();
  ~MainComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void timerCallback() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

  // ApplicationCommandTarget implementation
  void getAllCommands(juce::Array<juce::CommandID> &commands) override;
  void getCommandInfo(juce::CommandID commandID,
                      juce::ApplicationCommandInfo &result) override;
  bool perform(const InvocationInfo &info) override;
  juce::ApplicationCommandTarget *getNextCommandTarget() override;

private:
  MidiEngine midiEngine;
  DeviceManager deviceManager; // Must be before presetManager and inputProcessor
  PresetManager presetManager;
  VoiceManager voiceManager;
  InputProcessor inputProcessor;

  // Logic (must be before UI elements that reference it)
  RawInputManager rawInputManager;
  bool isInputInitialized = false;

  // UI Elements
  MappingEditorComponent mappingEditor;
  LogComponent logComponent; // <--- REPLACED TextEditor
  juce::TextButton clearButton;
  juce::ComboBox midiSelector;
  juce::TextButton saveButton;
  juce::TextButton loadButton;
  juce::TextButton deviceSetupButton;
  juce::ToggleButton performanceModeButton;

  // Command Manager for Undo/Redo
  juce::ApplicationCommandManager commandManager;

  // Helper to format the beautiful log string
  void logEvent(uintptr_t device, int keyCode, bool isDown);
  juce::String getNoteName(int noteNumber);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};