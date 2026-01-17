#include "DeviceSetupComponent.h"
#include "KeyNameUtilities.h"

void AliasListModel::selectedRowsChanged(int lastRowSelected) {
  if (parentComponent != nullptr)
    parentComponent->onAliasSelected();
}

DeviceSetupComponent::DeviceSetupComponent(DeviceManager &deviceMgr, RawInputManager &rawInputMgr)
    : deviceManager(deviceMgr), rawInputManager(rawInputMgr),
      aliasModel(deviceMgr, this), hardwareModel(deviceMgr) {
  
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
  scanButton.setBounds(buttonRow.removeFromLeft(120));
  buttonRow.removeFromLeft(10);
  removeButton.setBounds(buttonRow.removeFromLeft(100));
  area.removeFromTop(10);

  // Split into two columns
  auto leftPanel = area.removeFromLeft(getWidth() / 2 - 5);
  auto rightPanel = area.removeFromRight(getWidth() / 2 - 5);

  // Left: Alias list
  juce::Label aliasLabel("Aliases");
  aliasLabel.setBounds(leftPanel.removeFromTop(20));
  addAndMakeVisible(aliasLabel);
  aliasListBox.setBounds(leftPanel);

  // Right: Hardware list
  juce::Label hardwareLabel("Hardware");
  hardwareLabel.setBounds(rightPanel.removeFromTop(20));
  addAndMakeVisible(hardwareLabel);
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
          hardwareModel.setSelectedAlias("");
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
    hardwareModel.setSelectedAlias(selectedAlias);
    refreshHardwareList();
  } else {
    selectedAlias = "";
    hardwareModel.setSelectedAlias("");
    refreshHardwareList();
  }
}
