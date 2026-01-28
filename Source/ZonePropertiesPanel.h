#pragma once
#include "DeviceManager.h"
#include "KeyChipList.h"
#include "MidiNoteUtilities.h"
#include "RawInputManager.h"
#include "ScaleUtilities.h"
#include "Zone.h"
#include <JuceHeader.h>
#include <memory>

class ZoneManager;
class ScaleLibrary;
class ScaleEditorComponent;

class ZonePropertiesPanel : public juce::Component,
                            public RawInputManager::Listener,
                            public juce::ChangeListener {
public:
  ZonePropertiesPanel(ZoneManager *zoneMgr, DeviceManager *deviceMgr,
                      RawInputManager *rawInputMgr, ScaleLibrary *scaleLib);
  ~ZonePropertiesPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // Set the zone to edit
  void setZone(std::shared_ptr<Zone> zone);

  // Get required height for self-sizing
  int getRequiredHeight() const;

  // Callback when resize is requested (e.g., chip list changes)
  std::function<void()> onResizeRequested;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;
  ScaleLibrary *scaleLibrary;
  std::shared_ptr<Zone> currentZone;

  // UI Controls
  juce::Label aliasLabel;
  juce::ComboBox aliasSelector;
  // Phase 49: Zone layer assignment
  juce::Label layerLabel;
  juce::ComboBox layerSelector;
  juce::Label nameLabel;
  juce::TextEditor nameEditor;
  juce::Label scaleLabel;
  juce::ComboBox scaleSelector;
  juce::ToggleButton globalScaleToggle;
  juce::TextButton editScaleButton;
  juce::Label rootLabel;
  juce::Slider rootSlider;
  juce::ToggleButton globalRootToggle;
  juce::Label chromaticOffsetLabel;
  juce::Slider chromaticOffsetSlider;
  juce::Label degreeOffsetLabel;
  juce::Slider degreeOffsetSlider;
  juce::ToggleButton transposeLockButton;
  juce::ToggleButton captureKeysButton;
  juce::ToggleButton removeKeysButton;
  juce::Label strategyLabel;
  juce::ComboBox strategySelector;
  juce::Label gridIntervalLabel;
  juce::Slider gridIntervalSlider;
  juce::Label pianoHelpLabel;
  juce::Label channelLabel;
  juce::Slider channelSlider;
  juce::Label colorLabel;
  juce::TextButton colorButton;
  juce::Label chordTypeLabel;
  juce::ComboBox chordTypeSelector;
  juce::Label voicingLabel;
  juce::ComboBox voicingSelector;
  juce::Label playModeLabel;
  juce::ComboBox playModeSelector;
  juce::Label strumSpeedLabel;
  juce::Slider strumSpeedSlider;
  juce::Label releaseBehaviorLabel;
  juce::ComboBox releaseBehaviorSelector;
  juce::Label releaseDurationLabel;
  juce::Slider releaseDurationSlider;
  juce::ToggleButton allowSustainToggle;
  juce::Label baseVelLabel;
  juce::Slider baseVelSlider;
  juce::Label randVelLabel;
  juce::Slider randVelSlider;
  juce::ToggleButton strictGhostToggle;
  juce::Label ghostVelLabel;
  juce::Slider ghostVelSlider;
  juce::ToggleButton bassToggle;
  juce::Label bassOctaveLabel;
  juce::Slider bassOctaveSlider;
  juce::Label displayModeLabel;
  juce::ComboBox displayModeSelector;
  juce::Label polyphonyModeLabel;
  juce::ComboBox polyphonyModeSelector;
  juce::Label glideTimeLabel;
  juce::Slider glideTimeSlider;
  juce::ToggleButton adaptiveGlideToggle;
  juce::Label maxGlideTimeLabel;
  juce::Slider maxGlideTimeSlider;
  juce::Label monoWarningLabel; // Warning about channel separation (Phase 26.2)
  KeyChipList chipList;

  void refreshAliasSelector();
  void refreshScaleSelector();
  void updateControlsFromZone();
  void updateKeysAssignedLabel();
  void updateVisibility();
  void rebuildZoneCache();

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZonePropertiesPanel)
};
