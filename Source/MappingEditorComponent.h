#pragma once
#include "DeviceManager.h"
#include "LayerListPanel.h"
#include "MappingInspector.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include <JuceHeader.h>
#include <map>
#include <memory>

class InputCaptureOverlay;

class TouchpadMixerManager;

class MappingEditorComponent : public juce::Component,
                               public juce::TableListBoxModel,
                               public juce::ValueTree::Listener,
                               public juce::ChangeListener,
                               public RawInputManager::Listener {
public:
  MappingEditorComponent(PresetManager &pm, RawInputManager &rawInputMgr,
                         DeviceManager &deviceMgr,
                         SettingsManager &settingsMgr,
                         TouchpadMixerManager *touchpadMixerMgr = nullptr);
  ~MappingEditorComponent() override;

  // Get undo manager for command handling
  juce::UndoManager &getUndoManager() { return undoManager; }

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;
  void
  handleTouchpadContacts(uintptr_t deviceHandle,
                         const std::vector<TouchpadContact> &contacts) override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // TableListBoxModel methods
  int getNumRows() override;
  void paintRowBackground(juce::Graphics &, int rowNumber, int width,
                          int height, bool rowIsSelected) override;
  void paintCell(juce::Graphics &, int rowNumber, int columnId, int width,
                 int height, bool rowIsSelected) override;
  void selectedRowsChanged(int lastRowSelected) override;

  // ValueTree::Listener methods (Updates UI when data changes)

  // ValueTree::Listener methods
  void valueTreeChildAdded(juce::ValueTree &, juce::ValueTree &) override;
  void valueTreeChildRemoved(juce::ValueTree &, juce::ValueTree &,
                             int) override;
  void valueTreePropertyChanged(juce::ValueTree &,
                                const juce::Identifier &) override;
  void valueTreeParentChanged(juce::ValueTree &) override;

  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // Phase 45.3: notify listeners when the current layer changes
  std::function<void(int)> onLayerChanged;

private:
  // 1. Data/Managers
  PresetManager &presetManager;
  RawInputManager &rawInputManager;
  DeviceManager &deviceManager;
  SettingsManager &settingsManager;
  int selectedLayerId = 0; // Phase 41: Currently selected layer
  // Phase 45: Remember selection per layer (layerId -> row index)
  std::map<int, int> layerSelectionHistory;

  // 2. Content Components (Must live longer than containers)
  LayerListPanel layerListPanel; // Phase 41: Layer sidebar
  juce::TableListBox table;
  juce::TextButton addButton;
  juce::TextButton duplicateButton;
  juce::TextButton moveToLayerButton;
  juce::TextButton deleteButton;
  juce::ToggleButton learnButton;
  juce::UndoManager undoManager;
  MappingInspector inspector;

  // Phase 56.3: Smart Input Capture
  std::unique_ptr<InputCaptureOverlay> captureOverlay;
  bool wasMidiModeEnabledBeforeCapture = false;
  uintptr_t lastTouchpadDeviceForCapture =
      0; // Set while overlay visible for Map Touchpad
  void startInputCapture();
  void finishInputCapture(uintptr_t deviceHandle, int keyCode, bool skipped);
  void finishInputCaptureTouchpad();

  // 3. Containers (Must die first)
  juce::Viewport inspectorViewport;

  // Resizable layout for table and inspector
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar resizerBar;

  // Phase 41: Helper to get current layer's mappings
  juce::ValueTree getCurrentLayerMappings();

  void moveSelectedMappingsToLayer(int targetLayerId);

  void updateInspectorFromSelection(); // Phase 45.1: refresh inspector on
                                       // layer/row change

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingEditorComponent)
};