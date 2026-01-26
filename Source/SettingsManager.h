#pragma once
#include <JuceHeader.h>

class SettingsManager : public juce::ChangeBroadcaster, public juce::ValueTree::Listener {
public:
  SettingsManager();
  ~SettingsManager() override;

  // Pitch Bend Range
  int getPitchBendRange() const;
  void setPitchBendRange(int range);

  // Persistence
  void saveToXml(juce::File file);
  void loadFromXml(juce::File file);

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree& tree,
                                const juce::Identifier& property) override;

private:
  juce::ValueTree rootNode;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsManager)
};
