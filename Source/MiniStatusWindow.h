#pragma once
#include "SettingsManager.h"
#include <JuceHeader.h>

class MiniStatusWindow : public juce::DocumentWindow {
public:
  explicit MiniStatusWindow(SettingsManager& settingsMgr);
  ~MiniStatusWindow() override;

  void closeButtonPressed() override;

private:
  SettingsManager& settingsManager;
  juce::Label statusLabel;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniStatusWindow)
};
