#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include <JuceHeader.h>
#include <array>

class SettingsPanel : public juce::Component,
                      public juce::ChangeListener,
                      public RawInputManager::Listener {
public:
  explicit SettingsPanel(SettingsManager &settingsMgr, MidiEngine &midiEng,
                         RawInputManager &rawInputMgr);
  ~SettingsPanel() override;

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  SettingsManager &settingsManager;
  MidiEngine &midiEngine;
  RawInputManager &rawInputManager;
  juce::Slider pbRangeSlider;
  juce::Label pbRangeLabel;
  // Visualizer opacity controls
  juce::GroupComponent visualizerGroup{"Visualizer", "Visualizer"};
  juce::Slider visXOpacitySlider;
  juce::Label visXOpacityLabel;
  juce::Slider visYOpacitySlider;
  juce::Label visYOpacityLabel;
  juce::TextButton
      sendRpnButton; // Button to send RPN to all channels (Phase 25.2)
  juce::Label toggleKeyLabel;
  juce::TextButton toggleKeyButton;      // Button to set toggle key
  juce::TextButton resetToggleKeyButton; // Button to reset toggle key to F12
  bool isLearningToggleKey = false;

  juce::Label performanceModeKeyLabel;
  juce::TextButton
      performanceModeKeyButton; // Button to set performance mode key
  juce::TextButton resetPerformanceModeKeyButton; // Button to reset to F11
  bool isLearningPerformanceModeKey = false;

  juce::ToggleButton studioModeToggle; // Studio Mode (Multi-Device Support)
  juce::ToggleButton capRefresh30FpsToggle; // Cap window refresh at 30 FPS

  juce::Label delayMidiLabel;
  juce::ToggleButton delayMidiCheckbox;
  juce::Slider delayMidiSlider;

  juce::GroupComponent mappingColorsGroup;
  std::array<juce::TextButton, 3> typeColorButtons;

  void updateToggleKeyButtonText();
  void updatePerformanceModeKeyButtonText();
  void refreshTypeColorButtons();
  void launchColourSelectorForType(ActionType type, juce::TextButton *button);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
