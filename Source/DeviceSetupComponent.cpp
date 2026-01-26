#include "DeviceSetupComponent.h"
#include "KeyNameUtilities.h"
#include "PresetManager.h"

void AliasListModel::selectedRowsChanged(int lastRowSelected) {
  if (parentComponent != nullptr)
    parentComponent->onAliasSelected();
}

DeviceSetupComponent::DeviceSetupComponent(DeviceManager &deviceMgr, RawInputManager &rawInputMgr, PresetManager* presetMgr)
    : deviceManager(deviceMgr), rawInputManager(rawInputMgr), presetManager(presetMgr),
      aliasModel(deviceMgr, this), hardwareModel(deviceMgr) {
  
  // Setup labels
  aliasHeaderLabel.setText("Defined Aliases", juce::dontSendNotification);
  aliasHeaderLabel.setJustificationType(juce::Justification::left);
  addAndMakeVisible(aliasHeaderLabel);
  
  hardwareHeaderLabel.setText("Associated Hardware", juce::dontSendNotification);
  hardwareHeaderLabel.setJustificationType(juce::Justification::left);
  addAndMakeVisible(hardwareHeaderLabel);
  
  // Setup alias list
  aliasListBox.setModel(&aliasModel);
  aliasListBox.setRowHeight(25);
  addAndMakeVisible(aliasListBox);

  // Setup hardware list
  hardwareListBox.setModel(&hardwareModel);
  hardwareListBox.setRowHeight(25);
  addAndMakeVisible(hardwareListBox);

  // Setup buttons
  addAliasButton.setButtonText("Add Alias");
  addAliasButton.onClick = [this] { addAlias(); };
  addAndMakeVisible(addAliasButton);

  deleteAliasButton.setButtonText("Delete Alias");
  deleteAliasButton.onClick = [this] { deleteSelectedAlias(); };
  addAndMakeVisible(deleteAliasButton);

  renameButton.setButtonText("Rename");
  renameButton.onClick = [this] {
    int selectedRow = aliasListBox.getSelectedRow();
    auto aliases = deviceManager.getAllAliases();
    
    if (selectedRow < 0 || selectedRow >= aliases.size())
      return;
    
    juce::String oldName = aliases[selectedRow];
    
    auto* dialog = new juce::AlertWindow("Rename Alias", "Enter new alias name:", juce::AlertWindow::QuestionIcon);
    dialog->addTextEditor("name", oldName, "New Alias Name:");
    dialog->addButton("Rename", 1);
    dialog->addButton("Cancel", 0);
    
    dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog, oldName](int result) {
      if (result == 1) {
        juce::String newName = dialog->getTextEditorContents("name").trim();
        if (!newName.isEmpty() && newName != oldName) {
          // ASYNC CALL to prevent stack corruption
          juce::MessageManager::callAsync([this, oldName, newName]() {
            // Safe to call now
            deviceManager.renameAlias(oldName, newName, presetManager);
            
            // Update selected alias if it was renamed
            if (selectedAlias == oldName) {
              selectedAlias = newName;
            }
            
            // Refresh UI - this will update the alias list and reselect if needed
            refreshAliasList();
            
            // Reselect the renamed alias in the list
            auto aliases = deviceManager.getAllAliases();
            int newIndex = aliases.indexOf(newName);
            if (newIndex >= 0) {
              aliasListBox.selectRow(newIndex);
            }
            
            // Update hardware list if the renamed alias was selected
            if (selectedAlias == newName) {
              hardwareModel.setAlias(newName);
              refreshHardwareList();
            }
          });
        }
      }
      delete dialog;
    }), true);
  };
  addAndMakeVisible(renameButton);

  scanButton.setButtonText("Scan/Add");
  scanButton.onClick = [this] {
    isScanning = true;
    scannedDeviceHandle = 0;
    scanButton.setButtonText("Press a key...");
    scanButton.setEnabled(false);
  };
  addAndMakeVisible(scanButton);

  removeButton.setButtonText("Remove");
  removeButton.onClick = [this] { removeSelectedHardware(); };
  addAndMakeVisible(removeButton);

  // Register for raw input events
  rawInputManager.addListener(this);

  // Initial refresh
  refreshAliasList();
}

DeviceSetupComponent::~DeviceSetupComponent() {
  rawInputManager.removeListener(this);
}

void DeviceSetupComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void DeviceSetupComponent::resized() {
  auto area = getLocalBounds().reduced(10);
  
  // Top button row
  auto buttonRow = area.removeFromTop(30);
  addAliasButton.setBounds(buttonRow.removeFromLeft(100));
  buttonRow.removeFromLeft(10);
  deleteAliasButton.setBounds(buttonRow.removeFromLeft(100));
  buttonRow.removeFromLeft(10);
  renameButton.setBounds(buttonRow.removeFromLeft(100));
  buttonRow.removeFromLeft(10);
  scanButton.setBounds(buttonRow.removeFromLeft(120));
  buttonRow.removeFromLeft(10);
  removeButton.setBounds(buttonRow.removeFromLeft(100));
  area.removeFromTop(10);

  // Split into two columns
  auto leftPanel = area.removeFromLeft(getWidth() / 2 - 5);
  auto rightPanel = area.removeFromRight(getWidth() / 2 - 5);

  // Left: Alias list with header
  aliasHeaderLabel.setBounds(leftPanel.removeFromTop(24));
  aliasListBox.setBounds(leftPanel);

  // Right: Hardware list with header
  hardwareHeaderLabel.setBounds(rightPanel.removeFromTop(24));
  hardwareListBox.setBounds(rightPanel);
}


void DeviceSetupComponent::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  if (!isScanning || !isDown)
    return;

  // Store the device handle
  scannedDeviceHandle = deviceHandle;

  // Add to selected alias
  if (!selectedAlias.isEmpty() && scannedDeviceHandle != 0) {
    deviceManager.assignHardware(selectedAlias, scannedDeviceHandle);
    refreshHardwareList();
    
    // Reset scanning state
    isScanning = false;
    scanButton.setButtonText("Scan/Add");
    scanButton.setEnabled(true);
  }
}

void DeviceSetupComponent::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Not used in device setup
}

void DeviceSetupComponent::refreshAliasList() {
  aliasListBox.updateContent();
  aliasListBox.repaint();
}

void DeviceSetupComponent::refreshHardwareList() {
  hardwareListBox.updateContent();
}

void DeviceSetupComponent::addAlias() {
  auto* dialog = new juce::AlertWindow("New Alias", "Enter alias name:", juce::AlertWindow::NoIcon);
  dialog->addTextEditor("aliasName", "", "Alias Name:");
  dialog->addButton("OK", 1);
  dialog->addButton("Cancel", 0);

  dialog->enterModalState(true, juce::ModalCallbackFunction::create([this, dialog](int result) {
    if (result == 1) {
      juce::String name = dialog->getTextEditorContents("aliasName").trim();
      if (!name.isEmpty()) {
        deviceManager.createAlias(name);
        refreshAliasList();
      }
    }
    delete dialog;
  }), true);
}

void DeviceSetupComponent::deleteSelectedAlias() {
  int selectedRow = aliasListBox.getSelectedRow();
  auto aliases = deviceManager.getAllAliases();
  
  if (selectedRow < 0 || selectedRow >= aliases.size())
    return;

  juce::String aliasToDelete = aliases[selectedRow];
  
  // Confirm deletion using callback version
  juce::AlertWindow::showOkCancelBox(
      juce::AlertWindow::WarningIcon,
      "Delete Alias",
      "Are you sure you want to delete the alias \"" + aliasToDelete + "\"?\n\n"
      "This will remove all hardware assignments for this alias.",
      "Delete",
      "Cancel",
      this,
      juce::ModalCallbackFunction::create([this, aliasToDelete](int result) {
        if (result == 1) { // OK clicked
          // Delete the alias (this removes the alias and all its hardware assignments)
          deviceManager.deleteAlias(aliasToDelete);
          
          // Clear selection and refresh
          selectedAlias = "";
          hardwareModel.setAlias("");
          refreshAliasList();
          refreshHardwareList();
        }
      }));
}

void DeviceSetupComponent::removeSelectedHardware() {
  if (selectedAlias.isEmpty())
    return;

  int selectedRow = hardwareListBox.getSelectedRow();
  if (selectedRow < 0)
    return;

  auto hardwareIds = deviceManager.getHardwareForAlias(selectedAlias);
  if (selectedRow < hardwareIds.size()) {
    uintptr_t id = hardwareIds[selectedRow];
    deviceManager.removeHardware(selectedAlias, id);
    refreshHardwareList();
  }
}

void DeviceSetupComponent::onAliasSelected() {
  int selectedRow = aliasListBox.getSelectedRow();
  auto aliases = deviceManager.getAllAliases();
  
  if (selectedRow >= 0 && selectedRow < aliases.size()) {
    selectedAlias = aliases[selectedRow];
    hardwareModel.setAlias(selectedAlias);
    hardwareListBox.updateContent();
    hardwareListBox.repaint();
  } else {
    selectedAlias = "";
    hardwareModel.setAlias("");
    hardwareListBox.updateContent();
    hardwareListBox.repaint();
  }
}
