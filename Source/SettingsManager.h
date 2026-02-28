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

  // Debug mode (controls crash logging and other debug-only behavior)
  bool getDebugModeEnabled() const;
  void setDebugModeEnabled(bool enabled);

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

  // Show touchpad visualizer in mini window (performance mode)
  bool getShowTouchpadVisualizerInMiniWindow() const;
  void setShowTouchpadVisualizerInMiniWindow(bool show);

  // Light visualizer: solid fills instead of gradients (lighter CPU for live performance)
  bool getVisualizerLightMode() const;
  void setVisualizerLightMode(bool light);

  // Show selected layer in visualizer (Mappings/Zones tab selection drives layer)
  bool getVisualizerShowSelectedLayer() const;
  void setVisualizerShowSelectedLayer(bool show);

  // Hide cursor when in performance mode (cursor still clipped)
  bool getHideCursorInPerformanceMode() const;
  void setHideCursorInPerformanceMode(bool hide);

  // Mini window position (JUCE window state string; empty = use default)
  juce::String getMiniWindowPosition() const;
  void setMiniWindowPosition(const juce::String &state);
  void resetMiniWindowPosition();

  // Mapping type colors (Phase 37)
  juce::Colour getTypeColor(ActionType type) const;
  void setTypeColor(ActionType type, juce::Colour colour);

  // UI layout / state persistence
  bool getRememberUiState() const;
  void setRememberUiState(bool remember);

  // Main window state (JUCE window state string; empty = use default)
  juce::String getMainWindowState() const;
  void setMainWindowState(const juce::String &state);

  // MainComponent layout
  int getMainTabIndex() const;
  void setMainTabIndex(int index);

  int getVerticalSplitPos() const;
  void setVerticalSplitPos(int pos);

  int getHorizontalSplitPos() const;
  void setHorizontalSplitPos(int pos);

  // Detachable containers (Visualizer / Editors / Log)
  bool getVisualizerVisible() const;
  void setVisualizerVisible(bool visible);
  bool getVisualizerPoppedOut() const;
  void setVisualizerPoppedOut(bool poppedOut);
  juce::String getVisualizerWindowState() const;
  void setVisualizerWindowState(const juce::String &state);

  bool getEditorVisible() const;
  void setEditorVisible(bool visible);
  bool getEditorPoppedOut() const;
  void setEditorPoppedOut(bool poppedOut);
  juce::String getEditorWindowState() const;
  void setEditorWindowState(const juce::String &state);

  bool getLogVisible() const;
  void setLogVisible(bool visible);
  bool getLogPoppedOut() const;
  void setLogPoppedOut(bool poppedOut);
  juce::String getLogWindowState() const;
  void setLogWindowState(const juce::String &state);

  // Tab-specific selections
  // Mappings tab
  int getMappingsSelectedLayerId() const;
  void setMappingsSelectedLayerId(int layerId);
  int getMappingsSelectedRow() const;
  void setMappingsSelectedRow(int row);

  // Zones tab (index into ZoneManager::getZones(); -1 = none)
  int getZonesSelectedIndex() const;
  void setZonesSelectedIndex(int index);

  // Touchpad tab (row index in TouchpadMixerListPanel; -1 = none)
  int getTouchpadSelectedRow() const;
  void setTouchpadSelectedRow(int row);

  // Reset all UI-related state to defaults (used by Reset UI Layout).
  void resetUiStateToDefaults();

  // Persistence
  void saveToXml(juce::File file);
  void loadFromXml(juce::File file);

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;

private:
  juce::ValueTree rootNode;
  double cachedStepsPerSemitone = 8192.0 / 12.0; // 8192 / pitchBendRange
  bool cachedMidiModeActive = false;

  juce::ValueTree getUiStateNode();
  juce::ValueTree getUiStateNode() const;

  // Ensure UIState properties are present and within safe ranges.
  void sanitizeUiStateNode();

  juce::String getTypePropertyName(ActionType type) const;
  void updateCachedStepsPerSemitone();
  void updateCachedMidiModeActive();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsManager)
};
