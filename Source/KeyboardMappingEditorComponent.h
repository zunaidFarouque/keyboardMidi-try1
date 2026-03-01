#pragma once
#include "DeviceManager.h"
#include "LayerListPanel.h"
#include "PercentageSplitLayout.h"
#include "KeyboardMappingInspector.h"
#include "MappingListPanel.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include <JuceHeader.h>
#include <map>
#include <memory>

class InputCaptureOverlay;

class TouchpadLayoutManager;
class ZoneManager;

class KeyboardMappingEditorComponent : public juce::Component,
                               public juce::TableListBoxModel,
                               public juce::ValueTree::Listener,
                               public juce::ChangeListener,
                               public RawInputManager::Listener {
public:
  KeyboardMappingEditorComponent(PresetManager &pm, RawInputManager &rawInputMgr,
                         DeviceManager &deviceMgr,
                         SettingsManager &settingsMgr,
                         TouchpadLayoutManager *touchpadLayoutMgr = nullptr,
                         ZoneManager *zoneMgr = nullptr);
  ~KeyboardMappingEditorComponent() override;

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

  /// Currently selected layer in the Mappings tab (for visualizer / tab switch).
  int getSelectedLayerId() const { return selectedLayerId; }

  // UI state persistence
  void saveUiState(SettingsManager &settings) const;
  void loadUiState(SettingsManager &settings);

private:
  // 1. Data/Managers
  PresetManager &presetManager;
  RawInputManager &rawInputManager;
  DeviceManager &deviceManager;
  SettingsManager &settingsManager;
  ZoneManager *zoneManager = nullptr; // Optional: clear zone keyboardGroupId when group removed
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
  KeyboardMappingInspector inspector;
  MappingListPanel mappingListPanel;

  // Phase 56.3: Smart Input Capture
  std::unique_ptr<InputCaptureOverlay> captureOverlay;
  bool wasMidiModeEnabledBeforeCapture = false;
  void startInputCapture();
  void finishInputCapture(uintptr_t deviceHandle, int keyCode, bool skipped);

  // 3. Containers (Must die first)
  juce::Viewport inspectorViewport;

  // Percentage-based layout: only the dragged divider moves
  PercentageResizerBar layerResizerBar;
  PercentageResizerBar resizerBar;
  float divider1Fraction = 0.15f;  // Layer | table
  float divider2Fraction = 0.55f;  // Table | inspector

  // Phase 41: Helper to get current layer's mappings
  juce::ValueTree getCurrentLayerMappings();

  // Mappings tab shows only non-touchpad mappings (row index -> filtered list)
  int getNonTouchpadMappingCount() const;
  juce::ValueTree getMappingAtRow(int row) const;
  int rowToChildIndex(int row) const;

  void moveSelectedMappingsToLayer(int targetLayerId);

  void updateInspectorFromSelection(); // Phase 45.1: refresh inspector on
                                       // layer/row change

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyboardMappingEditorComponent)
};