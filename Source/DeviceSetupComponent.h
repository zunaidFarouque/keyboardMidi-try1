#pragma once
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "KeyNameUtilities.h"
#include <JuceHeader.h>

// Forward declaration
class DeviceSetupComponent;

// Separate model classes for each list box
class AliasListModel : public juce::ListBoxModel {
public:
  AliasListModel(DeviceManager &dm, DeviceSetupComponent *parent) 
    : deviceManager(dm), parentComponent(parent) {}
  
  int getNumRows() override {
    return deviceManager.getAllAliases().size();
  }
  
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) override {
    if (rowIsSelected)
      g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
    
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    
    auto aliases = deviceManager.getAllAliases();
    if (rowNumber < aliases.size()) {
      g.drawText(aliases[rowNumber], 4, 0, width - 8, height, juce::Justification::centredLeft, true);
    }
  }
  
  void selectedRowsChanged(int lastRowSelected) override;
  
private:
  DeviceManager &deviceManager;
  DeviceSetupComponent *parentComponent;
};

class HardwareListModel : public juce::ListBoxModel {
public:
  HardwareListModel(DeviceManager &dm) : deviceManager(dm) {}
  
  void setSelectedAlias(const juce::String &alias) {
    selectedAlias = alias;
  }
  
  int getNumRows() override {
    if (selectedAlias.isEmpty())
      return 0;
    return deviceManager.getHardwareForAlias(selectedAlias).size();
  }
  
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) override {
    if (rowIsSelected)
      g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
    
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    
    if (!selectedAlias.isEmpty()) {
      auto hardwareIds = deviceManager.getHardwareForAlias(selectedAlias);
      if (rowNumber < hardwareIds.size()) {
        uintptr_t id = hardwareIds[rowNumber];
        juce::String text = KeyNameUtilities::getFriendlyDeviceName(id);
        g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
      }
    }
  }
  
private:
  DeviceManager &deviceManager;
  juce::String selectedAlias;
};

class DeviceSetupComponent : public juce::Component,
                              public RawInputManager::Listener {
public:
  DeviceSetupComponent(DeviceManager &deviceMgr, RawInputManager &rawInputMgr);
  ~DeviceSetupComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // Called by AliasListModel when selection changes (needs to be public for model access)
  void onAliasSelected();

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

private:
  DeviceManager &deviceManager;
  RawInputManager &rawInputManager;

  // Models
  AliasListModel aliasModel;
  HardwareListModel hardwareModel;

  // UI Elements
  juce::ListBox aliasListBox;
  juce::ListBox hardwareListBox;
  juce::TextButton addAliasButton;
  juce::TextButton deleteAliasButton;
  juce::TextButton scanButton;
  juce::TextButton removeButton;

  // State (selectedAlias is also in HardwareListModel, but we keep a reference here for convenience)
  bool isScanning = false;
  uintptr_t scannedDeviceHandle = 0;

  // Helpers
  void refreshAliasList();
  void refreshHardwareList();
  void addAlias();
  void deleteSelectedAlias();
  void removeSelectedHardware();
  
  // State
  juce::String selectedAlias;
};
