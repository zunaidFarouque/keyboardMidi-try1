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

  addAndMakeVisible(groupsButton);
  groupsButton.setButtonText("Groups...");
  groupsButton.onClick = [this] {
    if (!manager)
      return;

    class GroupsDialog : public juce::Component, public juce::ListBoxModel {
    public:
      explicit GroupsDialog(TouchpadMixerManager *mgr) : manager(mgr) {
        addAndMakeVisible(listBox);
        listBox.setModel(this);
        listBox.setRowHeight(24);

        addButton.setButtonText("Add");
        removeButton.setButtonText("Remove");
        renameLabel.setText("Name:", juce::dontSendNotification);
        addAndMakeVisible(addButton);
        addAndMakeVisible(removeButton);
        addAndMakeVisible(renameLabel);
        addAndMakeVisible(renameEditor);

        addButton.onClick = [this] { addGroup(); };
        removeButton.onClick = [this] { removeSelectedGroup(); };
        renameEditor.onTextChange = [this] { confirmRename(); };
        renameEditor.onReturnKey = [this] { confirmRename(); };
        renameEditor.onFocusLost = [this] { confirmRename(); };

        refreshFromManager();
      }

      int getNumRows() override { return (int)groups.size(); }

      void paintListBoxItem(int row, juce::Graphics &g, int width, int height,
                            bool rowIsSelected) override {
        if (row < 0 || row >= (int)groups.size())
          return;
        if (rowIsSelected)
          g.fillAll(juce::Colour(0xff4a4a4a));
        else
          g.fillAll(juce::Colour(0xff2a2a2a));
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(groups[(size_t)row].second, 8, 0, width - 16, height,
                   juce::Justification::centredLeft);
      }

      void selectedRowsChanged(int lastRowSelected) override {
        if (lastRowSelected >= 0 && lastRowSelected < (int)groups.size()) {
          renameEditor.setText(groups[(size_t)lastRowSelected].second,
                               juce::dontSendNotification);
        } else {
          renameEditor.setText({}, juce::dontSendNotification);
        }
      }

      void resized() override {
        auto area = getLocalBounds().reduced(8);
        auto bottom = area.removeFromBottom(30);
        removeButton.setBounds(bottom.removeFromRight(80));
        bottom.removeFromRight(4);
        addButton.setBounds(bottom.removeFromRight(80));

        auto nameArea = area.removeFromBottom(24);
        renameLabel.setBounds(nameArea.removeFromLeft(60));
        renameEditor.setBounds(nameArea);

        listBox.setBounds(area.reduced(0, 4));
      }

    private:
      TouchpadMixerManager *manager = nullptr;
      juce::ListBox listBox{"Groups", this};
      juce::TextButton addButton;
      juce::TextButton removeButton;
      juce::Label renameLabel;
      juce::TextEditor renameEditor;

      std::vector<std::pair<int, juce::String>> groups;

      void refreshFromManager() {
        groups.clear();
        if (!manager)
          return;
        for (const auto &g : manager->getGroups()) {
          groups.emplace_back(g.id, juce::String(g.name));
        }
        listBox.updateContent();
      }

      void addGroup() {
        if (!manager)
          return;
        int nextId = 1;
        for (const auto &g : manager->getGroups())
          nextId = juce::jmax(nextId, g.id + 1);
        TouchpadLayoutGroup g;
        g.id = nextId;
        g.name = ("Group " + juce::String(nextId)).toStdString();
        manager->addGroup(g);
        refreshFromManager();
        int newRow = -1;
        for (int i = 0; i < (int)groups.size(); ++i) {
          if (groups[(size_t)i].first == nextId) {
            newRow = i;
            break;
          }
        }
        if (newRow >= 0) {
          listBox.selectRow(newRow);
          renameEditor.setText(groups[(size_t)newRow].second,
                               juce::dontSendNotification);
          renameEditor.grabKeyboardFocus();
          renameEditor.setHighlightedRegion(
              juce::Range<int>(0, renameEditor.getText().length()));
        }
      }

      void removeSelectedGroup() {
        if (!manager)
          return;
        int row = listBox.getSelectedRow();
        if (row < 0 || row >= (int)groups.size())
          return;
        int id = groups[(size_t)row].first;
        manager->removeGroup(id);
        refreshFromManager();
      }

      void confirmRename() {
        if (!manager)
          return;
        int row = listBox.getSelectedRow();
        if (row < 0 || row >= (int)groups.size())
          return;
        int id = groups[(size_t)row].first;
        auto text = renameEditor.getText().trim();
        if (text.isEmpty())
          return;
        auto currentName = groups[(size_t)row].second;
        if (text == currentName)
          return;
        manager->renameGroup(id, text);
        refreshFromManager();
        if (row >= 0 && row < (int)groups.size()) {
          listBox.selectRow(row);
          listBox.repaint();
        }
      }
    };

    juce::DialogWindow::LaunchOptions opts;
    opts.dialogTitle = "Touchpad Layout Groups";
    opts.dialogBackgroundColour = juce::Colour(0xff222222);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.content.setOwned(new GroupsDialog(manager));
    opts.content->setSize(300, 260);
    opts.launchAsync();
  };

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
  auto area = getLocalBounds();
  auto buttonArea = area.removeFromBottom(30);
  groupsButton.setBounds(buttonArea.removeFromRight(90).reduced(2));
  listBox.setBounds(area);
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
