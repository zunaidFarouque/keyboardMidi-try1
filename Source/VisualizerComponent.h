#pragma once
// Phase 52.2: Renders from InputProcessor::getContext() VisualGrid only (no
// findZoneForKey / isKeyInAnyZone / simulateInput). juce::Timer drives refresh.
#include "DeviceManager.h"
#include "GlobalPerformancePanel.h"
#include "RawInputManager.h"
#include "TouchpadTypes.h"
#include "TouchpadVisualizerPanel.h"
#include "ZoneManager.h"
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <set>
#include <vector>

class InputProcessor;
class PresetManager;
class ScaleLibrary;
class SettingsManager;
class VoiceManager;

class VisualizerComponent : public juce::Component,
                            public RawInputManager::Listener,
                            public juce::ChangeListener,
                            public juce::ValueTree::Listener,
                            public juce::Timer {
public:
  VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr,
                      const VoiceManager &voiceMgr,
                      SettingsManager *settingsMgr,
                      PresetManager *presetMgr = nullptr,
                      InputProcessor *inputProc = nullptr,
                      ScaleLibrary *scaleLib = nullptr);
  ~VisualizerComponent() override;

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

  // Phase 45.3: choose which layer the visualizer simulates for editing view
  void setVisualizedLayer(int layerId);

  /// When layoutIndex >= 0, touchpad overlay is shown only for that layout
  /// and only when currentVisualizedLayer == layerId. Pass -1 to hide overlay.
  void setSelectedTouchpadLayout(int layoutIndex, int layerId);

  /// Returns black or white for legible text on the given key fill color.
  static juce::Colour getTextColorForKeyFill(juce::Colour keyFillColor);

  /// Restart timer with the given interval (ms). Used when cap-30-FPS setting
  /// changes.
  void restartTimerWithInterval(int intervalMs);

  /// When false, touchpad view always follows the active layer (for Mappings/Zones/Settings tabs).
  /// When true, touchpad tab selection controls the view.
  void setTouchpadTabActive(bool active);

  std::function<void(int layerId, int layoutIndex)> onTouchpadViewChanged;

  void paint(juce::Graphics &) override;
  void resized() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;
  void
  handleTouchpadContacts(uintptr_t deviceHandle,
                         const std::vector<TouchpadContact> &contacts) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // ValueTree::Listener implementation
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &childWhichHasBeenAdded) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &childWhichHasBeenRemoved,
                             int indexFromWhichChildWasRemoved) override;
  void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                const juce::Identifier &property) override;
  void
  valueTreeParentChanged(juce::ValueTree &treeWhoseParentHasChanged) override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  const VoiceManager &voiceManager;
  SettingsManager *settingsManager;
  PresetManager *presetManager;
  InputProcessor *inputProcessor;
  ScaleLibrary *scaleLibrary;
  GlobalPerformancePanel globalPanel;
  std::unique_ptr<juce::Component> globalPanelResizerBar;
  juce::TextButton expandPanelButton;
  float globalPanelWidth_ = 300.0f;
  bool globalPanelCollapsed_ = false;
  static constexpr float kGlobalPanelDefaultWidth = 300.0f;
  static constexpr float kGlobalPanelMinWidth = 24.0f;
  static constexpr float kExpandTabWidth = 22.0f;
  static constexpr int kResizerBarWidth = 6;
  float getEffectiveRightPanelWidth() const;
  void setGlobalPanelWidthFromResizer(float newWidth);
  void updateGlobalPanelLayout(int w, int h);

  int currentVisualizedLayer = 0; // Phase 45.3: which layer we are inspecting
  int selectedTouchpadLayoutIndex_ = -1;
  int selectedTouchpadLayoutLayerId_ = 0;
  std::unique_ptr<TouchpadVisualizerPanel> touchpadPanel_;
  std::set<int> activeKeys;
  mutable juce::CriticalSection keyStateLock;

  // Touchpad contact display (forwarded to touchpadPanel_; kept for backward
  // compat during refactor, can be removed if panel owns all)
  std::vector<TouchpadContact> lastTouchpadContacts;
  mutable juce::CriticalSection contactsLock;
  std::atomic<uintptr_t> lastTouchpadDeviceHandle{0};
  int64_t lastTouchpadPanelUpdateMs = 0; // throttle to cap-30-FPS setting

  // Phase 50.9: Async Dynamic View (mailbox-style atomics)
  std::atomic<uintptr_t> lastInputDeviceHandle{0}; // Written by Input Thread
  std::atomic<bool> followInputEnabled{false};     // UI State

  // Phase 50.9.1: Follow toggle UI (kept simple for clarity)
  juce::TextButton followButton{"Follow Input"};
  void updateFollowButtonAppearance();

  bool touchpadTabActive_ = false; // when false, touchpad view follows active layer

  // Dirty flag for rendering optimization
  std::atomic<bool> needsRepaint{true};

  // State cache for polling external states
  bool lastSustainState = false;

  // Graphics cache for rendering optimization
  juce::Image backgroundCache;
  bool cacheValid = false;
  void refreshCache();

  // View Context (Phase 39/39.2)
  juce::ComboBox viewSelector;
  uintptr_t currentViewHash = 0;     // Default to Global (0)
  std::vector<uintptr_t> viewHashes; // Store full 64-bit hashes (Phase 39.2)
  void updateViewSelector();
  void onViewSelectorChanged();

  // Phase 50.9: 30Hz UI polling
  void timerCallback() override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerComponent)
};
