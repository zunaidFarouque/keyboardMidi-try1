#pragma once
#include "TouchpadMixerManager.h"
#include "TouchpadMixerTypes.h"
#include <JuceHeader.h>
#include <functional>

class TouchpadMixerListPanel : public juce::Component,
                               public juce::ListBoxModel,
                               public juce::ChangeListener {
public:
  explicit TouchpadMixerListPanel(TouchpadMixerManager *mgr);
  ~TouchpadMixerListPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void selectedRowsChanged(int lastRowSelected) override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  /// Returns the currently selected row index, or -1 if none.
  int getSelectedLayoutIndex() const;

  std::function<void(int layoutIndex, const TouchpadMixerConfig *)>
      onSelectionChanged;

private:
  TouchpadMixerManager *manager;
  juce::ListBox listBox;
  juce::TextButton addButton;
  juce::TextButton removeButton;
  juce::TextButton groupsButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerListPanel)
};
