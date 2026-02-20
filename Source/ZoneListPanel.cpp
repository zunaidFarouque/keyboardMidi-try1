#include "ZoneListPanel.h"

ZoneListPanel::ZoneListPanel(ZoneManager *zoneMgr)
    : zoneManager(zoneMgr) {
  addAndMakeVisible(listBox);
  listBox.setModel(this);
  listBox.setRowHeight(24);

  addAndMakeVisible(addButton);
  addButton.setButtonText("Add Zone");
  addButton.onClick = [this] {
    if (zoneManager) {
      auto newZone = zoneManager->createDefaultZone();
      listBox.updateContent();
      // Select the new zone
      int index = static_cast<int>(zoneManager->getZones().size()) - 1;
      listBox.selectRow(index);
    }
  };

  addAndMakeVisible(removeButton);
  removeButton.setButtonText("Remove");
  removeButton.onClick = [this] {
    int selectedRow = listBox.getSelectedRow();
    if (selectedRow >= 0 && zoneManager) {
      const auto &zones = zoneManager->getZones();
      if (selectedRow < static_cast<int>(zones.size())) {
        zoneManager->removeZone(zones[selectedRow]);
        listBox.updateContent();
        // Clear selection
        listBox.deselectAllRows();
      }
    }
  };

  if (zoneManager) {
    zoneManager->addChangeListener(this);
  }
}

ZoneListPanel::~ZoneListPanel() {
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
}

void ZoneListPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void ZoneListPanel::resized() {
  auto area = getLocalBounds().reduced(4);
  
  auto buttonArea = area.removeFromBottom(30);
  removeButton.setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(4);
  addButton.setBounds(buttonArea.removeFromRight(80));

  area.removeFromBottom(4);
  listBox.setBounds(area);
}

int ZoneListPanel::getNumRows() {
  if (!zoneManager)
    return 0;
  return static_cast<int>(zoneManager->getZones().size());
}

void ZoneListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height, bool rowIsSelected) {
  if (!zoneManager)
    return;

  const auto &zones = zoneManager->getZones();
  if (rowNumber < 0 || rowNumber >= static_cast<int>(zones.size()))
    return;

  auto zone = zones[rowNumber];
  if (!zone)
    return;

  // Background
  if (rowIsSelected) {
    g.fillAll(juce::Colour(0xff4a4a4a));
  } else {
    g.fillAll(juce::Colour(0xff2a2a2a));
  }

  // Text
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(zone->name, 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void ZoneListPanel::selectedRowsChanged(int lastRowSelected) {
  if (onSelectionChanged) {
    if (lastRowSelected >= 0 && zoneManager) {
      const auto &zones = zoneManager->getZones();
      if (lastRowSelected < static_cast<int>(zones.size())) {
        onSelectionChanged(zones[lastRowSelected], lastRowSelected);
      } else {
        onSelectionChanged(nullptr, -1);
      }
    } else {
      onSelectionChanged(nullptr, -1);
    }
  }
}

void ZoneListPanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == zoneManager) {
    if (isInitialLoad) {
      // First load: update synchronously for reliable selection restoration
      listBox.updateContent();
      
      // Restore pending selection right after list is updated
      if (pendingSelectionRow >= 0 && getNumRows() > 0) {
        int indexToSet = juce::jmin(pendingSelectionRow, getNumRows() - 1);
        if (indexToSet >= 0) {
          listBox.selectRow(indexToSet);
        }
        pendingSelectionRow = -1;
      }
      
      listBox.repaint();
      sendChangeMessage();
      isInitialLoad = false; // Switch to async for subsequent updates
    } else {
      // Subsequent updates: use async to batch rapid changes and keep UI responsive
      triggerAsyncUpdate();
    }
  }
}

void ZoneListPanel::handleAsyncUpdate() {
  listBox.updateContent();
  
  // Restore pending selection right after list is updated
  if (pendingSelectionRow >= 0 && getNumRows() > 0) {
    int indexToSet = juce::jmin(pendingSelectionRow, getNumRows() - 1);
    if (indexToSet >= 0) {
      listBox.selectRow(indexToSet);
    }
    pendingSelectionRow = -1;
  }
  
  listBox.repaint(); // Force repaint to ensure rendering happens
  sendChangeMessage();
}
