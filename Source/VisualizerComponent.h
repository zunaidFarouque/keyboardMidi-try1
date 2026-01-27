#pragma once
#include "ZoneManager.h"
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "KeyboardLayoutUtils.h"
#include "MidiNoteUtilities.h"
#include "MappingTypes.h"
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
                            public juce::ValueTree::Listener {
public:
  VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, const VoiceManager &voiceMgr, SettingsManager *settingsMgr, PresetManager *presetMgr = nullptr, InputProcessor *inputProc = nullptr);
  ~VisualizerComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // ValueTree::Listener implementation
  void valueTreeChildAdded(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenAdded) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree, juce::ValueTree &childWhichHasBeenRemoved, int indexFromWhichChildWasRemoved) override;
  void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged, const juce::Identifier &property) override;
  void valueTreeParentChanged(juce::ValueTree &treeWhoseParentHasChanged) override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  const VoiceManager &voiceManager;
  SettingsManager *settingsManager;
  PresetManager *presetManager;
  InputProcessor *inputProcessor;
  std::set<int> activeKeys;
  mutable juce::CriticalSection keyStateLock;
  std::unique_ptr<juce::VBlankAttachment> vBlankAttachment;
  
  // Dirty flag for rendering optimization
  std::atomic<bool> needsRepaint { true };
  
  // State cache for polling external states
  bool lastSustainState = false;

  // Graphics cache for rendering optimization
  juce::Image backgroundCache;
  bool cacheValid = false;
  void refreshCache();

  // View Context (Phase 39/39.2)
  juce::ComboBox viewSelector;
  uintptr_t currentViewHash = 0; // Default to Global (0)
  std::vector<uintptr_t> viewHashes; // Store full 64-bit hashes (Phase 39.2)
  void updateViewSelector();
  void onViewSelectorChanged();

  // Helper to check if a key belongs to any zone
  bool isKeyInAnyZone(int keyCode, uintptr_t aliasHash);
  
  // Helper to find which zone contains a key (returns root note if it's the root)
  std::pair<std::shared_ptr<Zone>, bool> findZoneForKey(int keyCode, uintptr_t aliasHash);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerComponent)
};
