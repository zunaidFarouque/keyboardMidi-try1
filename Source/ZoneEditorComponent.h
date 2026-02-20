#pragma once
#include "ZoneListPanel.h"
#include "ZonePropertiesPanel.h"
#include "ZoneManager.h"
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "ScaleLibrary.h"
#include <JuceHeader.h>

class SettingsManager;

class ZoneEditorComponent : public juce::Component, public juce::ChangeListener, public juce::Timer {
public:
  ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib, SettingsManager *settingsMgr = nullptr);
  ~ZoneEditorComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // UI state persistence
  void saveUiState(SettingsManager &settings) const;
  void loadUiState(SettingsManager &settings);

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // Timer implementation for retry mechanism
  void timerCallback() override;

private:
  // 1. Data/Managers
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;
  SettingsManager *settingsManager;

  // 2. Content Components (Must live longer than containers)
  ZoneListPanel listPanel;
  ZonePropertiesPanel propertiesPanel;

  // 3. Containers (Must die first)
  juce::Viewport propertiesViewport;
  int savedPropertiesScrollY = 0;

  // Resizable layout for list and properties
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar resizerBar;

  // Flag to prevent persist-on-change during loadUiState()
  bool isLoadingUiState = false;
  int pendingSelectionIndex = -1; // Selection to restore after list populates
  int loadRetryCount = 0; // Retry counter for delayed selection restoration

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneEditorComponent)
};
