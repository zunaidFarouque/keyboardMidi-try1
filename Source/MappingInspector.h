#pragma once
#include <JuceHeader.h>
#include <vector>

class DeviceManager;

class MappingInspector : public juce::Component, 
                         public juce::ValueTree::Listener,
                         public juce::ChangeListener {
public:
  MappingInspector(juce::UndoManager *undoMgr, DeviceManager *deviceMgr);
  ~MappingInspector() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // Main entry point: update UI based on selection
  void setSelection(const std::vector<juce::ValueTree> &selection);

  // Get required height for self-sizing
  int getRequiredHeight() const;

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

  // ChangeListener implementation (for DeviceManager changes)
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  juce::UndoManager *undoManager;
  DeviceManager *deviceManager;
  std::vector<juce::ValueTree> selectedTrees;

  // UI Controls
  juce::ComboBox typeSelector;
  juce::ComboBox aliasSelector;
  juce::Slider channelSlider;
  juce::Slider data1Slider;
  juce::Slider data2Slider;
  juce::Slider randVelSlider;
  juce::Label channelLabel;
  juce::Label data1Label;
  juce::Label data2Label;
  juce::Label randVelLabel;
  juce::Label typeLabel;
  juce::Label aliasLabel;
  juce::Label commandLabel;
  juce::ComboBox commandSelector;

  // Helper methods
  void updateControlsFromSelection();
  void enableControls(bool enabled);
  bool allTreesHaveSameValue(const juce::Identifier &property);
  juce::var getCommonValue(const juce::Identifier &property);
  void refreshAliasSelector();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingInspector)
};
