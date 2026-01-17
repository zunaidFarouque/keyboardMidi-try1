#pragma once
#include "Zone.h"
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "MidiNoteUtilities.h"
#include "ScaleUtilities.h"
#include "KeyChipList.h"
#include <JuceHeader.h>
#include <memory>

class ScaleLibrary;
class ScaleEditorComponent;

class ZonePropertiesPanel : public juce::Component,
                           public RawInputManager::Listener,
                           public juce::ChangeListener {
public:
  ZonePropertiesPanel(DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib);
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
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) override;

private:
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;
  ScaleLibrary *scaleLibrary;
  std::shared_ptr<Zone> currentZone;

  // UI Controls
  juce::Label aliasLabel;
  juce::ComboBox aliasSelector;
  juce::Label nameLabel;
  juce::TextEditor nameEditor;
  juce::Label scaleLabel;
  juce::ComboBox scaleSelector;
  juce::TextButton editScaleButton;
  juce::Label rootLabel;
  juce::Slider rootSlider;
  juce::Label chromaticOffsetLabel;
  juce::Slider chromaticOffsetSlider;
  juce::Label degreeOffsetLabel;
  juce::Slider degreeOffsetSlider;
  juce::ToggleButton transposeLockButton;
  juce::ToggleButton captureKeysButton;
  juce::Label strategyLabel;
  juce::ComboBox strategySelector;
  juce::Label gridIntervalLabel;
  juce::Slider gridIntervalSlider;
  KeyChipList chipList;

  void refreshAliasSelector();
  void refreshScaleSelector();
  void updateControlsFromZone();
  void updateKeysAssignedLabel();

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZonePropertiesPanel)
};
