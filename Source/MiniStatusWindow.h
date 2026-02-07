#pragma once
#include "SettingsManager.h"
#include "TouchpadTypes.h"
#include <JuceHeader.h>
#include <vector>

class InputProcessor;

class MiniStatusWindow : public juce::DocumentWindow,
                        public juce::ChangeListener {
public:
  MiniStatusWindow(SettingsManager &settingsMgr, InputProcessor *inputProc);
  ~MiniStatusWindow() override;

  void closeButtonPressed() override;
  void moved() override;

  void resetToDefaultPosition();

  void updateTouchpadContacts(const std::vector<TouchpadContact> &contacts,
                              uintptr_t deviceHandle);
  void setVisualizedLayer(int layerId);
  void setSelectedTouchpadStrip(int stripIndex, int layerId);

private:
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;
  void refreshContent();

  SettingsManager &settingsManager;
  InputProcessor *inputProcessor;
  juce::Label statusLabel;
  std::unique_ptr<juce::Component> touchpadPanelHolder;
  bool showingTouchpadPanel = false; // avoid clear/re-set when content type unchanged

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniStatusWindow)
};
