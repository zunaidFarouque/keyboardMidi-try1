#pragma once
#include "MidiEngine.h"
#include "SettingsManager.h"
#include <JuceHeader.h>

class SettingsPanel : public juce::Component {
public:
  explicit SettingsPanel(SettingsManager& settingsMgr, MidiEngine& midiEng);
  ~SettingsPanel() override = default;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  SettingsManager& settingsManager;
  MidiEngine& midiEngine;
  juce::Slider pbRangeSlider;
  juce::Label pbRangeLabel;
  juce::TextButton sendRpnButton; // Button to send RPN to all channels (Phase 25.2)

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
