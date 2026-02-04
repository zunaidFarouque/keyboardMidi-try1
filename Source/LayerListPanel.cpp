#include "LayerListPanel.h"

LayerListPanel::LayerListPanel(PresetManager &pm) : presetManager(pm) {
  listBox.setModel(this);
  listBox.setRowHeight(24);
  listBox.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff1a1a1a));
  addAndMakeVisible(listBox);

  inheritanceLabel.setText("Layer inheritance", juce::dontSendNotification);
  inheritanceLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
  addAndMakeVisible(inheritanceLabel);

  soloLayerToggle.setButtonText("Solo layer");
  soloLayerToggle.onClick = [this] {
    auto layer = presetManager.getLayerNode(selectedLayerId);
    if (layer.isValid()) {
      layer.setProperty("soloLayer", soloLayerToggle.getToggleState(), nullptr);
      presetManager.sendChangeMessage();
    }
  };
  addAndMakeVisible(soloLayerToggle);

  passthruToggle.setButtonText("Pass through below");
  passthruToggle.onClick = [this] {
    auto layer = presetManager.getLayerNode(selectedLayerId);
    if (layer.isValid()) {
      layer.setProperty("passthruInheritance", passthruToggle.getToggleState(),
                        nullptr);
      presetManager.sendChangeMessage();
    }
  };
  addAndMakeVisible(passthruToggle);

  privateToggle.setButtonText("Private to this layer");
  privateToggle.onClick = [this] {
    auto layer = presetManager.getLayerNode(selectedLayerId);
    if (layer.isValid()) {
      layer.setProperty("privateToLayer", privateToggle.getToggleState(),
                        nullptr);
      presetManager.sendChangeMessage();
    }
  };
  addAndMakeVisible(privateToggle);

  // Listen to Layers list changes
  presetManager.getLayersList().addListener(this);

  // Select Layer 0 by default
  setSelectedLayer(0);
  refreshInheritanceTogglesFromLayer();
}

void LayerListPanel::selectedRowsChanged(int lastRowSelected) {
  // Phase 41: row index = layer id (0..8)
  if (lastRowSelected >= 0 && lastRowSelected <= 8) {
    selectedLayerId = lastRowSelected;
    refreshInheritanceTogglesFromLayer();
    if (onLayerSelected)
      onLayerSelected(lastRowSelected);
  }
}

LayerListPanel::~LayerListPanel() {
  presetManager.getLayersList().removeListener(this);
}

void LayerListPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff1a1a1a));
}

void LayerListPanel::resized() {
  auto r = getLocalBounds();
  const int panelHeight = 92;
  if (r.getHeight() > panelHeight) {
    auto inheritanceArea = r.removeFromBottom(panelHeight);
    listBox.setBounds(r);
    const int pad = 4;
    inheritanceArea = inheritanceArea.reduced(pad);
    inheritanceLabel.setBounds(
        inheritanceArea.removeFromTop(18).reduced(0, 2));
    soloLayerToggle.setBounds(inheritanceArea.removeFromTop(22).reduced(0, 2));
    passthruToggle.setBounds(inheritanceArea.removeFromTop(22).reduced(0, 2));
    privateToggle.setBounds(inheritanceArea.removeFromTop(22).reduced(0, 2));
  } else {
    listBox.setBounds(r);
  }
}

void LayerListPanel::refreshInheritanceTogglesFromLayer() {
  auto layer = presetManager.getLayerNode(selectedLayerId);
  if (!layer.isValid())
    return;
  soloLayerToggle.setToggleState(
      (bool)layer.getProperty("soloLayer", false), juce::dontSendNotification);
  passthruToggle.setToggleState(
      (bool)layer.getProperty("passthruInheritance", false),
      juce::dontSendNotification);
  privateToggle.setToggleState(
      (bool)layer.getProperty("privateToLayer", false),
      juce::dontSendNotification);
}

int LayerListPanel::getNumRows() {
  // Phase 41: Static 9 layers (0=Base, 1-8=Overlays)
  return 9;
}

void LayerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                      int width, int height,
                                      bool rowIsSelected) {
  auto layer = getLayerAtRow(rowNumber);
  if (!layer.isValid())
    return;

  const int pad = 2;
  auto area = juce::Rectangle<int>(0, 0, width, height).reduced(pad, 0);
  if (rowIsSelected) {
    g.setColour(juce::Colours::lightblue.withAlpha(0.2f));
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
  }

  g.setColour(rowIsSelected ? juce::Colours::white : juce::Colours::grey);
  g.setFont(15.0f);
  int layerId = layer.getProperty("id", rowNumber);
  juce::String name = layer.getProperty("name", layerId == 0 ? "Base" : "Layer " + juce::String(layerId)).toString();
  juce::String displayText = juce::String(layerId) + ": " + name;
  if (rowNumber == 0)
    displayText += " (Default)";
  g.drawText(displayText, 10, 0, width - 10, height,
             juce::Justification::centredLeft, true);
}

void LayerListPanel::listBoxItemDoubleClicked(int row,
                                              const juce::MouseEvent &) {
  auto layer = getLayerAtRow(row);
  if (!layer.isValid())
    return;

  juce::String currentName =
      layer.getProperty("name", row == 0 ? "Base" : "Layer " + juce::String(row))
          .toString();
  auto *dialog = new juce::AlertWindow("Rename Layer", "Enter new name:",
                                       juce::AlertWindow::NoIcon);
  dialog->addTextEditor("name", currentName, "Layer name:", false);
  dialog->addButton("OK", 1);
  dialog->addButton("Cancel", 0);
  dialog->enterModalState(
      true,
      juce::ModalCallbackFunction::create(
          [this, dialog, layer](int result) mutable {
            if (result == 1) {
              juce::String newName = dialog->getTextEditorContents("name").trim();
              if (!newName.isEmpty())
                layer.setProperty("name", newName, nullptr);
              listBox.repaint();
            }
            delete dialog;
          }));
}

void LayerListPanel::valueTreeChildAdded(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) {
  if (parentTree.hasType("Layers")) {
    listBox.updateContent();
  }
}

void LayerListPanel::valueTreeChildRemoved(
    juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved,
    int) {
  if (parentTree.hasType("Layers")) {
    listBox.updateContent();
  }
}

void LayerListPanel::valueTreePropertyChanged(
    juce::ValueTree &tree, const juce::Identifier &property) {
  if (tree.hasType("Layer")) {
    if (property == juce::Identifier("name")) {
      listBox.repaint();
    } else if (property == juce::Identifier("soloLayer") ||
               property == juce::Identifier("passthruInheritance") ||
               property == juce::Identifier("privateToLayer")) {
      int id = (int)tree.getProperty("id", -1);
      if (id == selectedLayerId)
        refreshInheritanceTogglesFromLayer();
    }
  }
}

void LayerListPanel::setSelectedLayer(int layerId) {
  if (layerId < 0 || layerId > 8)
    return;
  selectedLayerId = layerId;
  listBox.selectRow(layerId); // Phase 41: row index = layer id
  if (onLayerSelected)
    onLayerSelected(layerId);
}

juce::ValueTree LayerListPanel::getLayerAtRow(int row) {
  // Phase 41: Static 9 layers; row index = layer id
  if (row >= 0 && row <= 8)
    return presetManager.getLayerNode(row);
  return juce::ValueTree();
}
