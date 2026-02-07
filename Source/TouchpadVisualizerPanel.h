#pragma once
#include "InputProcessor.h"
#include "SettingsManager.h"
#include "TouchpadTypes.h"
#include <JuceHeader.h>
#include <atomic>
#include <vector>

/// Shared touchpad visualizer component. Used by both the main VisualizerComponent
/// and the MiniStatusWindow (when "show touchpad visualizer in mini window" is on).
/// Changing drawing logic here affects both places.
class TouchpadVisualizerPanel : public juce::Component, public juce::Timer {
public:
  TouchpadVisualizerPanel(InputProcessor *inputProc, SettingsManager *settingsMgr);
  ~TouchpadVisualizerPanel() override;

  void setContacts(const std::vector<TouchpadContact> &contacts,
                   uintptr_t deviceHandle);
  void setVisualizedLayer(int layerId);
  void setSelectedStrip(int stripIndex, int layerId);

  /// When false, hide "Touchpad: Pt1: X=... Y=..." text (used in mini window)
  void setShowContactCoordinates(bool show);

  void paint(juce::Graphics &g) override;
  void visibilityChanged() override;

  void timerCallback() override;

private:
  InputProcessor *inputProcessor;
  SettingsManager *settingsManager;
  int currentVisualizedLayer = 0;
  int selectedStripIndex_ = -1;
  int selectedStripLayerId_ = 0;
  bool showContactCoordinates_ = true;

  std::vector<TouchpadContact> contacts_;
  std::atomic<uintptr_t> lastDeviceHandle_{0};
  mutable juce::CriticalSection contactsLock_;

  static constexpr float kTouchpadAspectW = 3.0f;
  static constexpr float kTouchpadAspectH = 2.0f;
  static constexpr int kRefreshIntervalMs = 34;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadVisualizerPanel)
};
