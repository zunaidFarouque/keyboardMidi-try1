#pragma once
#include "ZoneManager.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

class ZoneListPanel : public juce::Component,
                      public juce::ListBoxModel,
                      public juce::ChangeListener,
                      public juce::AsyncUpdater,
                      public juce::ChangeBroadcaster {
public:
  ZoneListPanel(ZoneManager *zoneMgr);
  ~ZoneListPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // ListBoxModel implementation
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) override;
  void selectedRowsChanged(int lastRowSelected) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // AsyncUpdater implementation - batches rapid updates
  void handleAsyncUpdate() override;

  // Callback for selection changes: (zone, rowIndex)
  // rowIndex is the selected row index, or -1 if deselected
  std::function<void(std::shared_ptr<Zone>, int rowIndex)> onSelectionChanged;

  // Selection helpers
  int getSelectedRow() const { return listBox.getSelectedRow(); }
  void setSelectedRow(int row) { listBox.selectRow(row); }
  
  // Set pending selection to restore after next update
  void setPendingSelection(int row) { pendingSelectionRow = row; }

private:
  ZoneManager *zoneManager;
  juce::ListBox listBox;
  juce::TextButton addButton;
  juce::TextButton removeButton;
  int pendingSelectionRow = -1; // Selection to restore after update
  bool isInitialLoad = true; // Track first load for synchronous updates

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneListPanel)
};
