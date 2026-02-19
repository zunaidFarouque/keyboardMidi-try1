#pragma once
#include "TouchpadMixerEditorComponent.h"
#include "TouchpadMixerListPanel.h"
#include <JuceHeader.h>
#include <functional>

class SettingsManager;

class TouchpadTabComponent : public juce::Component {
public:
  explicit TouchpadTabComponent(TouchpadMixerManager *mgr,
                                SettingsManager *settingsMgr = nullptr);
  ~TouchpadTabComponent() override;

  /// Notified when layout selection changes: (layoutIndex, layerId). (-1, 0)
  /// when none.
  std::function<void(int layoutIndex, int layerId)>
      onSelectionChangedForVisualizer;

  /// Sync visualizer with current list selection (call when tab is shown or
  /// after preset load).
  void refreshVisualizerSelection();

  void paint(juce::Graphics &) override;
  void resized() override;

  // UI state persistence
  void saveUiState(SettingsManager &settings) const;
  void loadUiState(SettingsManager &settings);

private:
  TouchpadMixerManager *manager;
  TouchpadMixerListPanel listPanel;
  TouchpadMixerEditorComponent editorPanel;
  juce::Viewport editorViewport;
  juce::StretchableLayoutManager layout;
  juce::StretchableLayoutResizerBar resizerBar;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadTabComponent)
};
