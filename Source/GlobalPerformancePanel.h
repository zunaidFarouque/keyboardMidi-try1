#pragma once
#include "ZoneManager.h"
#include <JuceHeader.h>

class ScaleLibrary;

class GlobalPerformancePanel : public juce::Component,
                               public juce::ChangeListener,
                               public juce::Slider::Listener {
public:
  GlobalPerformancePanel(ZoneManager *zoneMgr, ScaleLibrary *scaleLib = nullptr);
  ~GlobalPerformancePanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // Slider::Listener (transpose sliders)
  void sliderValueChanged(juce::Slider *slider) override;

private:
  ZoneManager *zoneManager;
  ScaleLibrary *scaleLibrary;
  juce::ComboBox rootNoteCombo;
  juce::ComboBox scaleCombo;
  juce::Label rootLabel;
  juce::Label scaleLabel;
  juce::Label transposeLabel;
  juce::TextButton chromaticDownButton;
  juce::TextButton chromaticUpButton;
  juce::Slider chromaticSlider;
  juce::Label statusLabel;

  void updateStatusLabel();
  void refreshGlobalRootAndScaleFromManager();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlobalPerformancePanel)
};
