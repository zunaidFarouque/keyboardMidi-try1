#pragma once
#include "ZoneManager.h"
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "KeyboardLayoutUtils.h"
#include "MidiNoteUtilities.h"
#include "MappingTypes.h"
#include <JuceHeader.h>
#include <set>

class InputProcessor;

class VisualizerComponent : public juce::Component,
                            public RawInputManager::Listener,
                            public juce::ChangeListener {
public:
  VisualizerComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, InputProcessor *inputProc = nullptr);
  ~VisualizerComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  InputProcessor *inputProcessor;
  std::set<int> activeKeys;

  // Helper to check if a key belongs to any zone
  bool isKeyInAnyZone(int keyCode, uintptr_t aliasHash);
  
  // Helper to find which zone contains a key (returns root note if it's the root)
  std::pair<std::shared_ptr<Zone>, bool> findZoneForKey(int keyCode, uintptr_t aliasHash);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualizerComponent)
};
