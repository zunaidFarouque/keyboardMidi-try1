#include "TouchpadGroupsPanel.h"

TouchpadGroupsPanel::TouchpadGroupsPanel(TouchpadMixerManager *mgr)
    : manager(mgr) {
  listBox.setModel(this);
  listBox.setRowHeight(24);
  listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
  listBox.setColour(juce::ListBox::outlineColourId,
                    juce::Colour(0xff404040));
  listBox.setOutlineThickness(1);
  listBox.setMultipleSelectionEnabled(false);
  addAndMakeVisible(listBox);

  if (manager)
    manager->addChangeListener(this);

  setSelectedFilter(-1);
}

TouchpadGroupsPanel::~TouchpadGroupsPanel() {
  if (manager)
    manager->removeChangeListener(this);
}

void TouchpadGroupsPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
}

void TouchpadGroupsPanel::resized() {
  listBox.setBounds(getLocalBounds());
}

int TouchpadGroupsPanel::rowToFilterGroupId(int row) const {
  if (row < 0)
    return -1;
  if (row == 0)
    return -1; // All Entries
  if (row == 1)
    return 0; // Ungrouped
  auto groups = manager ? manager->getGroups() : std::vector<TouchpadLayoutGroup>{};
  int groupRow = row - 2;
  if (groupRow >= 0 && groupRow < static_cast<int>(groups.size()))
    return groups[(size_t)groupRow].id;
  return -1;
}

int TouchpadGroupsPanel::filterGroupIdToRow(int filterGroupId) const {
  if (filterGroupId == -1)
    return 0;
  if (filterGroupId == 0)
    return 1;
  auto groups = manager ? manager->getGroups() : std::vector<TouchpadLayoutGroup>{};
  for (size_t i = 0; i < groups.size(); ++i) {
    if (groups[i].id == filterGroupId)
      return static_cast<int>(i) + 2;
  }
  return 0; // Fallback to All Entries
}

int TouchpadGroupsPanel::getNumRows() {
  if (!manager)
    return 2; // All Entries, Ungrouped
  return 2 + static_cast<int>(manager->getGroups().size());
}

void TouchpadGroupsPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                           int width, int height,
                                           bool rowIsSelected) {
  juce::String label;
  if (rowNumber == 0)
    label = "All Entries";
  else if (rowNumber == 1)
    label = "Ungrouped";
  else if (manager) {
    auto groups = manager->getGroups();
    int groupRow = rowNumber - 2;
    if (groupRow >= 0 && groupRow < static_cast<int>(groups.size()))
      label = juce::String(groups[(size_t)groupRow].name);
  }

  const int pad = 2;
  auto area = juce::Rectangle<int>(0, 0, width, height).reduced(pad, 0);
  if (rowIsSelected) {
    g.setColour(juce::Colour(0xff3d5a80));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    g.setColour(juce::Colours::lightblue.withAlpha(0.5f));
    g.drawRoundedRectangle(area.toFloat(), 4.0f, 1.0f);
  }

  g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::grey);
  g.setFont(15.0f);
  g.drawText(label, 10, 0, width - 10, height,
             juce::Justification::centredLeft, true);
}

void TouchpadGroupsPanel::selectedRowsChanged(int lastRowSelected) {
  int filterId = rowToFilterGroupId(lastRowSelected);
  selectedFilterGroupId = filterId;
  if (onGroupSelected)
    onGroupSelected(filterId);
}

void TouchpadGroupsPanel::changeListenerCallback(juce::ChangeBroadcaster *) {
  listBox.updateContent();
  listBox.repaint();
  // If selected group was removed, switch to All Entries
  if (selectedFilterGroupId > 0 && manager) {
    bool found = false;
    for (const auto &g : manager->getGroups()) {
      if (g.id == selectedFilterGroupId) {
        found = true;
        break;
      }
    }
    if (!found) {
      selectedFilterGroupId = -1;
      listBox.selectRow(0);
      if (onGroupSelected)
        onGroupSelected(-1);
    }
  }
}

void TouchpadGroupsPanel::setSelectedFilter(int filterGroupId) {
  selectedFilterGroupId = filterGroupId;
  int row = filterGroupIdToRow(filterGroupId);
  listBox.selectRow(row);
  if (onGroupSelected)
    onGroupSelected(filterGroupId);
}
