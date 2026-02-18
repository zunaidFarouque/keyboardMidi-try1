#include "TouchpadMixerListPanel.h"
#include "MappingTypes.h"

namespace {

struct TouchpadLayoutPreset {
  juce::String id;
  juce::String label;
  std::vector<TouchpadMixerConfig> configs;
};

TouchpadMixerConfig makeDefaultTouchpadConfig(TouchpadType type,
                                              const juce::String &name) {
  TouchpadMixerConfig cfg;
  cfg.type = type;
  cfg.name = name.toStdString();
  return cfg;
}

const std::vector<TouchpadLayoutPreset> &getTouchpadLayoutPresets() {
  static const std::vector<TouchpadLayoutPreset> presets = [] {
    std::vector<TouchpadLayoutPreset> v;

    // Full-screen 4x4 Drum Pad (note grid).
    {
      TouchpadMixerConfig drum =
          makeDefaultTouchpadConfig(TouchpadType::DrumPad, "4x4 Drum Pad");
      drum.drumPadRows = 4;
      drum.drumPadColumns = 4;
      drum.region.left = 0.0f;
      drum.region.top = 0.0f;
      drum.region.right = 1.0f;
      drum.region.bottom = 1.0f;
      v.push_back({"drum-4x4-full", "Full-screen 4x4 Drum Pad", {drum}});
    }

    // Full-width Mixer strip at bottom (for FX CCs).
    {
      TouchpadMixerConfig mix = makeDefaultTouchpadConfig(
          TouchpadType::Mixer, "Bottom Mixer Strip");
      mix.numFaders = 8;
      mix.region.left = 0.0f;
      mix.region.right = 1.0f;
      mix.region.top = 0.7f;
      mix.region.bottom = 1.0f;
      v.push_back(
          {"mixer-bottom-strip", "Bottom mixer strip (bottom 30%)", {mix}});
    }

    // Left-half Drum Pad, right-half Mixer strip (two layouts).
    {
      TouchpadMixerConfig leftDrum =
          makeDefaultTouchpadConfig(TouchpadType::DrumPad, "Left Drum Pad");
      leftDrum.drumPadRows = 4;
      leftDrum.drumPadColumns = 4;
      leftDrum.region.left = 0.0f;
      leftDrum.region.top = 0.0f;
      leftDrum.region.right = 0.5f;
      leftDrum.region.bottom = 1.0f;

      TouchpadMixerConfig rightMix = makeDefaultTouchpadConfig(
          TouchpadType::Mixer, "Right Mixer Strip");
      rightMix.numFaders = 8;
      rightMix.region.left = 0.5f;
      rightMix.region.top = 0.0f;
      rightMix.region.right = 1.0f;
      rightMix.region.bottom = 1.0f;

      v.push_back({"split-drum-mixer", "Left Drum Pad + Right Mixer",
                   {leftDrum, rightMix}});
    }

    return v;
  }();
  return presets;
}

} // namespace

TouchpadMixerListPanel::TouchpadMixerListPanel(TouchpadMixerManager *mgr)
    : manager(mgr) {
  addAndMakeVisible(listBox);
  listBox.setModel(this);
  listBox.setRowHeight(24);

  addAndMakeVisible(groupsButton);
  groupsButton.setButtonText("Groups...");
  groupsButton.onClick = [this] {
    if (!manager)
      return;

    // Simple dialog for managing layout groups.
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
        // Update rename on every keypress, Enter, or when focus is lost.
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

      // Local cache: vector of (id, name)
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
        // After adding, select the new group and focus the rename editor.
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
          return; // Don't allow empty names
        auto currentName = groups[(size_t)row].second;
        if (text == currentName)
          return; // No change
        manager->renameGroup(id, text);
        refreshFromManager();
        // Restore selection and repaint to show updated name.
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
   #if JUCE_MODAL_LOOPS_PERMITTED
    opts.launchAsync();
   #else
    opts.launchAsync();
   #endif
  };
  addAndMakeVisible(addButton);
  addButton.setButtonText("Add Entry");
  addButton.onClick = [this] {
    if (!manager)
      return;

    juce::PopupMenu menu;
    int nextId = 1;
    const int emptyLayoutId = nextId++;
    menu.addItem(emptyLayoutId, "Empty layout");
    const int emptyMappingId = nextId++;
    menu.addItem(emptyMappingId, "Empty touchpad mapping");

    const auto &presets = getTouchpadLayoutPresets();
    int firstPresetId = nextId;
    if (!presets.empty()) {
      menu.addSeparator();
      for (size_t i = 0; i < presets.size(); ++i) {
        const auto &p = presets[i];
        menu.addItem(nextId++, p.label);
      }
    }

    menu.showMenuAsync(
        juce::PopupMenu::Options(),
        [this, emptyLayoutId, emptyMappingId, firstPresetId](int result) {
          if (result <= 0 || !manager)
            return;

          if (result == emptyLayoutId) {
            TouchpadMixerConfig def;
            def.name = "Touchpad Mixer";
            manager->addLayout(def);
          } else if (result == emptyMappingId) {
            TouchpadMappingConfig cfg;
            cfg.name = "Touchpad Mapping";
            juce::ValueTree m("Mapping");
            m.setProperty("inputAlias", "Touchpad", nullptr);
            m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1Down,
                          nullptr);
            m.setProperty("type", "Note", nullptr);
            m.setProperty("channel", 1, nullptr);
            m.setProperty("data1", 60, nullptr);
            m.setProperty("data2", 100, nullptr);
            cfg.mapping = m;
            manager->addTouchpadMapping(cfg);
          } else if (result >= firstPresetId) {
            const auto &presets = getTouchpadLayoutPresets();
            int presetIdx = result - firstPresetId;
            if (presetIdx >= 0 &&
                presetIdx < static_cast<int>(presets.size())) {
              const auto &preset = presets[(size_t)presetIdx];
              for (const auto &cfg : preset.configs)
                manager->addLayout(cfg);
            }
          }

          listBox.updateContent();
          if (manager) {
            int totalRows = static_cast<int>(manager->getLayouts().size() +
                                             manager->getTouchpadMappings().size());
            if (totalRows > 0)
              listBox.selectRow(totalRows - 1);
          }
        });
  };

  addAndMakeVisible(removeButton);
  removeButton.setButtonText("Remove");
  removeButton.onClick = [this] {
    int row = listBox.getSelectedRow();
    if (row >= 0 && manager) {
      rebuildRowKinds();
      if (row >= 0 && row < static_cast<int>(rowKinds.size())) {
        RowKind kind = rowKinds[(size_t)row];
        if (kind == RowKind::Layout) {
          manager->removeLayout(row);
        } else {
          int mappingIndex = row - static_cast<int>(manager->getLayouts().size());
          if (mappingIndex >= 0)
            manager->removeTouchpadMapping(mappingIndex);
        }
      }
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
  buttonArea.removeFromRight(4);
  groupsButton.setBounds(buttonArea.removeFromRight(90));
  area.removeFromBottom(4);
  listBox.setBounds(area);
}

int TouchpadMixerListPanel::getNumRows() {
  if (!manager)
    return 0;
  rebuildRowKinds();
  return static_cast<int>(rowKinds.size());
}

void TouchpadMixerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                              int width, int height,
                                              bool rowIsSelected) {
  if (!manager)
    return;
  rebuildRowKinds();
  if (rowNumber < 0 || rowNumber >= static_cast<int>(rowKinds.size()))
    return;
  if (rowIsSelected)
    g.fillAll(juce::Colour(0xff4a4a4a));
  else
    g.fillAll(juce::Colour(0xff2a2a2a));
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();

  const RowKind kind = rowKinds[(size_t)rowNumber];
  juce::String label;
  if (kind == RowKind::Layout) {
    const int layoutIndex = rowNumber;
    if (layoutIndex >= 0 &&
        layoutIndex < static_cast<int>(layouts.size())) {
      label = juce::String(layouts[(size_t)layoutIndex].name);
    }
  } else {
    const int mappingIndex = rowNumber - static_cast<int>(layouts.size());
    if (mappingIndex >= 0 &&
        mappingIndex < static_cast<int>(mappings.size())) {
      const auto &m = mappings[(size_t)mappingIndex];
      label = "[Map] " + juce::String(m.name);
    }
  }
  if (label.isEmpty())
    label = "<invalid>";
  g.drawText(label, 8, 0, width - 16, height,
             juce::Justification::centredLeft);
}

int TouchpadMixerListPanel::getSelectedRowIndex() const {
  return listBox.getSelectedRow();
}

void TouchpadMixerListPanel::selectedRowsChanged(int lastRowSelected) {
  if (!onSelectionChanged || !manager) {
    return;
  }

  rebuildRowKinds();

  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();

  if (lastRowSelected < 0 ||
      lastRowSelected >= static_cast<int>(rowKinds.size())) {
    onSelectionChanged(RowKind::Layout, -1, nullptr, nullptr);
    return;
  }

  RowKind kind = rowKinds[(size_t)lastRowSelected];
  if (kind == RowKind::Layout) {
    int layoutIndex = lastRowSelected;
    if (layoutIndex >= 0 &&
        layoutIndex < static_cast<int>(layouts.size())) {
      onSelectionChanged(kind, layoutIndex,
                         &layouts[(size_t)layoutIndex], nullptr);
    } else {
      onSelectionChanged(kind, -1, nullptr, nullptr);
    }
  } else {
    int mappingIndex = lastRowSelected -
                       static_cast<int>(layouts.size());
    if (mappingIndex >= 0 &&
        mappingIndex < static_cast<int>(mappings.size())) {
      onSelectionChanged(kind, mappingIndex, nullptr,
                         &mappings[(size_t)mappingIndex]);
    } else {
      onSelectionChanged(kind, -1, nullptr, nullptr);
    }
  }
}

void TouchpadMixerListPanel::changeListenerCallback(juce::ChangeBroadcaster *) {
  juce::MessageManager::callAsync([this] {
    rebuildRowKinds();
    listBox.updateContent();
  });
}

void TouchpadMixerListPanel::rebuildRowKinds() {
  rowKinds.clear();
  if (!manager)
    return;
  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();
  rowKinds.reserve(layouts.size() + mappings.size());
  for (size_t i = 0; i < layouts.size(); ++i)
    rowKinds.push_back(RowKind::Layout);
  for (size_t i = 0; i < mappings.size(); ++i)
    rowKinds.push_back(RowKind::Mapping);
}

TouchpadMixerListPanel::RowKind
TouchpadMixerListPanel::getRowKind(int rowIndex) const {
  if (rowIndex < 0 || rowIndex >= static_cast<int>(rowKinds.size()))
    return RowKind::Layout;
  return rowKinds[(size_t)rowIndex];
}
