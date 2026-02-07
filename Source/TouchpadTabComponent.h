#pragma once
#include "TouchpadMixerEditorComponent.h"
#include "TouchpadMixerListPanel.h"
#include <JuceHeader.h>
#include <functional>

class TouchpadTabComponent : public juce::Component {
public:
  explicit TouchpadTabComponent(TouchpadMixerManager *mgr);
  ~TouchpadTabComponent() override;

  /// Notified when strip selection changes: (stripIndex, layerId). (-1, 0) when none.
  std::function<void(int stripIndex, int layerId)> onSelectionChangedForVisualizer;

  /// Sync visualizer with current list selection (call when tab is shown or after preset load).
  void refreshVisualizerSelection();

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  TouchpadMixerManager *manager;
  TouchpadMixerListPanel listPanel;
  TouchpadMixerEditorComponent editorPanel;
  juce::Viewport editorViewport;
  juce::StretchableLayoutManager layout;
  juce::StretchableLayoutResizerBar resizerBar;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadTabComponent)
};
