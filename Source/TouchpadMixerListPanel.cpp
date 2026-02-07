#include "TouchpadMixerListPanel.h"

TouchpadMixerListPanel::TouchpadMixerListPanel(TouchpadMixerManager *mgr)
    : manager(mgr) {
  addAndMakeVisible(listBox);
  listBox.setModel(this);
  listBox.setRowHeight(24);

  addAndMakeVisible(addButton);
  addButton.setButtonText("Add Strip");
  addButton.onClick = [this] {
    if (manager) {
      TouchpadMixerConfig def;
      def.name = "Touchpad Mixer";
      manager->addStrip(def);
      listBox.updateContent();
      int n = static_cast<int>(manager->getStrips().size());
      if (n > 0)
        listBox.selectRow(n - 1);
    }
  };

  addAndMakeVisible(removeButton);
  removeButton.setButtonText("Remove");
  removeButton.onClick = [this] {
    int row = listBox.getSelectedRow();
    if (row >= 0 && manager) {
      manager->removeStrip(row);
      listBox.updateContent();
      listBox.deselectAllRows();
    }
  };

  if (manager)
    manager->addChangeListener(this);
}

TouchpadMixerListPanel::~TouchpadMixerListPanel() {
  if (manager)
    manager->removeChangeListener(this);
}

void TouchpadMixerListPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void TouchpadMixerListPanel::resized() {
  auto area = getLocalBounds().reduced(4);
  auto buttonArea = area.removeFromBottom(30);
  removeButton.setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(4);
  addButton.setBounds(buttonArea.removeFromRight(80));
  area.removeFromBottom(4);
  listBox.setBounds(area);
}

int TouchpadMixerListPanel::getNumRows() {
  if (!manager)
    return 0;
  return static_cast<int>(manager->getStrips().size());
}

void TouchpadMixerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                              int width, int height,
                                              bool rowIsSelected) {
  if (!manager)
    return;
  auto strips = manager->getStrips();
  if (rowNumber < 0 || rowNumber >= static_cast<int>(strips.size()))
    return;
  if (rowIsSelected)
    g.fillAll(juce::Colour(0xff4a4a4a));
  else
    g.fillAll(juce::Colour(0xff2a2a2a));
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(juce::String(strips[(size_t)rowNumber].name), 8, 0, width - 16,
             height, juce::Justification::centredLeft);
}

int TouchpadMixerListPanel::getSelectedStripIndex() const {
  return listBox.getSelectedRow();
}

void TouchpadMixerListPanel::selectedRowsChanged(int lastRowSelected) {
  if (onSelectionChanged) {
    if (lastRowSelected >= 0 && manager) {
      auto strips = manager->getStrips();
      if (lastRowSelected < static_cast<int>(strips.size()))
        onSelectionChanged(lastRowSelected, &strips[(size_t)lastRowSelected]);
      else
        onSelectionChanged(-1, nullptr);
    } else {
      onSelectionChanged(-1, nullptr);
    }
  }
}

void TouchpadMixerListPanel::changeListenerCallback(juce::ChangeBroadcaster *) {
  juce::MessageManager::callAsync([this] { listBox.updateContent(); });
}
