#pragma once
#include "InputProcessor.h"
#include "MappingTypes.h"
#include "SettingsManager.h"
#include "TouchpadTypes.h"
#include <JuceHeader.h>
#include <atomic>
#include <unordered_map>
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
  void setSelectedLayout(int layoutIndex, int layerId);

  /// When false, hide "Touchpad: Pt1: X=... Y=..." text (used in mini window)
  void setShowContactCoordinates(bool show);

  /// Restart timer with interval from cap-30-FPS setting (30 or 60 fps)
  void restartTimerWithInterval(int intervalMs);

  void paint(juce::Graphics &g) override;
  void visibilityChanged() override;

  void timerCallback() override;

private:
  InputProcessor *inputProcessor;
  SettingsManager *settingsManager;
  int currentVisualizedLayer = 0;
  int selectedLayoutIndex_ = -1;
  int selectedLayoutLayerId_ = 0;
  bool showContactCoordinates_ = true;

  std::vector<TouchpadContact> contacts_;
  std::atomic<uintptr_t> lastDeviceHandle_{0};
  mutable juce::CriticalSection contactsLock_;

  // Track last update time for each contact to detect stale contacts
  std::unordered_map<int, int64_t> contactLastUpdateTime_;
  // Last time we had at least one tipDown contact (for timer efficiency)
  int64_t lastTimeHadContactsMs_ = 0;
  // Last painted state (to skip repaint when unchanged)
  int lastPaintedContactCount_ = -1;
  uint32_t lastPaintedStateHash_ = 0;

  static constexpr float kTouchpadAspectW = 3.0f;
  static constexpr float kTouchpadAspectH = 2.0f;
  static constexpr int kDefaultRefreshIntervalMs = 34;
  static constexpr int kContactTimeoutMs = 1000; // 1 second timeout for stale contacts

  // Cache PitchPadLayout by config hash for light live performance
  std::unordered_map<size_t, PitchPadLayout> pitchPadLayoutCache_;
  const void *lastContextForLayoutCache_ = nullptr;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadVisualizerPanel)
};
