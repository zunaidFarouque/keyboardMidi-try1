#pragma once
#include "DeviceManager.h"
#include "LayerListPanel.h"
#include "MappingInspector.h"
#include "PresetManager.h"
#include "RawInputManager.h"
#include <JuceHeader.h>
#include <map>

class MappingEditorComponent : public juce::Component,
                               public juce::TableListBoxModel,
                               public juce::ValueTree::Listener,
                               public juce::ChangeListener,
                               public RawInputManager::Listener {
public:
  MappingEditorComponent(PresetManager &pm, RawInputManager &rawInputMgr,
                         DeviceManager &deviceMgr);
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

private:
  // 1. Data/Managers
  PresetManager &presetManager;
  RawInputManager &rawInputManager;
  DeviceManager &deviceManager;
  int selectedLayerId = 0; // Phase 41: Currently selected layer
  // Phase 45: Remember selection per layer (layerId -> row index)
  std::map<int, int> layerSelectionHistory;

  // 2. Content Components (Must live longer than containers)
  LayerListPanel layerListPanel; // Phase 41: Layer sidebar
  juce::TableListBox table;
  juce::TextButton addButton;
  juce::TextButton duplicateButton;
  juce::TextButton deleteButton;
  juce::ToggleButton learnButton;
  juce::UndoManager undoManager;
  MappingInspector inspector;

  // 3. Containers (Must die first)
  juce::Viewport inspectorViewport;

  // Resizable layout for table and inspector
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar resizerBar;

  // Phase 41: Helper to get current layer's mappings
  juce::ValueTree getCurrentLayerMappings();

  void updateInspectorFromSelection(); // Phase 45.1: refresh inspector on
                                       // layer/row change

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingEditorComponent)
};