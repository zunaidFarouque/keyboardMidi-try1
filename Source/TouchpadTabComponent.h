#pragma once
#include "TouchpadGroupsPanel.h"
#include "TouchpadMixerEditorComponent.h"
#include "TouchpadMixerListPanel.h"
#include <JuceHeader.h>
#include <functional>

class SettingsManager;
class ScaleLibrary;

class TouchpadTabComponent : public juce::Component, public juce::ChangeListener, public juce::Timer {
public:
  explicit TouchpadTabComponent(TouchpadMixerManager *mgr,
                                SettingsManager *settingsMgr = nullptr,
                                ScaleLibrary *scaleLib = nullptr);
  ~TouchpadTabComponent() override;

  /// Notified when layout/mapping selection changes: (layoutIndex, layerId, layoutGroupId).
  /// layoutIndex -1 = mapping selected. layoutGroupId is the selected entry's group (0 = no group); used to show that group in the visualizer.
  std::function<void(int layoutIndex, int layerId, int layoutGroupId)>
      onSelectionChangedForVisualizer;

  /// Sync visualizer with current list selection (call when tab is shown or
  /// after preset load).
  void refreshVisualizerSelection();

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
  TouchpadMixerManager *manager;
  SettingsManager *settingsManager;
  TouchpadGroupsPanel groupsPanel;
  TouchpadMixerListPanel listPanel;
  TouchpadMixerEditorComponent editorPanel;
  juce::Viewport editorViewport;
  juce::StretchableLayoutManager layout;
  juce::StretchableLayoutResizerBar groupsResizerBar;
  juce::StretchableLayoutResizerBar resizerBar;

  // Flag to prevent persist-on-change during loadUiState()
  bool isLoadingUiState = false;
  int pendingSelectionIndex = -1; // Selection to restore after list populates
  int loadRetryCount = 0; // Retry counter for delayed selection restoration

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadTabComponent)
};
