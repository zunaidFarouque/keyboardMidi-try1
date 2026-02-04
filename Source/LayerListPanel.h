#pragma once
#include "PresetManager.h"
#include <JuceHeader.h>
#include <functional>

class LayerListPanel : public juce::Component,
                       public juce::ListBoxModel,
                       public juce::ValueTree::Listener {
public:
  LayerListPanel(PresetManager &pm);
  ~LayerListPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // ListBoxModel implementation
  int getNumRows() override;
  void paintListBoxItem(int rowNumber, juce::Graphics &g, int width, int height,
                        bool rowIsSelected) override;
  void listBoxItemDoubleClicked(int row, const juce::MouseEvent &) override;
  void selectedRowsChanged(int lastRowSelected) override;

  // ValueTree::Listener implementation
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &childWhichHasBeenAdded) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &childWhichHasBeenRemoved,
                             int) override;
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

  // Callback for layer selection
  std::function<void(int layerId)> onLayerSelected;

  // Set selected layer programmatically
  void setSelectedLayer(int layerId);

private:
  PresetManager &presetManager;
  juce::ListBox listBox;
  int selectedLayerId = 0;

  // Layer inheritance toggles (for selected layer)
  juce::Label inheritanceLabel;
  juce::ToggleButton soloLayerToggle;
  juce::ToggleButton passthruToggle;
  juce::ToggleButton privateToggle;

  void refreshInheritanceTogglesFromLayer();

  // Helper: Get layer node at row index
  juce::ValueTree getLayerAtRow(int row);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LayerListPanel)
};
