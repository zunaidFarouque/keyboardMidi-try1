#pragma once
#include "MappingDefinition.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

class DeviceManager;
class PresetManager;
class SettingsManager;

class MappingInspector : public juce::Component,
                         public juce::ValueTree::Listener,
                         public juce::ChangeListener {
public:
  MappingInspector(juce::UndoManager &undoMgr, DeviceManager &deviceMgr,
                   SettingsManager &settingsMgr,
                   PresetManager *presetMgr = nullptr);
  ~MappingInspector() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void setSelection(const std::vector<juce::ValueTree> &selection);
  int getRequiredHeight() const;

  void valueTreePropertyChanged(juce::ValueTree &tree,
                                const juce::Identifier &property) override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  juce::UndoManager &undoManager;
  DeviceManager &deviceManager;
  SettingsManager &settingsManager;
  PresetManager *presetManager =
      nullptr; // optional: notify to trigger grid rebuild
  std::vector<juce::ValueTree> selectedTrees;
  bool isUpdatingFromTree = false;

  // Phase 55.6: Multiple items per row (side-by-side toggles)
  struct UiItem {
    std::unique_ptr<juce::Component> component;
    float weight = 1.0f;
    bool isAutoWidth = false; // Phase 55.10: fit to content
  };
  struct UiRow {
    std::vector<UiItem> items;
    bool isSeparatorRow =
        false; // Phase 55.9: reduced height for separator rows
  };
  std::vector<UiRow> uiRows;

  // Phase 55.9: Draws separator line (optionally with label)
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
  void createControl(const InspectorControl &def, UiRow &currentRow);
  void createAliasRow();
  void createKeyRow();
  void createTouchpadEventRow();

  bool allTreesHaveSameValue(const juce::Identifier &property);
  juce::var getCommonValue(const juce::Identifier &property);

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingInspector)
};
