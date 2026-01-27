#include "LayerListPanel.h"

LayerListPanel::LayerListPanel(PresetManager &pm) : presetManager(pm) {
  listBox.setModel(this);
  listBox.setRowHeight(24);
  addAndMakeVisible(listBox);

  // Listen to Layers list changes
  presetManager.getLayersList().addListener(this);

  // Select Layer 0 by default
  setSelectedLayer(0);
}

void LayerListPanel::selectedRowsChanged(int lastRowSelected) {
  auto layer = getLayerAtRow(lastRowSelected);
  if (layer.isValid()) {
    int layerId = layer.getProperty("id", -1);
    selectedLayerId = layerId;
    if (onLayerSelected)
      onLayerSelected(layerId);
  }
}

LayerListPanel::~LayerListPanel() {
  presetManager.getLayersList().removeListener(this);
}

void LayerListPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void LayerListPanel::resized() { listBox.setBounds(getLocalBounds()); }

int LayerListPanel::getNumRows() {
  auto layersList = presetManager.getLayersList();
  return layersList.getNumChildren();
}

void LayerListPanel::paintListBoxItem(int rowNumber, juce::Graphics &g,
                                      int width, int height,
                                      bool rowIsSelected) {
  auto layer = getLayerAtRow(rowNumber);
  if (!layer.isValid())
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

  int layerId = layer.getProperty("id", -1);
  juce::String name = layer.getProperty("name", "Unknown").toString();

  juce::String displayText = juce::String(layerId) + ": " + name;
  g.drawText(displayText, 8, 0, width - 8, height,
             juce::Justification::centredLeft, true);
}

void LayerListPanel::listBoxItemDoubleClicked(int row,
                                              const juce::MouseEvent &) {
  auto layer = getLayerAtRow(row);
  if (!layer.isValid())
    return;

  juce::String currentName = layer.getProperty("name", "").toString();

  // Phase 41: Simple rename - show message for now
  // Full inline editing with AlertWindow text input can be enhanced later
  juce::AlertWindow::showMessageBoxAsync(
      juce::AlertWindow::InfoIcon, "Rename Layer",
      "Double-click rename functionality.\n"
      "Current layer: " +
          currentName +
          "\n\n"
          "To rename, edit the layer name property in the inspector or preset "
          "XML.");
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
  if (tree.hasType("Layer") && property == juce::Identifier("name")) {
    listBox.repaint();
  }
}

void LayerListPanel::setSelectedLayer(int layerId) {
  selectedLayerId = layerId;
  auto layersList = presetManager.getLayersList();
  for (int i = 0; i < layersList.getNumChildren(); ++i) {
    auto layer = layersList.getChild(i);
    if (layer.isValid()) {
      int id = static_cast<int>(layer.getProperty("id", -1));
      if (id == layerId) {
        listBox.selectRow(i);
        if (onLayerSelected)
          onLayerSelected(layerId);
        return;
      }
    }
  }
}

juce::ValueTree LayerListPanel::getLayerAtRow(int row) {
  auto layersList = presetManager.getLayersList();
  if (row >= 0 && row < layersList.getNumChildren()) {
    return layersList.getChild(row);
  }
  return juce::ValueTree();
}
