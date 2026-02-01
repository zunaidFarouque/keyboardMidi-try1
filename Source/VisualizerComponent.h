#pragma once
// Phase 52.2: Renders from InputProcessor::getContext() VisualGrid only (no
// findZoneForKey / isKeyInAnyZone / simulateInput). juce::Timer drives refresh.
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "ZoneManager.h"
#include <JuceHeader.h>
#include <atomic>
#include <set>

class InputProcessor;
class PresetManager;
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
                      InputProcessor *inputProc = nullptr);
  ~VisualizerComponent() override;

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

  // Phase 45.3: choose which layer the visualizer simulates for editing view
  void setVisualizedLayer(int layerId);

  /// Returns black or white for legible text on the given key fill color.
  static juce::Colour getTextColorForKeyFill(juce::Colour keyFillColor);

  void paint(juce::Graphics &) override;
  void resized() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

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
  int currentVisualizedLayer = 0; // Phase 45.3: which layer we are inspecting
  std::set<int> activeKeys;
  mutable juce::CriticalSection keyStateLock;

  // Phase 50.9: Async Dynamic View (mailbox-style atomics)
  std::atomic<uintptr_t> lastInputDeviceHandle{0}; // Written by Input Thread
  std::atomic<bool> followInputEnabled{false};     // UI State

  // Phase 50.9.1: Follow toggle UI (kept simple for clarity)
  juce::TextButton followButton{"Follow Input"};
  void updateFollowButtonAppearance();

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
