#pragma once
#include "DetachableContainer.h"
#include "DeviceManager.h"
#include "InputProcessor.h"
#include "LogComponent.h" // <--- NEW
#include "MappingEditorComponent.h"
#include "MidiEngine.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "ScaleLibrary.h"
#include "StartupManager.h"
#include "VisualizerComponent.h"
#include "ZoneEditorComponent.h"

#include <JuceHeader.h>
#include <vector>

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::ApplicationCommandTarget,
                      public RawInputManager::Listener,
                      public juce::MenuBarModel {
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
  DeviceManager
      deviceManager; // Must be before presetManager and inputProcessor
  PresetManager presetManager;
  ScaleLibrary scaleLibrary; // Must be before ZoneManager/InputProcessor
  VoiceManager voiceManager;
  InputProcessor inputProcessor;
  StartupManager startupManager; // Must be after all managers it references

  // Logic (must be before UI elements that reference it)
  RawInputManager rawInputManager;
  bool isInputInitialized = false;

  // Async logging: Input thread pushes POD only; timer processes batches
  // (Phase 21.1)
  struct PendingEvent {
    uintptr_t device;
    int keyCode;
    bool isDown;
  };
  std::vector<PendingEvent> eventQueue;
  juce::CriticalSection queueLock;

  // UI Elements (Logic components - MainComponent owns these)
  VisualizerComponent visualizer;
  juce::TabbedComponent mainTabs;
  MappingEditorComponent mappingEditor;
  ZoneEditorComponent zoneEditor;
  LogComponent logComponent; // <--- REPLACED TextEditor

  // Containers (Detachable wrappers)
  DetachableContainer visualizerContainer;
  DetachableContainer editorContainer;
  DetachableContainer logContainer;

  // Layout Managers
  juce::StretchableLayoutManager verticalLayout;
  juce::StretchableLayoutManager horizontalLayout;

  // Resizer Bars
  juce::StretchableLayoutResizerBar verticalBar;
  juce::StretchableLayoutResizerBar horizontalBar;

  juce::TextButton clearButton;
  juce::ComboBox midiSelector;
  juce::TextButton saveButton;
  juce::TextButton loadButton;
  juce::TextButton deviceSetupButton;
  juce::ToggleButton performanceModeButton;

  // Command Manager for Undo/Redo
  juce::ApplicationCommandManager commandManager;

  // Log formatting (called from timer batch only, not from input path)
  void logEvent(uintptr_t device, int keyCode, bool isDown);
  juce::String getNoteName(int noteNumber);

  // Layout persistence
  void loadLayoutPositions();
  void saveLayoutPositions();

  // MenuBarModel implementation
  juce::StringArray getMenuBarNames() override;
  juce::PopupMenu getMenuForIndex(int topLevelMenuIndex,
                                  const juce::String &menuName) override;
  void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

  // File menu items
  enum {
    FileSavePreset = 1,
    FileLoadPreset = 2,
    FileResetEverything = 4,
    FileExportVoicingReport = 5,
    FileExit = 3
  };

  // Window menu items
  enum {
    WindowShowVisualizer = 2001,
    WindowShowEditors = 2002,
    WindowShowLog = 2003
  };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};