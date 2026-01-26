#include "SettingsPanel.h"

SettingsPanel::SettingsPanel(SettingsManager& settingsMgr)
    : settingsManager(settingsMgr) {
  // Setup PB Range Slider
  pbRangeLabel.setText("Global Pitch Bend Range (+/- semitones):", juce::dontSendNotification);
  pbRangeLabel.attachToComponent(&pbRangeSlider, true);
  addAndMakeVisible(pbRangeLabel);
  addAndMakeVisible(pbRangeSlider);
  
  pbRangeSlider.setRange(1, 96, 1);
  pbRangeSlider.setTextValueSuffix(" semitones");
  pbRangeSlider.setValue(settingsManager.getPitchBendRange(), juce::dontSendNotification);
  
  pbRangeSlider.onValueChange = [this] {
    int value = static_cast<int>(pbRangeSlider.getValue());
    settingsManager.setPitchBendRange(value);
  };
}

void SettingsPanel::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void SettingsPanel::resized() {
  auto area = getLocalBounds().reduced(10);
  int labelWidth = 200;
  int controlHeight = 25;
  int spacing = 10;
  int topPadding = 10;
  int leftMargin = area.getX() + labelWidth;
  int width = area.getWidth() - labelWidth;

  int y = topPadding;
  pbRangeSlider.setBounds(leftMargin, y, width, controlHeight);
}
