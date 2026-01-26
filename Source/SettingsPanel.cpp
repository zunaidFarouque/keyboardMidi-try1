#include "SettingsPanel.h"

SettingsPanel::SettingsPanel(SettingsManager& settingsMgr, MidiEngine& midiEng, RawInputManager& rawInputMgr)
    : settingsManager(settingsMgr), midiEngine(midiEng), rawInputManager(rawInputMgr) {
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

  // Setup Send RPN Button (Phase 25.2)
  addAndMakeVisible(sendRpnButton);
  sendRpnButton.setButtonText("Sync Range to Synth");
  sendRpnButton.onClick = [this] {
    int range = settingsManager.getPitchBendRange();
    DBG("Sending RPN Range " + juce::String(range) + " to all channels...");
    // Send RPN to all 16 MIDI channels
    for (int ch = 1; ch <= 16; ++ch) {
      midiEngine.sendPitchBendRangeRPN(ch, range);
    }
    // Show feedback (optional - could use a toast or label)
    sendRpnButton.setButtonText("Sent!");
    juce::MessageManager::callAsync([this] {
      sendRpnButton.setButtonText("Sync Range to Synth");
    });
  };
  
  // Setup Toggle Key Button
  addAndMakeVisible(toggleKeyButton);
  updateToggleKeyButtonText();
  toggleKeyButton.onClick = [this] {
    if (!isLearningToggleKey) {
      // Enter learn mode
      isLearningToggleKey = true;
      toggleKeyButton.setButtonText("Press any key...");
      rawInputManager.addListener(this);
    } else {
      // Cancel learn mode
      isLearningToggleKey = false;
      rawInputManager.removeListener(this);
      updateToggleKeyButtonText();
    }
  };
}

SettingsPanel::~SettingsPanel() {
  if (isLearningToggleKey) {
    rawInputManager.removeListener(this);
  }
}

void SettingsPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  if (!isLearningToggleKey || !isDown) {
    return;
  }
  
  // Learn the key
  settingsManager.setToggleKey(keyCode);
  isLearningToggleKey = false;
  rawInputManager.removeListener(this);
  updateToggleKeyButtonText();
}

void SettingsPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events during key learning
}

void SettingsPanel::updateToggleKeyButtonText() {
  int toggleKey = settingsManager.getToggleKey();
  juce::String keyName = RawInputManager::getKeyName(toggleKey);
  toggleKeyButton.setButtonText("Toggle Key: " + keyName);
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
  y += controlHeight + spacing;
  
  // Position Send RPN button below the slider
  sendRpnButton.setBounds(leftMargin, y, 200, controlHeight);
  y += controlHeight + spacing;
  
  // Toggle Key Button
  toggleKeyButton.setBounds(leftMargin, y, 200, controlHeight);
}
