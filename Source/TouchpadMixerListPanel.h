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

  enum class RowKind { Layout, Mapping };

  void paint(juce::Graphics &) override;
  void resized() override;

  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void selectedRowsChanged(int lastRowSelected) override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  /// Returns the currently selected row index in the combined list, or -1 if
  /// none. Use getRowKind() to know whether this refers to a layout or
  /// mapping.
  int getSelectedRowIndex() const;

  /// Returns the kind of the given row (Layout / Mapping). Behavior is
  /// undefined for out-of-range indices.
  RowKind getRowKind(int rowIndex) const;

  std::function<void(RowKind kind, int index,
                     const TouchpadMixerConfig *layoutCfg,
                     const TouchpadMappingConfig *mappingCfg)>
      onSelectionChanged;

private:
  TouchpadMixerManager *manager;
  juce::ListBox listBox;
  juce::TextButton addButton;
  juce::TextButton removeButton;
  juce::TextButton groupsButton;

  // Cached row kinds for the combined list (rebuilt when manager changes).
  std::vector<RowKind> rowKinds;

  void rebuildRowKinds();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerListPanel)
};
