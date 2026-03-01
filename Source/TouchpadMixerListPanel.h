#pragma once
#include "TouchpadMixerManager.h"
#include "TouchpadMixerTypes.h"
#include <JuceHeader.h>
#include <utility>
#include <functional>

class TouchpadMixerListPanel : public juce::Component,
                               public juce::ListBoxModel,
                               public juce::ChangeListener,
                               public juce::AsyncUpdater,
                               public juce::ChangeBroadcaster {
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

  // AsyncUpdater implementation - batches rapid updates
  void handleAsyncUpdate() override;

  /// Returns the currently selected row index in the combined list, or -1 if
  /// none. Use getRowKind() to know whether this refers to a layout or
  /// mapping.
  int getSelectedRowIndex() const;

  /// Select a row programmatically (clamped to valid range).
  void setSelectedRowIndex(int row);

  /// Set filter by group id: -1 = All, 0 = Ungrouped, >0 = specific group
  void setFilterGroupId(int filterGroupId);

  // Set pending selection to restore after next update
  void setPendingSelection(int row) { pendingSelectionRow = row; }

  /// Returns the kind of the given row (Layout / Mapping). Behavior is
  /// undefined for out-of-range indices.
  RowKind getRowKind(int rowIndex) const;

  /// Returns the actual layout index for the selected row, or -1 if none or mapping selected.
  int getSelectedLayoutIndex();
  /// Returns the actual mapping index for the selected row, or -1 if none or layout selected.
  int getSelectedMappingIndex();

  std::function<void(RowKind kind, int index,
                     const TouchpadMixerConfig *layoutCfg,
                     const TouchpadMappingConfig *mappingCfg,
                     int combinedRowIndex)>
      onSelectionChanged;

private:
  TouchpadMixerManager *manager;
  juce::ListBox listBox;
  juce::TextButton addButton;
  juce::TextButton removeButton;

  // Cached row kinds for the combined list (rebuilt when manager changes).
  std::vector<RowKind> rowKinds;
  // Maps displayed row index -> (RowKind, actual layout/mapping index)
  std::vector<std::pair<RowKind, int>> rowToSource;
  int filterGroupId_ = -1; // -1=all, 0=ungrouped, >0=group id
  int pendingSelectionRow = -1; // Selection to restore after update
  bool isInitialLoad = true; // Track first load for synchronous updates

  void rebuildRowKinds();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerListPanel)
};
