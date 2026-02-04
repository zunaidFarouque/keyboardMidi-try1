#pragma once
#include "MappingTypes.h"
#include <JuceHeader.h>

class SettingsManager : public juce::ChangeBroadcaster,
                        public juce::ValueTree::Listener {
public:
  SettingsManager();
  ~SettingsManager() override;

  // Pitch Bend Range
  int getPitchBendRange() const;
  void setPitchBendRange(int range);
  // Cached 8192/range for PB value conversion (avoids division on hot path)
  double getStepsPerSemitone() const;

  // MIDI Mode Toggle
  bool isMidiModeActive() const;
  void setMidiModeActive(bool active);

  // Toggle Key
  int getToggleKey() const;
  void setToggleKey(int vkCode);

  // Performance Mode shortcut key
  int getPerformanceModeKey() const;
  void setPerformanceModeKey(int vkCode);

  // Last MIDI Device
  juce::String getLastMidiDevice() const;
  void setLastMidiDevice(const juce::String &name);

  // Studio Mode (Multi-Device Support)
  bool isStudioMode() const;
  void setStudioMode(bool active);

  // Window refresh rate: when true, cap at 30 FPS; when false, use higher rate
  bool isCapWindowRefresh30Fps() const;
  void setCapWindowRefresh30Fps(bool cap);
  int getWindowRefreshIntervalMs() const;

  // Delay MIDI messages (for binding in other software)
  bool isDelayMidiEnabled() const;
  void setDelayMidiEnabled(bool enabled);
  int getDelayMidiSeconds() const;
  void setDelayMidiSeconds(int seconds);

  // Visualizer: opacity for X and Y touchpad overlays (0.0–1.0)
  float getVisualizerXOpacity() const;
  void setVisualizerXOpacity(float alpha);
  float getVisualizerYOpacity() const;
  void setVisualizerYOpacity(float alpha);

  // Mapping type colors (Phase 37)
  juce::Colour getTypeColor(ActionType type) const;
  void setTypeColor(ActionType type, juce::Colour colour);

  // Persistence
  void saveToXml(juce::File file);
  void loadFromXml(juce::File file);

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

private:
  juce::ValueTree rootNode;
  double cachedStepsPerSemitone = 8192.0 / 12.0; // 8192 / pitchBendRange

  juce::String getTypePropertyName(ActionType type) const;
  void updateCachedStepsPerSemitone();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsManager)
};
