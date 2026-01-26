#pragma once
#include "SettingsManager.h"
#include <JuceHeader.h>

class SettingsPanel : public juce::Component {
public:
  explicit SettingsPanel(SettingsManager& settingsMgr);
  ~SettingsPanel() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  SettingsManager& settingsManager;
  juce::Slider pbRangeSlider;
  juce::Label pbRangeLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
