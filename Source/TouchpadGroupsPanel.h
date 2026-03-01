#pragma once
#include "TouchpadMixerManager.h"
#include <JuceHeader.h>
#include <functional>

class TouchpadGroupsPanel : public juce::Component,
                            public juce::ListBoxModel,
                            public juce::ChangeListener {
public:
  explicit TouchpadGroupsPanel(TouchpadMixerManager *mgr);
  ~TouchpadGroupsPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // ListBoxModel implementation
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void selectedRowsChanged(int lastRowSelected) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  /// Callback when user selects a filter. filterGroupId: -1 = All, 0 = Ungrouped, >0 = group id
  std::function<void(int filterGroupId)> onGroupSelected;

  /// Set selected filter programmatically
  void setSelectedFilter(int filterGroupId);

private:
  TouchpadMixerManager *manager;
  juce::ListBox listBox;
  int selectedFilterGroupId = -1;

  /// Row index -> filterGroupId: row 0 = -1 (All), row 1 = 0 (Ungrouped), row 2+ = group id
  int rowToFilterGroupId(int row) const;

  /// Find row index for a given filterGroupId
  int filterGroupIdToRow(int filterGroupId) const;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadGroupsPanel)
};
