#include "MappingEditorComponent.h"
#include "KeyNameUtilities.h"
#include "MappingDefinition.h"
#include "TouchpadMixerManager.h"
#include "ZoneManager.h"
#include <algorithm>
#include <functional>

// Phase 56.3: Overlay for "Press any key..." capture
class InputCaptureOverlay : public juce::Component {
public:
  std::function<void(bool skipped)> onDismiss;

  InputCaptureOverlay() {
    label.setText("Press any key to add mapping...",
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

MappingEditorComponent::MappingEditorComponent(PresetManager &pm,
                                               RawInputManager &rawInputMgr,
                                               DeviceManager &deviceMgr,
                                               SettingsManager &settingsMgr,
                                               TouchpadMixerManager *touchpadMixerMgr,
                                               ZoneManager *zoneMgr)
    : presetManager(pm), rawInputManager(rawInputMgr), deviceManager(deviceMgr),
      settingsManager(settingsMgr), zoneManager(zoneMgr), layerListPanel(pm),
      inspector(undoManager, deviceManager, settingsManager, &presetManager, touchpadMixerMgr),
      resizerBar(&horizontalLayout, 1, true) { // Item index 1, vertical bar

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
  // Setup Headers
  table.getHeader().addColumn("Key", 1, 50);
  table.getHeader().addColumn("Device", 2, 70);
  table.getHeader().addColumn("Type", 3, 60);
  table.getHeader().addColumn("Data1", 4, 50); // Note
  table.getHeader().addColumn("Data2", 5, 50); // Vel
  table.getHeader().addColumn("Ch", 6, 30);

  table.setModel(this);
  table.setMultipleSelectionEnabled(true);
  addAndMakeVisible(table);

  // Setup viewport for inspector
  addAndMakeVisible(inspectorViewport);
  inspectorViewport.setViewedComponent(&inspector, false);
  inspectorViewport.setScrollBarsShown(true, false); // Vertical only

  // Add resizer bar
  addAndMakeVisible(resizerBar);

  // Setup horizontal layout: Table | Bar | Inspector
  horizontalLayout.setItemLayout(
      0, -0.3, -0.7, -0.5); // Item 0 (Table): Min 30%, Max 70%, Preferred 50%
  horizontalLayout.setItemLayout(1, 5, 5, 5); // Item 1 (Bar): Fixed 5px width
  horizontalLayout.setItemLayout(
      2, -0.3, -0.7,
      -0.5); // Item 2 (Inspector): Min 30%, Max 70%, Preferred 50%

  // Setup Add Button – Phase 56.3: Smart Input Capture
  addButton.setButtonText("+");
  addButton.onClick = [this] { startInputCapture(); };
  addAndMakeVisible(addButton);

  // Keyboard groups dialog (mirror Touchpad layout groups)
  groupsButton.setButtonText("Groups...");
  groupsButton.onClick = [this] {
    class KeyboardGroupsDialog : public juce::Component, public juce::ListBoxModel {
    public:
      KeyboardGroupsDialog(PresetManager *pm, ZoneManager *zm)
          : presetManager(pm), zoneManager(zm) {
        addAndMakeVisible(listBox);
        listBox.setModel(this);
        listBox.setRowHeight(24);
        addButton.setButtonText("Add");
        removeButton.setButtonText("Remove");
        renameLabel.setText("Name:", juce::dontSendNotification);
        addAndMakeVisible(addButton);
        addAndMakeVisible(removeButton);
        addAndMakeVisible(renameLabel);
        addAndMakeVisible(renameEditor);
        addButton.onClick = [this] { addGroup(); };
        removeButton.onClick = [this] { removeSelectedGroup(); };
        renameEditor.onTextChange = [this] { confirmRename(); };
        renameEditor.onReturnKey = [this] { confirmRename(); };
        renameEditor.onFocusLost = [this] { confirmRename(); };
        refreshFromManager();
      }
      int getNumRows() override { return (int)groups.size(); }
      void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                            bool rowIsSelected) override {
        if (row < 0 || row >= (int)groups.size()) return;
        if (rowIsSelected) g.fillAll(juce::Colour(0xff4a4a4a));
        else g.fillAll(juce::Colour(0xff2a2a2a));
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(groups[(size_t)row].second, 8, 0, width - 16, height,
                   juce::Justification::centredLeft);
      }
      void selectedRowsChanged(int lastRowSelected) override {
        if (lastRowSelected >= 0 && lastRowSelected < (int)groups.size()) {
          renameEditor.setText(groups[(size_t)lastRowSelected].second,
                               juce::dontSendNotification);
        } else {
          renameEditor.setText({}, juce::dontSendNotification);
        }
      }
      void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto bottom = area.removeFromBottom(30);
        removeButton.setBounds(bottom.removeFromRight(80));
        bottom.removeFromRight(4);
        addButton.setBounds(bottom.removeFromRight(80));
        auto nameArea = area.removeFromBottom(24);
        renameLabel.setBounds(nameArea.removeFromLeft(60));
        renameEditor.setBounds(nameArea);
        listBox.setBounds(area.reduced(0, 4));
      }
    private:
      PresetManager *presetManager = nullptr;
      ZoneManager *zoneManager = nullptr;
      juce::ListBox listBox{"KeyboardGroups", this};
      juce::TextButton addButton, removeButton;
      juce::Label renameLabel;
      juce::TextEditor renameEditor;
      std::vector<std::pair<int, juce::String>> groups;
      void refreshFromManager() {
        groups.clear();
        if (!presetManager) return;
        for (const auto &g : presetManager->getKeyboardGroups())
          groups.emplace_back(g.id, g.name);
        listBox.updateContent();
      }
      void addGroup() {
        if (!presetManager) return;
        int nextId = 1;
        for (const auto &g : presetManager->getKeyboardGroups())
          nextId = juce::jmax(nextId, g.id + 1);
        presetManager->addKeyboardGroup(nextId, "Group " + juce::String(nextId));
        refreshFromManager();
        int newRow = -1;
        for (int i = 0; i < (int)groups.size(); ++i) {
          if (groups[(size_t)i].first == nextId) { newRow = i; break; }
        }
        if (newRow >= 0) {
          listBox.selectRow(newRow);
          renameEditor.setText(groups[(size_t)newRow].second,
                               juce::dontSendNotification);
          renameEditor.grabKeyboardFocus();
          renameEditor.setHighlightedRegion(
              juce::Range<int>(0, renameEditor.getText().length()));
        }
      }
      void removeSelectedGroup() {
        if (!presetManager) return;
        int row = listBox.getSelectedRow();
        if (row < 0 || row >= (int)groups.size()) return;
        int id = groups[(size_t)row].first;
        presetManager->removeKeyboardGroup(id);
        if (zoneManager)
          zoneManager->clearKeyboardGroupFromAllZones(id);
        refreshFromManager();
      }
      void confirmRename() {
        if (!presetManager) return;
        int row = listBox.getSelectedRow();
        if (row < 0 || row >= (int)groups.size()) return;
        int id = groups[(size_t)row].first;
        auto text = renameEditor.getText().trim();
        if (text.isEmpty()) return;
        if (text == groups[(size_t)row].second) return;
        presetManager->renameKeyboardGroup(id, text);
        refreshFromManager();
        if (row >= 0 && row < (int)groups.size()) {
          listBox.selectRow(row);
          listBox.repaint();
        }
      }
    };
    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Keyboard Groups";
    opts.dialogBackgroundColour = juce::Colour(0xff222222);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(new KeyboardGroupsDialog(&presetManager, zoneManager));
    opts.content->setSize(300, 260);
    opts.launchAsync();
  };
  addAndMakeVisible(groupsButton);

  // Setup Duplicate Button
  duplicateButton.setButtonText("Duplicate");
  duplicateButton.onClick = [this] {
    int row = table.getSelectedRow();
    auto mappingsNode = getCurrentLayerMappings();

    if (row < 0 || row >= getNumRows()) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select a mapping to duplicate.", "OK");
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
  addAndMakeVisible(duplicateButton);

  // Setup Move to Layer Button
  moveToLayerButton.setButtonText("Move to layer...");
  moveToLayerButton.onClick = [this] {
    int numSelected = table.getNumSelectedRows();
    if (numSelected == 0) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select one or more mappings to move.", "OK");
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
  addAndMakeVisible(moveToLayerButton);

  // Setup Delete Button
  deleteButton.setButtonText("-");
  deleteButton.onClick = [this] {
    int numSelected = table.getNumSelectedRows();
    if (numSelected == 0) {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::InfoIcon, "No Selection",
          "Please select one or more mappings to delete.", "OK");
      return;
    }

    // Build confirmation message
    juce::String message;
    if (numSelected == 1) {
      message = "Delete the selected mapping?\n\nThis action cannot be undone.";
    } else {
      message = "Delete " + juce::String(numSelected) +
                " selected mappings?\n\nThis action cannot be undone.";
    }

    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::WarningIcon, "Delete Mappings", message, "Delete",
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

            undoManager.beginNewTransaction("Delete Mappings");
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
  addAndMakeVisible(deleteButton);

  // Setup Learn Button
  learnButton.setButtonText("Learn");
  learnButton.setClickingTogglesState(true);
  addAndMakeVisible(learnButton);

  // Phase 42: Listeners and initial update moved to initialize()
}

void MappingEditorComponent::initialize() {
  presetManager.getRootNode().addListener(this);
  presetManager.addChangeListener(this);
  rawInputManager.addListener(this);
  table.updateContent();
}

MappingEditorComponent::~MappingEditorComponent() {
  presetManager.getRootNode().removeListener(this);
  presetManager.removeChangeListener(this);
  rawInputManager.removeListener(this);
}

void MappingEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff333333));

  // If empty, draw a hint
  if (getNumRows() == 0) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No Mappings. Click '+' to add.", getLocalBounds(),
               juce::Justification::centred, true);
  }
}

void MappingEditorComponent::startInputCapture() {
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

void MappingEditorComponent::finishInputCapture(uintptr_t deviceHandle,
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

void MappingEditorComponent::resized() {
  auto area = getLocalBounds();
  if (captureOverlay)
    captureOverlay->setBounds(area);
  auto header = area.removeFromTop(24);
  addButton.setBounds(header.removeFromRight(30));
  header.removeFromRight(4);
  groupsButton.setBounds(header.removeFromRight(70));
  header.removeFromRight(4);
  duplicateButton.setBounds(header.removeFromRight(80));
  header.removeFromRight(4);
  moveToLayerButton.setBounds(header.removeFromRight(110));
  header.removeFromRight(4);
  deleteButton.setBounds(header.removeFromRight(30));
  header.removeFromRight(4);
  learnButton.setBounds(header.removeFromRight(60));

  // Phase 41: juce::Grid (1 row, 2 cols): layerListPanel 20%, table area 80%
  juce::Grid grid;
  grid.templateRows = {juce::Grid::TrackInfo(juce::Grid::Fr(1))};
  grid.templateColumns = {juce::Grid::TrackInfo(juce::Grid::Fr(2)),
                          juce::Grid::TrackInfo(juce::Grid::Fr(8))};
  grid.items = {juce::GridItem(layerListPanel), juce::GridItem(table)};
  grid.performLayout(area);

  // Right 80%: split Table | Bar | Inspector
  auto rightArea = table.getBounds();
  juce::Component *horizontalComps[] = {&table, &resizerBar,
                                        &inspectorViewport};
  horizontalLayout.layOutComponents(horizontalComps, 3, rightArea.getX(),
                                    rightArea.getY(), rightArea.getWidth(),
                                    rightArea.getHeight(), false, true);
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

int MappingEditorComponent::getNonTouchpadMappingCount() const {
  auto mappingsNode = presetManager.getMappingsListForLayer(selectedLayerId);
  int n = 0;
  for (int i = 0; i < mappingsNode.getNumChildren(); ++i) {
    if (!isTouchpadMapping(mappingsNode.getChild(i)))
      ++n;
  }
  return n;
}

juce::ValueTree MappingEditorComponent::getMappingAtRow(int row) const {
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

int MappingEditorComponent::rowToChildIndex(int row) const {
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

int MappingEditorComponent::getNumRows() {
  return getNonTouchpadMappingCount();
}

juce::ValueTree MappingEditorComponent::getCurrentLayerMappings() {
  return presetManager.getMappingsListForLayer(selectedLayerId);
}

void MappingEditorComponent::moveSelectedMappingsToLayer(int targetLayerId) {
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

void MappingEditorComponent::paintRowBackground(juce::Graphics &g,
                                                int rowNumber, int width,
                                                int height,
                                                bool rowIsSelected) {
  auto node = getMappingAtRow(rowNumber);
  const bool disabled =
      node.isValid() && !MappingDefinition::isMappingEnabled(node);
  if (rowIsSelected)
    g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
  else if (disabled)
    g.fillAll(juce::Colours::grey.withAlpha(0.35f));
  else if (rowNumber % 2)
    g.fillAll(juce::Colours::white.withAlpha(0.05f));
}

void MappingEditorComponent::paintCell(juce::Graphics &g, int rowNumber,
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
  g.setColour(disabled ? juce::Colours::grey : juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft,
             true);
}

// --- VALUE TREE LISTENER CALLBACKS (THE FIX) ---
// These ensure the table refreshes when you Load a Preset

void MappingEditorComponent::valueTreeChildAdded(juce::ValueTree &parent,
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

void MappingEditorComponent::valueTreeChildRemoved(juce::ValueTree &parent,
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

void MappingEditorComponent::valueTreePropertyChanged(
    juce::ValueTree &, const juce::Identifier &) {
  if (presetManager.getIsLoading())
    return;
  table.repaint();
}

void MappingEditorComponent::valueTreeParentChanged(juce::ValueTree &child) {
  if (presetManager.getIsLoading())
    return;
  if (child.hasType("Mappings"))
    table.updateContent();
}

void MappingEditorComponent::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == &presetManager)
    table.updateContent();
}

void MappingEditorComponent::selectedRowsChanged(int lastRowSelected) {
  updateInspectorFromSelection();
}

void MappingEditorComponent::updateInspectorFromSelection() {
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

void MappingEditorComponent::saveUiState(SettingsManager &settings) const {
  if (!settings.getRememberUiState())
    return;
  settings.setMappingsSelectedLayerId(selectedLayerId);
  settings.setMappingsSelectedRow(table.getSelectedRow());
}

void MappingEditorComponent::loadUiState(SettingsManager &settings) {
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

void MappingEditorComponent::handleTouchpadContacts(
    uintptr_t /*deviceHandle*/, const std::vector<TouchpadContact> &) {
  // Touchpad mappings are managed in the Touchpad tab; no capture in Mappings tab.
}

void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle,
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

void MappingEditorComponent::handleAxisEvent(uintptr_t deviceHandle,
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