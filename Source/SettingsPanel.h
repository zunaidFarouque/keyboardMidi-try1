#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "RawInputManager.h"
#include "SettingsManager.h"
#include "SettingsDefinition.h"
#include <JuceHeader.h>
#include <functional>
#include <array>
#include <vector>

class SettingsPanel : public juce::Component,
                      public juce::ChangeListener,
                      public RawInputManager::Listener {
public:
  explicit SettingsPanel(SettingsManager &settingsMgr, MidiEngine &midiEng,
                         RawInputManager &rawInputMgr);
  ~SettingsPanel() override;

  // Phase 42: Two-stage init – call after object graph is built
  void initialize();

  std::function<void()> onResetMiniWindowPosition;
  std::function<void()> onResetUiLayout;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void parentSizeChanged() override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  // Height needed to show all settings rows (used by MainComponent/Viewport).
  int getRequiredHeight() const;

private:
  struct UiItem {
    std::unique_ptr<juce::Component> component;
    float weight = 1.0f;
    bool isAutoWidth = false;
  };

  struct UiRow {
    std::vector<UiItem> items;
    bool isSeparatorRow = false;
  };

  // Simple separator renderer (matches MappingInspector / ZonePropertiesPanel style)
  class SeparatorComponent : public juce::Component {
  public:
    SeparatorComponent(const juce::String &label,
                       juce::Justification justification);
    void paint(juce::Graphics &g) override;

  private:
    juce::String labelText;
    juce::Justification textAlign;
  };

  SettingsManager &settingsManager;
  MidiEngine &midiEngine;
  RawInputManager &rawInputManager;

  // Dynamic row model
  std::vector<UiRow> uiRows;

  // Non-owning pointers to key controls we need to keep in sync with SettingsManager
  juce::Slider *pbRangeSlider = nullptr;
  juce::Slider *visXOpacitySlider = nullptr;
  juce::Slider *visYOpacitySlider = nullptr;
  juce::ToggleButton *showTouchpadInMiniWindowToggle = nullptr;
  juce::ToggleButton *hideCursorInPerformanceModeToggle = nullptr;
  juce::ToggleButton *studioModeToggle = nullptr;       // Studio Mode (Multi-Device Support)
  juce::ToggleButton *capRefresh30FpsToggle = nullptr;  // Cap window refresh at 30 FPS
  juce::ToggleButton *delayMidiCheckbox = nullptr;
  juce::Slider *delayMidiSlider = nullptr;
  juce::ToggleButton *rememberUiStateToggle = nullptr;

  // Key-learning buttons (non-owning; used by helper methods and callbacks)
  juce::TextButton *toggleKeyButton = nullptr;                 // Set toggle key
  juce::TextButton *resetToggleKeyButton = nullptr;            // Reset toggle key to F12
  bool isLearningToggleKey = false;

  juce::TextButton *performanceModeKeyButton = nullptr;        // Set performance mode key
  juce::TextButton *resetPerformanceModeKeyButton = nullptr;   // Reset to F11
  bool isLearningPerformanceModeKey = false;

  // Mapping Colors buttons for Note / Expression / Command
  std::array<juce::TextButton *, 3> typeColorButtons{{nullptr, nullptr, nullptr}};

  void updateToggleKeyButtonText();
  void updatePerformanceModeKeyButtonText();
  void refreshTypeColorButtons();
  void launchColourSelectorForType(ActionType type, juce::TextButton *button);

  // Schema-driven helpers
  void rebuildUI();
  void createControl(const InspectorControl &def, UiRow &currentRow);
  juce::var getSettingsValue(const juce::String &propertyId) const;
  void applySettingsValue(const juce::String &propertyId, const juce::var &value);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPanel)
};
