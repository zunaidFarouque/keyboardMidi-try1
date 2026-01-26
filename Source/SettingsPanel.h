#pragma once
#include "MidiEngine.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include <JuceHeader.h>

class SettingsPanel : public juce::Component, public RawInputManager::Listener {
public:
  explicit SettingsPanel(SettingsManager& settingsMgr, MidiEngine& midiEng, RawInputManager& rawInputMgr);
  ~SettingsPanel() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

  void paint(juce::Graphics& g) override;
  void resized() override;

private:
  SettingsManager& settingsManager;
  MidiEngine& midiEngine;
  RawInputManager& rawInputManager;
  juce::Slider pbRangeSlider;
  juce::Label pbRangeLabel;
  juce::TextButton sendRpnButton; // Button to send RPN to all channels (Phase 25.2)
  juce::TextButton toggleKeyButton; // Button to set toggle key
  bool isLearningToggleKey = false;
  
  void updateToggleKeyButtonText();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
