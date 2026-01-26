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
  
  void setAlias(const juce::String &name) {
    currentAliasName = name;
  }
  
  int getNumRows() override {
    if (currentAliasName.isEmpty())
      return 0;
    return deviceManager.getHardwareForAlias(currentAliasName).size();
  }
  
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) override {
    if (rowIsSelected)
      g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
    
    g.setColour(juce::Colours::white);
    g.setFont(14.0f);
    
    if (!currentAliasName.isEmpty()) {
      auto hardwareIds = deviceManager.getHardwareForAlias(currentAliasName);
      if (rowNumber < hardwareIds.size()) {
        uintptr_t id = hardwareIds[rowNumber];
        juce::String text = KeyNameUtilities::getFriendlyDeviceName(id);
        g.drawText(text, 4, 0, width - 8, height, juce::Justification::centredLeft, true);
      }
    }
  }
  
private:
  DeviceManager &deviceManager;
  juce::String currentAliasName;
};

class PresetManager;

class DeviceSetupComponent : public juce::Component,
                              public RawInputManager::Listener {
public:
  DeviceSetupComponent(DeviceManager &deviceMgr, RawInputManager &rawInputMgr, PresetManager* presetMgr = nullptr);
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
  PresetManager* presetManager;

  // Models
  AliasListModel aliasModel;
  HardwareListModel hardwareModel;

  // UI Elements
  juce::ListBox aliasListBox;
  juce::ListBox hardwareListBox;
  juce::Label aliasHeaderLabel;
  juce::Label hardwareHeaderLabel;
  juce::TextButton addAliasButton;
  juce::TextButton deleteAliasButton;
  juce::TextButton renameButton;
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
