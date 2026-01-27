#include "MappingEditorComponent.h"
#include "KeyNameUtilities.h"
#include <algorithm>

// Helper to parse Hex strings from XML correctly
static uintptr_t parseDeviceHash(const juce::var &var) {
  if (var.isString())
    return (uintptr_t)var.toString().getHexValue64();
  return (uintptr_t)static_cast<juce::int64>(var);
}

MappingEditorComponent::MappingEditorComponent(PresetManager &pm,
                                               RawInputManager &rawInputMgr,
                                               DeviceManager &deviceMgr)
    : presetManager(pm), rawInputManager(rawInputMgr), deviceManager(deviceMgr),
      layerListPanel(pm),
      inspector(&undoManager, &deviceManager),
      resizerBar(&horizontalLayout, 1, true) { // Item index 1, vertical bar
  
  // Phase 41: Setup layer list panel callback
  layerListPanel.onLayerSelected = [this](int layerId) {
    selectedLayerId = layerId;
    table.updateContent();
    table.repaint();
    inspector.setSelection({});  // Clear selection when switching layers
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
  horizontalLayout.setItemLayout(0, -0.3, -0.7, -0.5); // Item 0 (Table): Min 30%, Max 70%, Preferred 50%
  horizontalLayout.setItemLayout(1, 5, 5, 5);          // Item 1 (Bar): Fixed 5px width
  horizontalLayout.setItemLayout(2, -0.3, -0.7, -0.5); // Item 2 (Inspector): Min 30%, Max 70%, Preferred 50%

  // Setup Add Button with Popup Menu
  addButton.setButtonText("+");
  addButton.onClick = [this] {
    juce::PopupMenu menu;
    menu.addItem(1, "Add Key Mapping");
    menu.addItem(2, "Add Scroll Mapping");
    menu.addItem(3, "Add Trackpad X");
    menu.addItem(4, "Add Trackpad Y");

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&addButton),
        [this](int result) {
          juce::ValueTree newMapping("Mapping");

          switch (result) {
          case 1: // Key Mapping
            newMapping.setProperty("inputKey", 81, nullptr); // Default Q
            newMapping.setProperty("deviceHash", "0", nullptr);
            newMapping.setProperty("type", "Note", nullptr);
            newMapping.setProperty("channel", 1, nullptr);
            newMapping.setProperty("data1", 60, nullptr); // Middle C
            newMapping.setProperty("data2", 127, nullptr);
            break;
          case 2: // Scroll Mapping (Scroll Up)
            newMapping.setProperty("inputKey", 0x1001, nullptr);
            newMapping.setProperty("deviceHash", "0", nullptr);
            newMapping.setProperty("type", "CC", nullptr);
            newMapping.setProperty("channel", 1, nullptr);
            newMapping.setProperty("data1", 1, nullptr);
            newMapping.setProperty("data2", 64, nullptr);
            break;
          case 3: // Trackpad X
            newMapping.setProperty("inputKey", 0x2000, nullptr);
            newMapping.setProperty("deviceHash", "0", nullptr);
            newMapping.setProperty("type", "CC", nullptr);
            newMapping.setProperty("channel", 1, nullptr);
            newMapping.setProperty("data1", 10, nullptr);
            newMapping.setProperty("data2", 64, nullptr);
            break;
          case 4: // Trackpad Y
            newMapping.setProperty("inputKey", 0x2001, nullptr);
            newMapping.setProperty("deviceHash", "0", nullptr);
            newMapping.setProperty("type", "CC", nullptr);
            newMapping.setProperty("channel", 1, nullptr);
            newMapping.setProperty("data1", 11, nullptr);
            newMapping.setProperty("data2", 64, nullptr);
            break;
          default:
            return; // User cancelled
          }

          // Phase 41: Set layerID and add to current layer's mappings
          newMapping.setProperty("layerID", selectedLayerId, nullptr);
          auto mappingsNode = getCurrentLayerMappings();
          if (mappingsNode.isValid()) {
            mappingsNode.addChild(newMapping, -1, &undoManager);
          }
          table.updateContent();
          table.repaint();
        });
  };
  addAndMakeVisible(addButton);

  // Setup Duplicate Button
  duplicateButton.setButtonText("Duplicate");
  duplicateButton.onClick = [this] {
    int row = table.getSelectedRow();
    auto mappingsNode = getCurrentLayerMappings();
    
    // Validation
    if (row < 0 || row >= mappingsNode.getNumChildren()) {
      juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "No Selection",
        "Please select a mapping to duplicate.",
        "OK"
      );
      return;
    }
    
    // Get the original mapping
    juce::ValueTree original = mappingsNode.getChild(row);
    if (!original.isValid()) {
      return;
    }
    
    // Create deep copy
    juce::ValueTree copy = original.createCopy();
    copy.setProperty("layerID", selectedLayerId, nullptr);  // Ensure layerID is set
    
    // Insert directly below the original
    mappingsNode.addChild(copy, row + 1, &undoManager);
    
    // Select the new row
    table.selectRow(row + 1);
    
    // Refresh UI
    table.updateContent();
    table.repaint();
  };
  addAndMakeVisible(duplicateButton);

  // Setup Delete Button
  deleteButton.setButtonText("-");
  deleteButton.onClick = [this] {
    int numSelected = table.getNumSelectedRows();
    if (numSelected == 0) {
      juce::AlertWindow::showMessageBoxAsync(
        juce::AlertWindow::InfoIcon,
        "No Selection",
        "Please select one or more mappings to delete.",
        "OK"
      );
      return;
    }

    // Build confirmation message
    juce::String message;
    if (numSelected == 1) {
      message = "Delete the selected mapping?\n\nThis action cannot be undone.";
    } else {
      message = "Delete " + juce::String(numSelected) + " selected mappings?\n\nThis action cannot be undone.";
    }

    juce::AlertWindow::showOkCancelBox(
      juce::AlertWindow::WarningIcon,
      "Delete Mappings",
      message,
      "Delete",
      "Cancel",
      this,
        juce::ModalCallbackFunction::create([this, numSelected](int result) {
        if (result == 1) { // OK clicked
          auto mappingsNode = getCurrentLayerMappings();
          if (!mappingsNode.isValid())
            return;

          // Collect selected rows (sorted descending to avoid index shifting)
          juce::Array<int> selectedRows;
          for (int i = 0; i < numSelected; ++i) {
            int row = table.getSelectedRow(i);
            if (row >= 0)
              selectedRows.add(row);
          }
          selectedRows.sort();
          std::reverse(selectedRows.begin(), selectedRows.end()); // Remove from end to start

          // Remove each selected mapping
          undoManager.beginNewTransaction("Delete Mappings");
          for (int row : selectedRows) {
            auto child = mappingsNode.getChild(row);
            if (child.isValid()) {
              mappingsNode.removeChild(child, &undoManager);
            }
          }

          // Clear selection and refresh
          table.deselectAllRows();
          table.updateContent();
          inspector.setSelection({});
        }
      })
    );
  };
  addAndMakeVisible(deleteButton);

  // Setup Learn Button
  learnButton.setButtonText("Learn");
  learnButton.setClickingTogglesState(true);
  addAndMakeVisible(learnButton);

  // Attach listeners
  presetManager.getRootNode().addListener(this);
  rawInputManager.addListener(this);

  // Initial update
  table.updateContent();
}

MappingEditorComponent::~MappingEditorComponent() {
  presetManager.getRootNode().removeListener(this);
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

void MappingEditorComponent::resized() {
  auto area = getLocalBounds();
  auto header = area.removeFromTop(24);
  addButton.setBounds(header.removeFromRight(30));
  header.removeFromRight(4);
  duplicateButton.setBounds(header.removeFromRight(80));
  header.removeFromRight(4);
  deleteButton.setBounds(header.removeFromRight(30));
  header.removeFromRight(4);
  learnButton.setBounds(header.removeFromRight(60));
  
  // Phase 41: Layout: LayerListPanel (20%) | Table/Inspector (80%)
  int layerListWidth = (int)(area.getWidth() * 0.2f);
  layerListPanel.setBounds(area.removeFromLeft(layerListWidth));
  
  // Use StretchableLayoutManager for horizontal split: Table | Bar | Inspector
  juce::Component* horizontalComps[] = { &table, &resizerBar, &inspectorViewport };
  horizontalLayout.layOutComponents(
    horizontalComps, 3, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
    false, true); // false = horizontal layout (left to right)
  
  // Set inspector content bounds (accounting for scrollbar width of 15px)
  int contentWidth = inspectorViewport.getWidth() - 15;
  int contentHeight = inspector.getRequiredHeight();
  inspector.setBounds(0, 0, contentWidth, contentHeight);
}

int MappingEditorComponent::getNumRows() {
  // Phase 41: Return count for current layer
  return getCurrentLayerMappings().getNumChildren();
}

juce::ValueTree MappingEditorComponent::getCurrentLayerMappings() {
  return presetManager.getMappingsListForLayer(selectedLayerId);
}

void MappingEditorComponent::paintRowBackground(juce::Graphics &g,
                                                int rowNumber, int width,
                                                int height,
                                                bool rowIsSelected) {
  if (rowIsSelected)
    g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
  else if (rowNumber % 2)
    g.fillAll(juce::Colours::white.withAlpha(0.05f));
}

void MappingEditorComponent::paintCell(juce::Graphics &g, int rowNumber,
                                       int columnId, int width, int height,
                                       bool rowIsSelected) {
  // Phase 41: Get mapping from current layer
  auto node = getCurrentLayerMappings().getChild(rowNumber);
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
    // Calculate clean name dynamically from inputKey
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

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(text, 2, 0, width - 4, height, juce::Justification::centredLeft,
             true);
}

// --- VALUE TREE LISTENER CALLBACKS (THE FIX) ---
// These ensure the table refreshes when you Load a Preset

void MappingEditorComponent::valueTreeChildAdded(juce::ValueTree &parent,
                                                 juce::ValueTree &child) {
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
  table.repaint();
}

void MappingEditorComponent::valueTreeParentChanged(juce::ValueTree &child) {
  if (child.hasType("Mappings"))
    table.updateContent();
}

void MappingEditorComponent::selectedRowsChanged(int lastRowSelected) {
  // Get selected rows from table
  int numSelected = table.getNumSelectedRows();
  
  // Build vector of selected ValueTrees
  std::vector<juce::ValueTree> selectedTrees;
  auto mappingsNode = getCurrentLayerMappings();  // Phase 41: Use current layer
  
  // Iterate over selected rows
  for (int i = 0; i < numSelected; ++i) {
    int row = table.getSelectedRow(i);
    if (row >= 0) {
      auto child = mappingsNode.getChild(row);
      if (child.isValid())
        selectedTrees.push_back(child);
    }
  }
  
  // Update inspector with selection
  inspector.setSelection(selectedTrees);
}

void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle,
                                                int keyCode, bool isDown) {
  // Check if learn mode is active
  if (!learnButton.getToggleState())
    return;

  // Only learn on key down
  if (!isDown)
    return;

  // Thread safety: wrap UI updates in callAsync
  juce::MessageManager::callAsync([this, deviceHandle, keyCode] {
    // Get selected row
    int selectedRow = table.getSelectedRow();
    if (selectedRow < 0)
      return;

    // Get the ValueTree for the selected row
    auto mappingsNode = presetManager.getMappingsNode();
    if (!mappingsNode.isValid())
      return;

    auto mappingNode = mappingsNode.getChild(selectedRow);
    if (!mappingNode.isValid())
      return;

    // Update the mapping properties
    mappingNode.setProperty("inputKey", keyCode, nullptr);
    
    // Get alias name for this hardware ID
    juce::String aliasName = deviceManager.getAliasForHardware(deviceHandle);
    
    // If no alias exists, show alert and use "Unassigned"
    if (aliasName == "Unassigned") {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::WarningIcon,
          "Device Not Assigned",
          "This device is not assigned to an alias. Please assign it in Device Setup first.",
          "OK");
      aliasName = "Unassigned";
    }
    
    // Save alias name instead of hardware ID
    mappingNode.setProperty("inputAlias", aliasName, nullptr);
    
    // Also keep deviceHash for backward compatibility (legacy support)
    mappingNode.setProperty("deviceHash",
                            juce::String::toHexString((juce::int64)deviceHandle)
                                .toUpperCase(),
                            nullptr);
    
    // Phase 41: Ensure layerID is set
    mappingNode.setProperty("layerID", selectedLayerId, nullptr);
    
    // Phase 41: Ensure layerID is set
    mappingNode.setProperty("layerID", selectedLayerId, nullptr);

    // Create display name using KeyNameUtilities
    juce::String keyName = KeyNameUtilities::getKeyName(keyCode);
    juce::String displayName = aliasName + " - " + keyName;
    mappingNode.setProperty("displayName", displayName, nullptr);

    // Turn off learn mode
    learnButton.setToggleState(false, juce::dontSendNotification);

    // Refresh the table
    table.repaint();
  });
}

void MappingEditorComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                             float value) {
  // Check if learn mode is active
  if (!learnButton.getToggleState())
    return;

  // Jitter Filter: Calculate deviation from center (0.0-1.0 range)
  float deviation = std::abs(value - 0.5f);
  
  // Threshold: Only trigger if deviation is significant (user is swiping, not just touching)
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

  // Thread safety: wrap UI updates in callAsync
  juce::MessageManager::callAsync([this, deviceToUse, axisToLearn] {
    // Get selected row
    int selectedRow = table.getSelectedRow();
    if (selectedRow < 0)
      return;

    // Get the ValueTree for the selected row
    auto mappingsNode = presetManager.getMappingsNode();
    if (!mappingsNode.isValid())
      return;

    auto mappingNode = mappingsNode.getChild(selectedRow);
    if (!mappingNode.isValid())
      return;

    // Update the mapping properties
    mappingNode.setProperty("inputKey", axisToLearn, nullptr);
    
    // Get alias name for this hardware ID
    juce::String aliasName = deviceManager.getAliasForHardware(deviceToUse);
    if (aliasName == "Unassigned") {
      juce::AlertWindow::showMessageBoxAsync(
          juce::AlertWindow::WarningIcon,
          "Device Not Assigned",
          "This device is not assigned to an alias. Please assign it in Device Setup first.",
          "OK");
      aliasName = "Unassigned";
    }
    
    // Save alias name instead of hardware ID
    mappingNode.setProperty("inputAlias", aliasName, nullptr);
    
    // Also keep deviceHash for backward compatibility
    mappingNode.setProperty("deviceHash",
                            juce::String::toHexString((juce::int64)deviceToUse)
                                .toUpperCase(),
                            nullptr);
    mappingNode.setProperty("type", "CC", nullptr); // Ensure type is CC

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