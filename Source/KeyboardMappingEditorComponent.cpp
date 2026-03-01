#include "KeyboardMappingEditorComponent.h"
#include "KeyNameUtilities.h"
#include "MappingDefinition.h"
#include "MappingListPanel.h"
#include "TouchpadLayoutManager.h"
#include "ZoneManager.h"
#include <algorithm>
#include <functional>

// Phase 56.3: Overlay for "Press any key..." capture
class InputCaptureOverlay : public juce::Component {
public:
  std::function<void(bool skipped)> onDismiss;

  InputCaptureOverlay() {
    label.setText("Press any key to add keyboard mapping...",
                  juce::dontSendNotification);
    label.setFont(juce::Font(20.0f, juce::Font::bold));
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(label);

    skipButton.setButtonText("Skip (Add Default)");
    skipButton.onClick = [this] {
      if (onDismiss)
        onDismiss(true);
    };
    addAndMakeVisible(skipButton);

    cancelButton.setButtonText("Cancel");
    cancelButton.onClick = [this] {
      if (onDismiss)
        onDismiss(false);
    };
    addAndMakeVisible(cancelButton);
  }

  void paint(juce::Graphics &g) override {
    g.fillAll(juce::Colour(0xcc000000)); // Semi-transparent black
  }

  void resized() override {
    auto r = getLocalBounds().reduced(40);
    label.setBounds(r.removeFromTop(60));
    auto btnArea = r.removeFromBottom(40);
    cancelButton.setBounds(btnArea.removeFromRight(80));
    btnArea.removeFromRight(10);
    skipButton.setBounds(btnArea.removeFromRight(140));
  }

private:
  juce::Label label;
  juce::TextButton skipButton;
  juce::TextButton cancelButton;
};

// Helper to parse Hex strings from XML correctly
static uintptr_t parseDeviceHash(const juce::var &var) {
  if (var.isString())
    return (uintptr_t)var.toString().getHexValue64();
  return (uintptr_t)static_cast<juce::int64>(var);
}

KeyboardMappingEditorComponent::KeyboardMappingEditorComponent(PresetManager &pm,
                                               RawInputManager &rawInputMgr,
                                               DeviceManager &deviceMgr,
                                               SettingsManager &settingsMgr,
                                               TouchpadLayoutManager *touchpadLayoutMgr,
                                               ZoneManager *zoneMgr)
    : presetManager(pm), rawInputManager(rawInputMgr), deviceManager(deviceMgr),
      settingsManager(settingsMgr), zoneManager(zoneMgr), layerListPanel(pm, zoneMgr),
      inspector(undoManager, deviceManager, settingsManager, &presetManager, touchpadLayoutMgr),
      mappingListPanel(table, addButton, deleteButton, moveToLayerButton, duplicateButton),
      layerResizerBar(true), resizerBar(true) {

  // Phase 41/45: Setup layer list panel callback with per-layer selection
  // memory
  layerListPanel.onLayerSelected = [this](int newLayerId) {
    // 1. Save current selection for previously active layer
    int currentRow = table.getSelectedRow();
    layerSelectionHistory[selectedLayerId] = currentRow;

    // 2. Switch to new layer and refresh table
    selectedLayerId = newLayerId;
    table.updateContent();
    table.repaint();

    // 3. Restore saved selection for new layer, if valid
    int savedRow = -1;
    auto it = layerSelectionHistory.find(newLayerId);
    if (it != layerSelectionHistory.end())
      savedRow = it->second;

    if (savedRow >= 0 && savedRow < getNumRows()) {
      table.selectRow(savedRow);
    } else {
      table.deselectAllRows();
    }

    // Phase 45.1: force inspector refresh even if row index is unchanged
    updateInspectorFromSelection();

    // Phase 45.3: notify external listeners (e.g. Visualizer) of layer change
    if (onLayerChanged)
      onLayerChanged(selectedLayerId);
  };
  addAndMakeVisible(layerListPanel);
  addAndMakeVisible(layerResizerBar);
  // Setup Headers
  table.getHeader().addColumn("Key", 1, 50);
  table.getHeader().addColumn("Device", 2, 70);
  table.getHeader().addColumn("Type", 3, 60);
  table.getHeader().addColumn("Data1", 4, 50); // Note
  table.getHeader().addColumn("Data2", 5, 50); // Vel
  table.getHeader().addColumn("Ch", 6, 30);

  table.setModel(this);
  table.setMultipleSelectionEnabled(true);

  addAndMakeVisible(mappingListPanel);
  mappingListPanel.addAndMakeVisible(table);

  // Setup viewport for inspector
  addAndMakeVisible(inspectorViewport);
  inspectorViewport.setViewedComponent(&inspector, false);
  inspectorViewport.setScrollBarsShown(true, false); // Vertical only

  addAndMakeVisible(resizerBar);

  layerResizerBar.onPositionChange = [this](float f) {
    divider1Fraction = f;
    resized();
  };
  resizerBar.onPositionChange = [this](float f) {
    divider2Fraction = f;
    resized();
  };

  // Setup Add Button – Phase 56.3: Smart Input Capture
  addButton.setButtonText("+");
  addButton.onClick = [this] { startInputCapture(); };
  mappingListPanel.addAndMakeVisible(addButton);

  // Setup Duplicate Button (parented to mappingListPanel)
  duplicateButton.setButtonText("Duplicate");
  duplicateButton.onClick = [this] {
    int row = table.getSelectedRow();
    auto mappingsNode = getCurrentLayerMappings();

    if (row < 0 || row >= getNumRows()) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select a keyboard mapping to duplicate.", "OK");
      return;
    }

    int childIndex = rowToChildIndex(row);
    juce::ValueTree original = mappingsNode.getChild(childIndex);
    if (!original.isValid())
      return;

    juce::ValueTree copy = original.createCopy();
    copy.setProperty("layerID", selectedLayerId, nullptr);
    mappingsNode.addChild(copy, childIndex + 1, &undoManager);

    table.selectRow(row + 1);
    table.updateContent();
    table.repaint();
  };
  mappingListPanel.addAndMakeVisible(duplicateButton);

  // Setup Move to Layer Button
  moveToLayerButton.setButtonText("Move to layer...");
  moveToLayerButton.onClick = [this] {
    int numSelected = table.getNumSelectedRows();
    if (numSelected == 0) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select one or more keyboard mappings to move.", "OK");
      return;
    }

    juce::PopupMenu menu;
    auto layerOptions = MappingDefinition::getLayerOptions();
    for (const auto &pair : layerOptions) {
      int layerId = pair.first;
      juce::String name = pair.second;
      bool isCurrent = (layerId == selectedLayerId);
      menu.addItem(layerId + 1, name, !isCurrent, false);
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&moveToLayerButton),
        [this](int result) {
          if (result > 0) {
            int targetLayerId = result - 1;
            moveSelectedMappingsToLayer(targetLayerId);
          }
        });
  };
  mappingListPanel.addAndMakeVisible(moveToLayerButton);

  // Setup Delete Button
  deleteButton.setButtonText("-");
  deleteButton.onClick = [this] {
    int numSelected = table.getNumSelectedRows();
    if (numSelected == 0) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select one or more keyboard mappings to delete.", "OK");
      return;
    }

    // Build confirmation message
    juce::String message;
    if (numSelected == 1) {
      message = "Delete the selected keyboard mapping?\n\nThis action cannot be undone.";
    } else {
      message = "Delete " + juce::String(numSelected) +
                " selected keyboard mappings?\n\nThis action cannot be undone.";
    }

    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon, "Delete Keyboard Mappings", message, "Delete",
        "Cancel", this,
        juce::ModalCallbackFunction::create([this, numSelected](int result) {
          if (result == 1) { // OK clicked
            auto mappingsNode = getCurrentLayerMappings();
            if (!mappingsNode.isValid())
              return;

            juce::Array<int> selectedRows;
            for (int i = 0; i < numSelected; ++i) {
              int row = table.getSelectedRow(i);
              if (row >= 0)
                selectedRows.add(row);
            }
            selectedRows.sort();
            std::reverse(selectedRows.begin(), selectedRows.end());

            undoManager.beginNewTransaction("Delete Keyboard Mappings");
            for (int row : selectedRows) {
              auto child = getMappingAtRow(row);
              if (child.isValid())
                mappingsNode.removeChild(child, &undoManager);
            }

            table.deselectAllRows();
            table.updateContent();
            inspector.setSelection({});
          }
        }));
  };
  mappingListPanel.addAndMakeVisible(deleteButton);

  // Setup Learn Button
  learnButton.setButtonText("Learn");
  learnButton.setClickingTogglesState(true);
  addAndMakeVisible(learnButton);

  // Phase 42: Listeners and initial update moved to initialize()
}

void KeyboardMappingEditorComponent::initialize() {
  inspector.setLearnButton(&learnButton);
  inspector.setOnLayerChangeRequested([this](int targetLayerId) {
    moveSelectedMappingsToLayer(targetLayerId);
  });

  presetManager.getRootNode().addListener(this);
  presetManager.addChangeListener(this);
  rawInputManager.addListener(this);
  table.updateContent();
}

KeyboardMappingEditorComponent::~KeyboardMappingEditorComponent() {
  presetManager.getRootNode().removeListener(this);
  presetManager.removeChangeListener(this);
  rawInputManager.removeListener(this);
}

void KeyboardMappingEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff333333));

  // If empty, draw a hint
  if (getNumRows() == 0) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No keyboard mappings. Click '+' to add.", getLocalBounds(),
               juce::Justification::centred, true);
  }
}

void KeyboardMappingEditorComponent::startInputCapture() {
  wasMidiModeEnabledBeforeCapture = settingsManager.isMidiModeActive();
  if (!wasMidiModeEnabledBeforeCapture)
    settingsManager.setMidiModeActive(true);

  captureOverlay = std::make_unique<InputCaptureOverlay>();
  captureOverlay->onDismiss = [this](bool skipped) {
    if (skipped) {
      finishInputCapture(0, 0, true);
    } else {
      // Cancelled: restore MIDI mode, remove overlay
      if (!wasMidiModeEnabledBeforeCapture)
        settingsManager.setMidiModeActive(false);
      captureOverlay.reset();
      resized();
    }
  };
  addAndMakeVisible(captureOverlay.get());
  resized();
}

void KeyboardMappingEditorComponent::finishInputCapture(uintptr_t deviceHandle,
                                                int keyCode, bool skipped) {
  // 1. Cleanup: remove overlay, restore MIDI mode
  if (!wasMidiModeEnabledBeforeCapture)
    settingsManager.setMidiModeActive(false);
  captureOverlay.reset();
  resized();

  // 2. Create mapping
  int inputKey = 0;
  juce::String deviceHashStr = "0";
  juce::String inputAlias;

  if (!skipped) {
    inputKey = keyCode;
    juce::String aliasName = deviceManager.getAliasForHardware(deviceHandle);
    if (aliasName.isEmpty() || aliasName == "Unassigned" ||
        aliasName == "Unknown") {
      deviceHashStr = "0";
      inputAlias = "";
    } else {
      uintptr_t hash =
          static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName.trim()));
      deviceHashStr =
          juce::String::toHexString((juce::int64)hash).toUpperCase();
      inputAlias = aliasName;
    }
  } else {
    inputKey = 81;
    deviceHashStr = "0";
    inputAlias = "";
  }

  juce::ValueTree newMapping("Mapping");
  newMapping.setProperty("enabled", true, nullptr);
  newMapping.setProperty("inputKey", inputKey, nullptr);
  newMapping.setProperty("deviceHash", deviceHashStr, nullptr);
  newMapping.setProperty("inputAlias", inputAlias, nullptr);
  newMapping.setProperty("layerID", selectedLayerId, nullptr);

  if (skipped) {
    newMapping.setProperty("type", "Note", nullptr);
    newMapping.setProperty("channel", 1, nullptr);
    newMapping.setProperty("data1", 60, nullptr);
    newMapping.setProperty("data2", 127, nullptr);
    newMapping.setProperty("releaseBehavior", "Send Note Off", nullptr);
    newMapping.setProperty("followTranspose", true, nullptr);
  } else {
    newMapping.setProperty("type", "Note", nullptr);
    newMapping.setProperty("channel", 1, nullptr);
    newMapping.setProperty("data1", 60, nullptr);
    newMapping.setProperty("data2", 127, nullptr);
    newMapping.setProperty("releaseBehavior", "Send Note Off", nullptr);
    newMapping.setProperty("followTranspose", true, nullptr);
  }

  auto mappingsNode = getCurrentLayerMappings();
  if (mappingsNode.isValid()) {
    mappingsNode.addChild(newMapping, -1, &undoManager);
  }

  table.updateContent();
  table.repaint();

  int newRow = getNumRows() - 1;
  if (newRow >= 0)
    table.selectRow(newRow);
}

void KeyboardMappingEditorComponent::resized() {
  auto area = getLocalBounds();
  if (captureOverlay)
    captureOverlay->setBounds(area);

  PercentageSplitLayout::apply(
      area.getX(), area.getY(), area.getWidth(), area.getHeight(), true,
      divider1Fraction, divider2Fraction,
      layerListPanel, layerResizerBar, mappingListPanel, resizerBar,
      inspectorViewport, 5, 80, 150, 250);

  int contentWidth = inspectorViewport.getWidth() - 15;
  int contentHeight = inspector.getRequiredHeight();
  inspector.setBounds(0, 0, contentWidth, contentHeight);
}

static bool isTouchpadMapping(const juce::ValueTree &mapping) {
  return mapping.getProperty("inputAlias", "")
      .toString()
      .trim()
      .equalsIgnoreCase("Touchpad");
}

int KeyboardMappingEditorComponent::getNonTouchpadMappingCount() const {
  auto mappingsNode = presetManager.getMappingsListForLayer(selectedLayerId);
  int n = 0;
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    if (!isTouchpadMapping(mappingsNode.getChild(i)))
      ++n;
  }
  return n;
}

juce::ValueTree KeyboardMappingEditorComponent::getMappingAtRow(int row) const {
  auto mappingsNode = presetManager.getMappingsListForLayer(selectedLayerId);
  int k = 0;
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    auto child = mappingsNode.getChild(i);
    if (!isTouchpadMapping(child)) {
      if (k == row)
        return child;
      ++k;
    }
  }
  return juce::ValueTree();
}

int KeyboardMappingEditorComponent::rowToChildIndex(int row) const {
  auto mappingsNode = presetManager.getMappingsListForLayer(selectedLayerId);
  int k = 0;
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    if (!isTouchpadMapping(mappingsNode.getChild(i))) {
      if (k == row)
        return i;
      ++k;
    }
  }
  return -1;
}

int KeyboardMappingEditorComponent::getNumRows() {
  return getNonTouchpadMappingCount();
}

juce::ValueTree KeyboardMappingEditorComponent::getCurrentLayerMappings() {
  return presetManager.getMappingsListForLayer(selectedLayerId);
}

void KeyboardMappingEditorComponent::moveSelectedMappingsToLayer(int targetLayerId) {
  if (targetLayerId == selectedLayerId)
    return;
  if (targetLayerId < 0 || targetLayerId > 8)
    return;

  auto sourceMappings = getCurrentLayerMappings();
  juce::ValueTree targetMappings = presetManager.getMappingsListForLayer(targetLayerId);
  if (!sourceMappings.isValid() || !targetMappings.isValid())
    return;

  int numSelected = table.getNumSelectedRows();
  if (numSelected == 0)
    return;

  juce::Array<int> selectedRows;
  for (int i = 0; i < numSelected; ++i) {
    int row = table.getSelectedRow(i);
    if (row >= 0 && row < getNumRows())
      selectedRows.add(row);
  }
  selectedRows.sort();
  std::reverse(selectedRows.begin(), selectedRows.end());

  juce::String layerName = targetLayerId == 0
                               ? "Base"
                               : presetManager.getLayerNode(targetLayerId)
                                     .getProperty("name", "Layer " + juce::String(targetLayerId))
                                     .toString();
  undoManager.beginNewTransaction("Move to " + layerName);

  for (int row : selectedRows) {
    auto child = getMappingAtRow(row);
    if (!child.isValid())
      continue;
    juce::ValueTree copy = child.createCopy();
    copy.setProperty("layerID", targetLayerId, &undoManager);
    targetMappings.addChild(copy, -1, &undoManager);
    sourceMappings.removeChild(child, &undoManager);
  }

  table.deselectAllRows();
  table.updateContent();
  table.repaint();
  inspector.setSelection({});
}

void KeyboardMappingEditorComponent::paintRowBackground(juce::Graphics &g,
                                                int rowNumber, int width,
                                                int height,
                                                bool rowIsSelected) {
  auto node = getMappingAtRow(rowNumber);
  const bool disabled =
      node.isValid() && !MappingDefinition::isMappingEnabled(node);
  const int pad = 2;
  auto area = juce::Rectangle<int>(0, 0, width, height).reduced(pad, 0);
  if (rowIsSelected) {
    g.setColour(juce::Colour(0xff3d5a80));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    g.setColour(juce::Colours::lightblue.withAlpha(0.5f));
    g.fillRect(area.getX(), area.getY(), 3, area.getHeight());
  } else if (disabled) {
    g.setColour(juce::Colour(0xff2a2a2a));
    g.fillRect(area);
    g.setColour(juce::Colours::grey.withAlpha(0.4f));
    g.fillRect(area.getX(), area.getY(), 3, area.getHeight());
  } else {
    g.setColour(rowNumber % 2 ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff252525));
    g.fillRect(0, 0, width, height);
  }
}

void KeyboardMappingEditorComponent::paintCell(juce::Graphics &g, int rowNumber,
                                       int columnId, int width, int height,
                                       bool rowIsSelected) {
  auto node = getMappingAtRow(rowNumber);
  if (!node.isValid())
    return;

  juce::String text;

  auto getHex = [](const juce::var &v) {
    if (v.isString())
      return v.toString();
    return juce::String::toHexString((juce::int64)v).toUpperCase();
  };

  switch (columnId) {
  case 1:
    text = KeyNameUtilities::getKeyName((int)node.getProperty("inputKey"));
    break;
  case 2:
    // Show alias name if available, otherwise convert deviceHash to alias name
    if (node.hasProperty("inputAlias")) {
      text = node.getProperty("inputAlias").toString();
    } else {
      // Legacy: convert deviceHash (hardware ID) to alias name
      uintptr_t deviceHash = parseDeviceHash(node.getProperty("deviceHash"));
      text = deviceManager.getAliasName(deviceHash);
    }
    break;
  case 3:
    text = node.getProperty("type").toString();
    break;
  case 4:
    text = node.getProperty("data1").toString();
    break;
  case 5:
    text = node.getProperty("data2").toString();
    break;
  case 6:
    text = node.getProperty("channel").toString();
    break;
  }

  const bool disabled = !MappingDefinition::isMappingEnabled(node);
  g.setColour(disabled ? juce::Colours::grey : (rowIsSelected ? juce::Colours::white : juce::Colours::lightgrey));
  g.setFont(14.0f);
  // Column 1 (Key) is leftmost: add padding to clear the selection accent bar (pad 2 + bar 3 + gap 3)
  const int textLeft = (columnId == 1) ? 10 : 4;
  g.drawText(text, textLeft, 0, width - textLeft - 4, height,
             juce::Justification::centredLeft, true);
}

// --- VALUE TREE LISTENER CALLBACKS (THE FIX) ---
// These ensure the table refreshes when you Load a Preset

void KeyboardMappingEditorComponent::valueTreeChildAdded(juce::ValueTree &parent,
                                                 juce::ValueTree &child) {
  if (presetManager.getIsLoading())
    return;
  // Phase 41: Update if mappings are added to current layer
  if (parent.hasType("Mappings") || child.hasType("Mappings")) {
    // Check if this is for the current layer
    if (parent.getParent().isValid() && parent.getParent().hasType("Layer")) {
      int layerId = parent.getParent().getProperty("id", -1);
      if (layerId == selectedLayerId) {
        table.updateContent();
      }
    } else if (parent.hasType("Mappings")) {
      // Legacy: update if it's a direct child
      table.updateContent();
    }
  }
}

void KeyboardMappingEditorComponent::valueTreeChildRemoved(juce::ValueTree &parent,
                                                   juce::ValueTree &child,
                                                   int) {
  if (presetManager.getIsLoading())
    return;
  // Phase 41: Update if mappings are removed from current layer
  if (parent.hasType("Mappings") || child.hasType("Mappings")) {
    if (parent.getParent().isValid() && parent.getParent().hasType("Layer")) {
      int layerId = parent.getParent().getProperty("id", -1);
      if (layerId == selectedLayerId) {
        table.updateContent();
      }
    } else if (parent.hasType("Mappings")) {
      table.updateContent();
    }
  }
}

void KeyboardMappingEditorComponent::valueTreePropertyChanged(
    juce::ValueTree &, const juce::Identifier &) {
  if (presetManager.getIsLoading())
    return;
  table.repaint();
}

void KeyboardMappingEditorComponent::valueTreeParentChanged(juce::ValueTree &child) {
  if (presetManager.getIsLoading())
    return;
  if (child.hasType("Mappings"))
    table.updateContent();
}

void KeyboardMappingEditorComponent::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == &presetManager)
    table.updateContent();
}

void KeyboardMappingEditorComponent::selectedRowsChanged(int lastRowSelected) {
  updateInspectorFromSelection();
}

void KeyboardMappingEditorComponent::updateInspectorFromSelection() {
  auto selectedRows = table.getSelectedRows();
  std::vector<juce::ValueTree> selectedTrees;
  for (int i = 0; i < selectedRows.size(); ++i) {
    int row = selectedRows[i];
    if (row >= 0 && row < getNumRows()) {
      auto child = getMappingAtRow(row);
      if (child.isValid())
        selectedTrees.push_back(child);
    }
  }
  inspector.setSelection(selectedTrees);
}

void KeyboardMappingEditorComponent::saveUiState(SettingsManager &settings) const {
  if (!settings.getRememberUiState())
    return;
  settings.setMappingsSelectedLayerId(selectedLayerId);
  settings.setMappingsSelectedRow(table.getSelectedRow());
}

void KeyboardMappingEditorComponent::loadUiState(SettingsManager &settings) {
  if (!settings.getRememberUiState())
    return;
  int layerId = settings.getMappingsSelectedLayerId();
  int row = settings.getMappingsSelectedRow();
  if (layerId < 0 || layerId > 8)
    layerId = 0;
  layerListPanel.setSelectedLayer(layerId);
  if (row >= 0 && row < getNumRows())
    table.selectRow(row);
}

void KeyboardMappingEditorComponent::handleTouchpadContacts(
    uintptr_t /*deviceHandle*/, const std::vector<TouchpadContact> &) {
  // Touchpad mappings are managed in the Touchpad tab; no capture in Mappings tab.
}

void KeyboardMappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle,
                                               int keyCode, bool isDown) {
  // Phase 56.3: Smart Input Capture – capture key when overlay is active
  if (captureOverlay != nullptr) {
    if (isDown) {
      juce::MessageManager::callAsync([this, deviceHandle, keyCode] {
        finishInputCapture(deviceHandle, keyCode, false);
      });
    }
    return;
  }

  // Check if learn mode is active
  if (!learnButton.getToggleState())
    return;

  // Only learn on key down
  if (!isDown)
    return;

  // Phase 45.2: ignore the global toggle key (e.g. Scroll Lock) in learn mode
  if (keyCode == settingsManager.getToggleKey())
    return;

  // Thread safety: wrap UI updates in callAsync
  juce::MessageManager::callAsync([this, deviceHandle, keyCode] {
    // Re-check learn state on the message thread
    if (!learnButton.getToggleState())
      return;

    int selectedRow = table.getSelectedRow();
    if (selectedRow < 0)
      return;

    auto mappingNode = getMappingAtRow(selectedRow);
    if (!mappingNode.isValid())
      return;

    // --- Update mapping from learned event ---
    // 1) Always update key code
    mappingNode.setProperty("inputKey", keyCode, nullptr);

    // 2) Conditionally update device alias/hash (Phase 45.6/45.7)
    if (settingsManager.isStudioMode()) {
      auto aliases = deviceManager.getAliasesForHardware(deviceHandle);

      DBG("Learn: Key " + juce::String(keyCode) + " on Device " +
          juce::String::toHexString((juce::int64)deviceHandle));
      DBG("Learn: Found " + juce::String(aliases.size()) +
          " aliases for device.");

      juce::uint64 bestAlias = 0;
      bool foundValid = false;

      // Prefer first non-zero alias (specific alias) over Global(0)
      for (int i = 0; i < aliases.size(); ++i) {
        auto a = aliases[i];
        if (a != 0) {
          bestAlias = a;
          foundValid = true;
          break;
        }
      }
      // If no non-zero alias but we did find something (e.g. only Global),
      // allow using that.
      if (!foundValid && aliases.size() > 0) {
        bestAlias = aliases[0];
        foundValid = true;
      }

      if (foundValid) {
        // Phase 46.6: also write inputAlias so InputProcessor compiles
        // correctly
        juce::String aliasName =
            deviceManager.getAliasName(static_cast<uintptr_t>(bestAlias));

        DBG("Learn: Switching to Alias " + aliasName + " (" +
            juce::String::toHexString((juce::int64)bestAlias) + ")");
        DBG("Learn: Writing deviceHash: " +
            juce::String::toHexString((juce::int64)bestAlias));

        mappingNode.setProperty(
            "deviceHash",
            juce::String::toHexString((juce::int64)bestAlias).toUpperCase(),
            nullptr);

        // Keep XML consistent with Inspector/Compiler expectations.
        // (InputProcessor uses inputAlias as the primary source of truth.)
        if (bestAlias == 0 || aliasName == "Global (All Devices)" ||
            aliasName == "Unknown") {
          mappingNode.setProperty("inputAlias", "", nullptr);
        } else {
          mappingNode.setProperty("inputAlias", aliasName, nullptr);
        }
      } else {
        DBG("Learn: No alias found. Keeping existing deviceHash.");
        // Do not touch deviceHash – preserve existing alias when there is no
        // mapping.
      }
    } else {
      // Studio Mode OFF: force Global (0)
      mappingNode.setProperty("deviceHash", "0", nullptr);
      // Phase 46.6: keep compiler consistent (Global = empty inputAlias)
      mappingNode.setProperty("inputAlias", "", nullptr);
    }

    // Turn off learn mode
    learnButton.setToggleState(false, juce::dontSendNotification);

    // Refresh the table
    table.repaint();
  });
}

void KeyboardMappingEditorComponent::handleAxisEvent(uintptr_t deviceHandle,
                                             int inputCode, float value) {
  // Check if learn mode is active
  if (!learnButton.getToggleState())
    return;

  // Jitter Filter: Calculate deviation from center (0.0-1.0 range)
  float deviation = std::abs(value - 0.5f);

  // Threshold: Only trigger if deviation is significant (user is swiping, not
  // just touching)
  const float threshold = 0.2f; // 20% deviation threshold
  if (deviation < threshold)
    return;

  // Track which axis has the maximum deviation (first to cross threshold wins)
  static float maxDeviation = 0.0f;
  static int learnedAxisID = -1;
  static uintptr_t learnedDevice = 0;

  // Reset if device changed or learn button was toggled off/on
  if (learnedDevice != deviceHandle) {
    maxDeviation = 0.0f;
    learnedAxisID = -1;
    learnedDevice = deviceHandle;
  }

  // Only learn the axis with maximum deviation
  if (deviation > maxDeviation) {
    maxDeviation = deviation;
    learnedAxisID = inputCode;
  } else if (learnedAxisID != inputCode) {
    // Another axis is winning, ignore this one
    return;
  }

  // Store values in local variables for lambda capture
  int axisToLearn = learnedAxisID;
  uintptr_t deviceToUse = deviceHandle;

  // Reset learning state before async call
  maxDeviation = 0.0f;
  learnedAxisID = -1;
  learnedDevice = 0;

  juce::MessageManager::callAsync([this, deviceToUse, axisToLearn] {
    int selectedRow = table.getSelectedRow();
    if (selectedRow < 0)
      return;

    auto mappingNode = getMappingAtRow(selectedRow);
    if (!mappingNode.isValid())
      return;

    // Update the mapping properties
    mappingNode.setProperty("inputKey", axisToLearn, nullptr);

    // Get alias name for this hardware ID
    juce::String aliasName = deviceManager.getAliasForHardware(deviceToUse);
    if (aliasName == "Unassigned") {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::WarningIcon, "Device Not Assigned",
          "This device is not assigned to an alias. Please assign it in Device "
          "Setup first.",
          "OK");
      aliasName = "Unassigned";
    }

    // Save alias name instead of hardware ID
    mappingNode.setProperty("inputAlias", aliasName, nullptr);

    // Also keep deviceHash for backward compatibility
    mappingNode.setProperty(
        "deviceHash",
        juce::String::toHexString((juce::int64)deviceToUse).toUpperCase(),
        nullptr);
    mappingNode.setProperty("type", "Expression", nullptr);

    // Create display name using KeyNameUtilities
    juce::String deviceName =
        KeyNameUtilities::getFriendlyDeviceName(deviceToUse);
    juce::String keyName = KeyNameUtilities::getKeyName(axisToLearn);
    juce::String displayName = deviceName + " - " + keyName;
    mappingNode.setProperty("displayName", displayName, nullptr);

    // Turn off learn mode
    learnButton.setToggleState(false, juce::dontSendNotification);

    // Refresh the table
    table.repaint();
  });
}