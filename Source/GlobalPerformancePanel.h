#pragma once
#include "ZoneManager.h"
#include <JuceHeader.h>

class GlobalPerformancePanel : public juce::Component,
                               public juce::ChangeListener {
public:
  GlobalPerformancePanel(ZoneManager *zoneMgr);
  ~GlobalPerformancePanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  ZoneManager *zoneManager;
  juce::TextButton degreeDownButton;
  juce::TextButton degreeUpButton;
  juce::TextButton chromaticDownButton;
  juce::TextButton chromaticUpButton;
  juce::Label statusLabel;

  void updateStatusLabel();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalPerformancePanel)
};
