#include "TouchpadMixerListPanel.h"
#include "MappingTypes.h"

namespace {

struct TouchpadLayoutPreset {
  juce::String id;
  juce::String label;
  std::vector<TouchpadMixerConfig> layouts;
  std::vector<TouchpadMappingConfig> mappings;
};

TouchpadMixerConfig makeDefaultTouchpadConfig(TouchpadType type,
                                              const juce::String &name) {
  TouchpadMixerConfig cfg;
  cfg.type = type;
  cfg.name = name.toStdString();
  return cfg;
}

TouchpadMappingConfig makeXYMapping(const juce::String &name, int eventId,
                                    int ccNumber, float regionLeft,
                                    float regionRight) {
  TouchpadMappingConfig cfg;
  cfg.name = name.toStdString();
  cfg.layerId = 0;
  cfg.layoutGroupId = 0;
  cfg.midiChannel = 1;
  cfg.region.left = regionLeft;
  cfg.region.top = 0.0f;
  cfg.region.right = regionRight;
  cfg.region.bottom = 1.0f;
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", eventId, nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "CC", nullptr);
  m.setProperty("expressionCCMode", "Position", nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("data1", ccNumber, nullptr);
  m.setProperty("touchpadInputMin", 0.0, nullptr);
  m.setProperty("touchpadInputMax", 1.0, nullptr);
  m.setProperty("touchpadOutputMin", 0, nullptr);
  m.setProperty("touchpadOutputMax", 127, nullptr);
  cfg.mapping = m;
  return cfg;
}

TouchpadMappingConfig makePitchBendMapping(const juce::String &name,
                                            const juce::String &mode) {
  TouchpadMappingConfig cfg;
  cfg.name = name.toStdString();
  cfg.layerId = 0;
  cfg.layoutGroupId = 0;
  cfg.midiChannel = 1;
  cfg.region.left = 0.0f;
  cfg.region.top = 0.0f;
  cfg.region.right = 1.0f;
  cfg.region.bottom = 1.0f;
  juce::ValueTree m("Mapping");
  m.setProperty("inputAlias", "Touchpad", nullptr);
  m.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  m.setProperty("type", "Expression", nullptr);
  m.setProperty("adsrTarget", "PitchBend", nullptr);
  m.setProperty("channel", 1, nullptr);
  m.setProperty("touchpadInputMin", 0.0, nullptr);
  m.setProperty("touchpadInputMax", 1.0, nullptr);
  m.setProperty("pitchPadUseCustomRange", true, nullptr);
  m.setProperty("touchpadOutputMin", -2, nullptr);
  m.setProperty("touchpadOutputMax", 2, nullptr);
  m.setProperty("pitchPadMode", mode, nullptr);
  cfg.mapping = m;
  return cfg;
}

const std::vector<TouchpadLayoutPreset> &getTouchpadLayoutPresets() {
  static const std::vector<TouchpadLayoutPreset> presets = [] {
    std::vector<TouchpadLayoutPreset> v;

    // --- Mixers: CC0, Base layer, no group, full region ---
    auto addMixer = [&](const juce::String &label, int numFaders,
                        bool muteButtons) {
      TouchpadMixerConfig mix =
          makeDefaultTouchpadConfig(TouchpadType::Mixer, label);
      mix.layerId = 0;
      mix.layoutGroupId = 0;
      mix.numFaders = numFaders;
      mix.ccStart = 0;
      mix.muteButtonsEnabled = muteButtons;
      mix.region = {0.0f, 0.0f, 1.0f, 1.0f};
      v.push_back({"mixer-" + juce::String(numFaders), label, {mix}, {}});
    };
    addMixer("3-column mixer", 3, false);
    addMixer("4-column mixer (with mute)", 4, true);
    addMixer("5-column mixer", 5, false);

    // --- Drum Pads: Base layer, no group, full region ---
    auto addDrumPad = [&](const juce::String &label, int rows, int cols) {
      TouchpadMixerConfig drum =
          makeDefaultTouchpadConfig(TouchpadType::DrumPad, label);
      drum.layerId = 0;
      drum.layoutGroupId = 0;
      drum.drumPadRows = rows;
      drum.drumPadColumns = cols;
      drum.drumPadMidiNoteStart = 60;
      drum.region = {0.0f, 0.0f, 1.0f, 1.0f};
      v.push_back(
          {"drum-" + juce::String(rows) + "x" + juce::String(cols), label,
           {drum}, {}});
    };
    addDrumPad("2x2 Drum Pad", 2, 2);
    addDrumPad("3x2 Drum Pad", 3, 2);
    addDrumPad("3x3 Drum Pad", 3, 3);
    addDrumPad("4x4 Drum Pad", 4, 4);

    // --- Controllers: XY Pad mappings ---
    {
      std::vector<TouchpadMappingConfig> xyMappings;
      xyMappings.push_back(
          makeXYMapping("XY X (CC1)", TouchpadEvent::Finger1X, 1, 0.0f, 1.0f));
      xyMappings.push_back(
          makeXYMapping("XY Y (CC2)", TouchpadEvent::Finger1Y, 2, 0.0f, 1.0f));
      v.push_back({"xy-pad", "XY Pad (X→CC1, Y→CC2)", {}, xyMappings});
    }
    {
      std::vector<TouchpadMappingConfig> dualMappings;
      dualMappings.push_back(
          makeXYMapping("Left X (CC1)", TouchpadEvent::Finger1X, 1, 0.0f, 0.5f));
      dualMappings.push_back(
          makeXYMapping("Left Y (CC2)", TouchpadEvent::Finger1Y, 2, 0.0f, 0.5f));
      dualMappings.push_back(
          makeXYMapping("Right X (CC3)", TouchpadEvent::Finger1X, 3, 0.5f, 1.0f));
      dualMappings.push_back(
          makeXYMapping("Right Y (CC4)", TouchpadEvent::Finger1Y, 4, 0.5f, 1.0f));
      v.push_back({"xy-dual", "Dual XY Pads (left 50% + right 50%)", {},
                   dualMappings});
    }

    // --- Pitch Bend: ±2 horizontal ---
    {
      v.push_back({"pb-abs", "PB ±2 horizontal (Absolute)", {},
                   {makePitchBendMapping("Pitch Bend ±2", "Absolute")}});
    }
    {
      v.push_back({"pb-rel", "PB ±2 horizontal (Relative)", {},
                   {makePitchBendMapping("Pitch Bend ±2", "Relative")}});
    }

    // --- Combo: Drum Pad + Mixer strip ---
    {
      TouchpadMixerConfig leftDrum =
          makeDefaultTouchpadConfig(TouchpadType::DrumPad, "Left Drum Pad");
      leftDrum.layerId = 0;
      leftDrum.layoutGroupId = 0;
      leftDrum.drumPadRows = 4;
      leftDrum.drumPadColumns = 4;
      leftDrum.region = {0.0f, 0.0f, 0.5f, 1.0f};

      TouchpadMixerConfig rightMix = makeDefaultTouchpadConfig(
          TouchpadType::Mixer, "Right Mixer Strip");
      rightMix.layerId = 0;
      rightMix.layoutGroupId = 0;
      rightMix.numFaders = 8;
      rightMix.ccStart = 0;
      rightMix.region = {0.5f, 0.0f, 1.0f, 1.0f};

      v.push_back({"combo-drum-mixer", "Drum Pad + Mixer strip",
                   {leftDrum, rightMix}, {}});
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
  listBox.setColour(juce::ListBox::outlineColourId, juce::Colour(0xff404040));
  listBox.setOutlineThickness(1);

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
    menu.addSeparator();

    const auto &presets = getTouchpadLayoutPresets();
    int firstPresetId = nextId;

    juce::PopupMenu mixerMenu;
    mixerMenu.addItem(nextId++, "3-column mixer");
    mixerMenu.addItem(nextId++, "4-column mixer (with mute)");
    mixerMenu.addItem(nextId++, "5-column mixer");
    menu.addSubMenu("Mixers", mixerMenu);

    juce::PopupMenu drumMenu;
    drumMenu.addItem(nextId++, "2x2 Drum Pad");
    drumMenu.addItem(nextId++, "3x2 Drum Pad");
    drumMenu.addItem(nextId++, "3x3 Drum Pad");
    drumMenu.addItem(nextId++, "4x4 Drum Pad");
    menu.addSubMenu("Drum Pads", drumMenu);

    juce::PopupMenu controllerMenu;
    controllerMenu.addItem(nextId++, "XY Pad (X→CC1, Y→CC2)");
    controllerMenu.addItem(nextId++, "Dual XY Pads (left 50% + right 50%)");
    menu.addSubMenu("Controllers", controllerMenu);

    juce::PopupMenu pbMenu;
    pbMenu.addItem(nextId++, "PB ±2 horizontal (Absolute)");
    pbMenu.addItem(nextId++, "PB ±2 horizontal (Relative)");
    menu.addSubMenu("Pitch Bend", pbMenu);

    juce::PopupMenu comboMenu;
    comboMenu.addItem(nextId++, "Drum Pad + Mixer strip");
    menu.addSubMenu("Combos", comboMenu);

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
              for (const auto &cfg : preset.layouts)
                manager->addLayout(cfg);
              for (const auto &cfg : preset.mappings)
                manager->addTouchpadMapping(cfg);
            }
          }

          listBox.updateContent();
          if (manager) {
            int n = getNumRows();
            if (n > 0)
              listBox.selectRow(n - 1);
          }
        });
  };

  addAndMakeVisible(removeButton);
  removeButton.setButtonText("Remove");
  removeButton.onClick = [this] {
    int row = listBox.getSelectedRow();
    if (row >= 0 && manager) {
      rebuildRowKinds();
      if (row >= 0 && row < static_cast<int>(rowToSource.size())) {
        const auto &p = rowToSource[(size_t)row];
        if (p.first == RowKind::Layout) {
          manager->removeLayout(p.second);
        } else {
          manager->removeTouchpadMapping(p.second);
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
  area.removeFromBottom(4);
  listBox.setBounds(area);
}

int TouchpadMixerListPanel::getNumRows() {
  if (!manager)
    return 0;
  rebuildRowKinds();
  return static_cast<int>(rowToSource.size());
}

void TouchpadMixerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                              int width, int height,
                                              bool rowIsSelected) {
  if (!manager)
    return;
  rebuildRowKinds();
  if (rowNumber < 0 || rowNumber >= static_cast<int>(rowToSource.size()))
    return;
  if (rowIsSelected) {
    g.fillAll(juce::Colour(0xff3d5a80));
    g.setColour(juce::Colours::lightblue.withAlpha(0.5f));
    g.fillRect(0, 0, 3, height);
  } else {
    g.fillAll(juce::Colour(0xff2a2a2a));
  }
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();

  const auto &p = rowToSource[(size_t)rowNumber];
  juce::String label;
  if (p.first == RowKind::Layout) {
    if (p.second >= 0 && p.second < static_cast<int>(layouts.size()))
      label = juce::String(layouts[(size_t)p.second].name);
  } else {
    if (p.second >= 0 && p.second < static_cast<int>(mappings.size())) {
      const auto &m = mappings[(size_t)p.second];
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

void TouchpadMixerListPanel::setSelectedRowIndex(int row) {
  int num = getNumRows();
  if (num <= 0)
    return;
  if (row < 0 || row >= num)
    row = 0;
  listBox.selectRow(row);
}

void TouchpadMixerListPanel::setFilterGroupId(int filterGroupId) {
  if (filterGroupId_ == filterGroupId)
    return;
  filterGroupId_ = filterGroupId;
  rebuildRowKinds();
  listBox.updateContent();
  listBox.repaint();
  // Clear selection when filter changes (selection may no longer be valid)
  listBox.deselectAllRows();
  if (rowToSource.size() > 0)
    listBox.selectRow(0);
}

void TouchpadMixerListPanel::selectedRowsChanged(int lastRowSelected) {
  if (!onSelectionChanged || !manager) {
    return;
  }

  rebuildRowKinds();

  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();

  if (lastRowSelected < 0 ||
      lastRowSelected >= static_cast<int>(rowToSource.size())) {
    onSelectionChanged(RowKind::Layout, -1, nullptr, nullptr, -1);
    return;
  }

  const auto &p = rowToSource[(size_t)lastRowSelected];
  RowKind kind = p.first;
  int actualIndex = p.second;
  if (kind == RowKind::Layout) {
    if (actualIndex >= 0 &&
        actualIndex < static_cast<int>(layouts.size())) {
      onSelectionChanged(kind, actualIndex,
                         &layouts[(size_t)actualIndex], nullptr, lastRowSelected);
    } else {
      onSelectionChanged(kind, -1, nullptr, nullptr, -1);
    }
  } else {
    if (actualIndex >= 0 &&
        actualIndex < static_cast<int>(mappings.size())) {
      onSelectionChanged(kind, actualIndex, nullptr,
                         &mappings[(size_t)actualIndex], lastRowSelected);
    } else {
      onSelectionChanged(kind, -1, nullptr, nullptr, -1);
    }
  }
}

void TouchpadMixerListPanel::changeListenerCallback(juce::ChangeBroadcaster *) {
  if (isInitialLoad) {
    // First load: update synchronously for reliable selection restoration
    rebuildRowKinds();
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

void TouchpadMixerListPanel::handleAsyncUpdate() {
  rebuildRowKinds();
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

void TouchpadMixerListPanel::rebuildRowKinds() {
  rowKinds.clear();
  rowToSource.clear();
  if (!manager)
    return;
  auto layouts = manager->getLayouts();
  auto mappings = manager->getTouchpadMappings();

  auto matchesFilter = [this](int layoutGroupId) {
    if (filterGroupId_ == -1)
      return true;
    if (filterGroupId_ == 0)
      return layoutGroupId == 0;
    return layoutGroupId == filterGroupId_;
  };

  for (size_t i = 0; i < layouts.size(); ++i) {
    if (matchesFilter(layouts[i].layoutGroupId)) {
      rowKinds.push_back(RowKind::Layout);
      rowToSource.emplace_back(RowKind::Layout, static_cast<int>(i));
    }
  }
  for (size_t i = 0; i < mappings.size(); ++i) {
    if (matchesFilter(mappings[i].layoutGroupId)) {
      rowKinds.push_back(RowKind::Mapping);
      rowToSource.emplace_back(RowKind::Mapping, static_cast<int>(i));
    }
  }
}

TouchpadMixerListPanel::RowKind
TouchpadMixerListPanel::getRowKind(int rowIndex) const {
  if (rowIndex < 0 || rowIndex >= static_cast<int>(rowToSource.size()))
    return RowKind::Layout;
  return rowToSource[(size_t)rowIndex].first;
}

int TouchpadMixerListPanel::getSelectedLayoutIndex() {
  rebuildRowKinds();
  int row = listBox.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(rowToSource.size()))
    return -1;
  const auto &p = rowToSource[(size_t)row];
  if (p.first != RowKind::Layout)
    return -1;
  return p.second;
}

int TouchpadMixerListPanel::getSelectedMappingIndex() {
  rebuildRowKinds();
  int row = listBox.getSelectedRow();
  if (row < 0 || row >= static_cast<int>(rowToSource.size()))
    return -1;
  const auto &p = rowToSource[(size_t)row];
  if (p.first != RowKind::Mapping)
    return -1;
  return p.second;
}
