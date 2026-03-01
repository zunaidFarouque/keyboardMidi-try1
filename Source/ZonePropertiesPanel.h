#pragma once
#include "DeviceManager.h"
#include "KeyChipList.h"
#include "MidiNoteUtilities.h"
#include "RawInputManager.h"
#include "ScaleUtilities.h"
#include "Zone.h"
#include "ZoneDefinition.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

class PresetManager;
class ZoneManager;
class ScaleLibrary;
class ScaleEditorComponent;

class ZonePropertiesPanel : public juce::Component,
                            public RawInputManager::Listener,
                            public juce::ChangeListener {
public:
  ZonePropertiesPanel(ZoneManager *zoneMgr, DeviceManager *deviceMgr,
                      RawInputManager *rawInputMgr, ScaleLibrary *scaleLib,
                      PresetManager *presetMgr = nullptr);
  ~ZonePropertiesPanel() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void setZone(std::shared_ptr<Zone> zone);
  int getRequiredHeight() const;

  std::function<void()> onResizeRequested;
  /// Called at start of rebuildUI(); parent can save viewport scroll position.
  std::function<void()> onBeforeRebuild;
  /// Called at end of rebuildUI(); parent can restore viewport scroll position.
  std::function<void()> onAfterRebuild;

  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;
  void handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                       float value) override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;
  ScaleLibrary *scaleLibrary;
  PresetManager *presetManager = nullptr; // Optional: for keyboard group combo
  std::shared_ptr<Zone> currentZone;

  struct UiItem {
    std::unique_ptr<juce::Component> component;
    float weight = 1.0f;
    bool isAutoWidth = false;
  };
  struct UiRow {
    std::vector<UiItem> items;
    bool isSeparatorRow = false;
    bool isChipListRow = false;       // dynamic height
    bool isWrappableLabelRow = false; // multi-line hint (e.g. Sustain)
  };
  std::vector<UiRow> uiRows;
  juce::String lastSchemaSignature_;

  // Refs used by handleRawKeyEvent and layout (set in rebuildUI, cleared when
  // rebuilding)
  juce::ToggleButton *captureKeysButtonRef = nullptr;
  juce::ToggleButton *removeKeysButtonRef = nullptr;
  KeyChipList *chipListRef = nullptr;
  int chipListRowIndex = -1;

  class SeparatorComponent : public juce::Component {
  public:
    SeparatorComponent(const juce::String &label,
                       juce::Justification justification);
    void paint(juce::Graphics &g) override;

  private:
    juce::String labelText;
    juce::Justification textAlign;
  };

  void rebuildUI();
  void createControl(const ZoneControl &def, UiRow &currentRow);
  void createAliasRow();
  void createLayerRow();
  void createKeyboardGroupRow();
  void createNameRow();
  void createScaleRow();
  void createKeyAssignRow();
  void createChipListRow();
  void createColorRow();
  void rebuildZoneCache();

  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZonePropertiesPanel)
};
