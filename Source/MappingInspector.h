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

  // ADSR controls (for Envelope type)
  juce::Slider attackSlider;
  juce::Slider decaySlider;
  juce::Slider sustainSlider;
  juce::Slider releaseSlider;
  juce::Label attackLabel;
  juce::Label decayLabel;
  juce::Label sustainLabel;
  juce::Label releaseLabel;
  juce::ComboBox envTargetSelector;
  juce::Label envTargetLabel;

  // Pitch Bend musical controls (for Envelope with PitchBend target)
  juce::Slider pbShiftSlider;
  juce::Label pbShiftLabel;
  juce::Label pbGlobalRangeLabel; // "Uses Global Range" label

  // Smart Scale Bend controls (for Envelope with SmartScaleBend target)
  juce::Slider smartStepSlider;
  juce::Label smartStepLabel;

  // Helper methods
  void updateControlsFromSelection();
  void enableControls(bool enabled);
  bool allTreesHaveSameValue(const juce::Identifier &property);
  juce::var getCommonValue(const juce::Identifier &property);
  void refreshAliasSelector();
  void updatePitchBendPeakValue(juce::ValueTree& tree); // Calculate and set data2 from pbRange/pbShift

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingInspector)
};
