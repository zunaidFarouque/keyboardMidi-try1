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
#include "SettingsManager.h"
#include "SettingsPanel.h"
#include "StartupManager.h"
#include "VisualizerComponent.h"
#include "ZoneEditorComponent.h"
#include "MiniStatusWindow.h"

#include <JuceHeader.h>
#include <vector>

class MainComponent : public juce::Component,
                      public juce::Timer,
                      public juce::ApplicationCommandTarget,
                      public RawInputManager::Listener,
                      public juce::MenuBarModel,
                      public juce::ChangeListener {
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
  // 1. Core Engines & Inputs (Must die LAST)
  SettingsManager settingsManager;
  DeviceManager deviceManager;
  MidiEngine midiEngine;
  ScaleLibrary scaleLibrary;
  std::unique_ptr<RawInputManager> rawInputManager; // MOVED UP (Was #5) - Must be before UI components that reference it

  // 2. Logic Managers
  VoiceManager voiceManager; // Depends on MidiEngine, SettingsManager
  PresetManager presetManager;

  // 3. Processors
  InputProcessor inputProcessor; // Listens to Preset/Device/Zone

  // 4. Persistence
  StartupManager startupManager;

  // 5. Content Components (Must live longer than containers)
  VisualizerComponent visualizer; // Listens to RawInput/Voice/Zone
  MappingEditorComponent mappingEditor; // CONTENT for Tab
  ZoneEditorComponent zoneEditor;       // CONTENT for Tab
  SettingsPanel settingsPanel;          // CONTENT for Tab
  LogComponent logComponent;

  // 6. Containers / Wrappers (Must die first)
  juce::TabbedComponent mainTabs;
  DetachableContainer visualizerContainer;
  DetachableContainer editorContainer;
  DetachableContainer logContainer;
  bool isInputInitialized = false;
  juce::TextButton clearButton;
  juce::ComboBox midiSelector;
  juce::TextButton saveButton;
  juce::TextButton loadButton;
  juce::TextButton deviceSetupButton;
  juce::ToggleButton performanceModeButton;

  // 8. Layout
  juce::StretchableLayoutManager verticalLayout;
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar verticalBar;
  juce::StretchableLayoutResizerBar horizontalBar;

  // 9. Windows
  std::unique_ptr<MiniStatusWindow> miniWindow;

  // Async logging: Input thread pushes POD only; timer processes batches
  // (Phase 21.1)
  struct PendingEvent {
    uintptr_t device;
    int keyCode;
    bool isDown;
  };
  std::vector<PendingEvent> eventQueue;
  juce::CriticalSection queueLock;

  // Command Manager for Undo/Redo
  juce::ApplicationCommandManager commandManager;

  // Log formatting (called from timer batch only, not from input path)
  void logEvent(uintptr_t device, int keyCode, bool isDown);
  juce::String getNoteName(int noteNumber);

  // Layout persistence
  void loadLayoutPositions();
  void saveLayoutPositions();

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster* source) override;

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