#pragma once
#include "DeviceManager.h"
#include "PresetManager.h"
#include "ZoneManager.h"
#include <JuceHeader.h>

class SettingsManager;
class TouchpadMixerManager;

class StartupManager : public juce::Timer,
                       public juce::ValueTree::Listener,
                       public juce::ChangeListener {
public:
  StartupManager(PresetManager *presetMgr, DeviceManager *deviceMgr,
                 ZoneManager *zoneMgr, TouchpadMixerManager *touchpadMixerMgr,
                 SettingsManager *settingsMgr);
  ~StartupManager() override;

  // Initialize application (load or create factory default)
  void initApp();

  // Create factory default configuration
  void createFactoryDefault();

  // Trigger a debounced save (starts 2-second timer)
  void triggerSave();

  // Save immediately (flush pending saves)
  void saveImmediate();

  // Timer callback (auto-save)
  void timerCallback() override;

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree &treeWhosePropertyHasChanged,
                                const juce::Identifier &property) override;
  void valueTreeChildAdded(juce::ValueTree &parentTree,
                           juce::ValueTree &childWhichHasBeenAdded) override;
  void valueTreeChildRemoved(juce::ValueTree &parentTree,
                             juce::ValueTree &childWhichHasBeenRemoved,
                             int indexFromWhichChildWasRemoved) override;
  void
  valueTreeChildOrderChanged(juce::ValueTree &parentTreeWhoseChildrenHaveMoved,
                             int oldIndex, int newIndex) override;

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  PresetManager *presetManager;
  DeviceManager *deviceManager;
  ZoneManager *zoneManager;
  TouchpadMixerManager *touchpadMixerManager;
  SettingsManager *settingsManager;

  juce::File appDataFolder;
  juce::File autoloadFile;
  juce::File settingsFile;

  void performSave();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StartupManager)
};
