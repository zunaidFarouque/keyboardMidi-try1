#pragma once
#include <JuceHeader.h>
#include <vector>

class MappingInspector : public juce::Component, public juce::ValueTree::Listener {
public:
  MappingInspector(juce::UndoManager *undoMgr);
  ~MappingInspector() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // Main entry point: update UI based on selection
  void setSelection(const std::vector<juce::ValueTree> &selection);

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

private:
  juce::UndoManager *undoManager;
  std::vector<juce::ValueTree> selectedTrees;

  // UI Controls
  juce::ComboBox typeSelector;
  juce::Slider channelSlider;
  juce::Slider data1Slider;
  juce::Slider data2Slider;
  juce::Label channelLabel;
  juce::Label data1Label;
  juce::Label data2Label;
  juce::Label typeLabel;

  // Helper methods
  void updateControlsFromSelection();
  void enableControls(bool enabled);
  bool allTreesHaveSameValue(const juce::Identifier &property);
  juce::var getCommonValue(const juce::Identifier &property);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingInspector)
};
