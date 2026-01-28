#pragma once
#include "DeviceManager.h"
#include "KeyNameUtilities.h"
#include "RawInputManager.h"
#include <JuceHeader.h>

// Forward declaration
class DeviceSetupComponent;

// Separate model classes for each list box
class AliasListModel : public juce::ListBoxModel {
public:
  AliasListModel(DeviceManager &dm, DeviceSetupComponent *parent)
      : deviceManager(dm), parentComponent(parent) {}

  int getNumRows() override {
    // Phase 46: extra row 0 for "[ Unassigned Devices ]"
    return deviceManager.getAllAliases().size() + 1;
  }

  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override {
    if (rowIsSelected)
      g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    auto aliases = deviceManager.getAllAliases();
    if (rowNumber == 0) {
      // Special virtual row for unassigned devices
      g.setColour(juce::Colours::orange);
      g.setFont(juce::Font(14.0f, juce::Font::italic));
      g.drawText("[ Unassigned Devices ]", 4, 0, width - 8, height,
                 juce::Justification::centredLeft, true);
    } else {
      int aliasIndex = rowNumber - 1;
      if (aliasIndex >= 0 && aliasIndex < aliases.size()) {
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(aliases[aliasIndex], 4, 0, width - 8, height,
                   juce::Justification::centredLeft, true);
      }
    }
  }

  void selectedRowsChanged(int lastRowSelected) override;

private:
  DeviceManager &deviceManager;
  DeviceSetupComponent *parentComponent;
};

class HardwareListModel : public juce::ListBoxModel {
public:
  HardwareListModel(DeviceManager &dm, DeviceSetupComponent *parent)
      : deviceManager(dm), parentComponent(parent) {}

  void setAlias(const juce::String &name) {
    currentAliasName = name;
    showUnassigned = false;
  }

  // Phase 46: special mode for showing unassigned devices (alias row 0)
  void setShowUnassigned(bool shouldShow) {
    showUnassigned = shouldShow;
    if (showUnassigned)
      currentAliasName.clear();
  }

  int getNumRows() override {
    if (showUnassigned) {
      return static_cast<int>(deviceManager.getUnassignedDevices().size());
    }
    if (currentAliasName.isEmpty())
      return 0;
    return deviceManager.getHardwareForAlias(currentAliasName).size();
  }

  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override {
    if (rowIsSelected)
      g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));

    g.setColour(juce::Colours::white);
    g.setFont(14.0f);

    if (showUnassigned) {
      const auto &hardwareIds = deviceManager.getUnassignedDevices();
      if (rowNumber >= 0 && rowNumber < static_cast<int>(hardwareIds.size())) {
        uintptr_t id = hardwareIds[(size_t)rowNumber];
        juce::String text = KeyNameUtilities::getFriendlyDeviceName(id);
        g.drawText(text, 4, 0, width - 8, height,
                   juce::Justification::centredLeft, true);
      }
    } else if (!currentAliasName.isEmpty()) {
      auto hardwareIds = deviceManager.getHardwareForAlias(currentAliasName);
      if (rowNumber < hardwareIds.size()) {
        uintptr_t id = hardwareIds[rowNumber];
        juce::String text = KeyNameUtilities::getFriendlyDeviceName(id);
        g.drawText(text, 4, 0, width - 8, height,
                   juce::Justification::centredLeft, true);
      }
    }
  }

  void selectedRowsChanged(int lastRowSelected) override;

private:
  DeviceManager &deviceManager;
  DeviceSetupComponent *parentComponent = nullptr;
  juce::String currentAliasName;
  bool showUnassigned = false;
};

class PresetManager;

class DeviceSetupComponent : public juce::Component,
                             public RawInputManager::Listener {
public:
  DeviceSetupComponent(DeviceManager &deviceMgr, RawInputManager &rawInputMgr,
                       PresetManager *presetMgr = nullptr);
  ~DeviceSetupComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // Called by AliasListModel when selection changes (needs to be public for
  // model access)
  void onAliasSelected();
  void onHardwareSelected();

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

private:
  DeviceManager &deviceManager;
  RawInputManager &rawInputManager;
  PresetManager *presetManager;

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

  // State (selectedAlias is also in HardwareListModel, but we keep a reference
  // here for convenience)
  bool isScanning = false;
  uintptr_t scannedDeviceHandle = 0;

  // Helpers
  void refreshAliasList();
  void refreshHardwareList();
  void updateButtonStates();
  void addAlias();
  void deleteSelectedAlias();
  void removeSelectedHardware();

  // State
  juce::String selectedAlias;
};
