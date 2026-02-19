#pragma once
#include "ZoneManager.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>

class ZoneListPanel : public juce::Component,
                      public juce::ListBoxModel,
                      public juce::ChangeListener {
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

  // Callback for selection changes
  std::function<void(std::shared_ptr<Zone>)> onSelectionChanged;

  // Selection helpers
  int getSelectedRow() const { return listBox.getSelectedRow(); }
  void setSelectedRow(int row) { listBox.selectRow(row); }

private:
  ZoneManager *zoneManager;
  juce::ListBox listBox;
  juce::TextButton addButton;
  juce::TextButton removeButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneListPanel)
};
