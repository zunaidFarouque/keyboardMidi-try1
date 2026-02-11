#include "TouchpadMixerListPanel.h"

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

  addAndMakeVisible(addButton);
  addButton.setButtonText("Add Entry");
  addButton.onClick = [this] {
    if (!manager)
      return;

    juce::PopupMenu menu;
    int nextId = 1;
    const int emptyId = nextId++;
    menu.addItem(emptyId, "Empty layout");

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
        [this, emptyId, firstPresetId](int result) {
          if (result <= 0 || !manager)
            return;

          if (result == emptyId) {
            TouchpadMixerConfig def;
            def.name = "Touchpad Mixer";
            manager->addLayout(def);
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
            int n = static_cast<int>(manager->getLayouts().size());
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
      manager->removeLayout(row);
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
  return static_cast<int>(manager->getLayouts().size());
}

void TouchpadMixerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                              int width, int height,
                                              bool rowIsSelected) {
  if (!manager)
    return;
  auto layouts = manager->getLayouts();
  if (rowNumber < 0 || rowNumber >= static_cast<int>(layouts.size()))
    return;
  if (rowIsSelected)
    g.fillAll(juce::Colour(0xff4a4a4a));
  else
    g.fillAll(juce::Colour(0xff2a2a2a));
  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText(juce::String(layouts[(size_t)rowNumber].name), 8, 0, width - 16,
             height, juce::Justification::centredLeft);
}

int TouchpadMixerListPanel::getSelectedLayoutIndex() const {
  return listBox.getSelectedRow();
}

void TouchpadMixerListPanel::selectedRowsChanged(int lastRowSelected) {
  if (onSelectionChanged) {
    if (lastRowSelected >= 0 && manager) {
      auto layouts = manager->getLayouts();
      if (lastRowSelected < static_cast<int>(layouts.size()))
        onSelectionChanged(lastRowSelected, &layouts[(size_t)lastRowSelected]);
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
