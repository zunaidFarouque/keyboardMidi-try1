#pragma once
#include "MappingDefinition.h"
#include "TouchpadMixerManager.h"
#include "TouchpadMixerTypes.h"
#include "TouchpadRelayoutDialog.h"
#include <JuceHeader.h>
#include <functional>
#include <memory>
#include <vector>

class SettingsManager;
class ScaleLibrary;

class TouchpadMixerEditorComponent : public juce::Component {
public:
  explicit TouchpadMixerEditorComponent(TouchpadMixerManager *mgr,
                                        SettingsManager *settingsMgr = nullptr,
                                        ScaleLibrary *scaleLib = nullptr);
  ~TouchpadMixerEditorComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void setLayout(int index, const TouchpadMixerConfig *config);
  void setMapping(int index, const TouchpadMappingConfig *config);

  /// Height needed to show all schema rows (used by parent for viewport sizing).
  int getPreferredContentHeight() const;

  /// Called when schema/row count may have changed (e.g. after rebuildUI).
  /// Parent can use this to resize the viewport content.
  std::function<void()> onContentHeightMaybeChanged;

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

  void rebuildUI();
  void createControl(const InspectorControl &def,
                     std::vector<UiItem> &currentRow);
  void launchRelayoutDialog();
  void onRelayoutRegionChosen(float x1, float y1, float x2, float y2);
  juce::var getConfigValue(const juce::String &propertyId) const;
  void applyConfigValue(const juce::String &propertyId, const juce::var &value);

  TouchpadMixerManager *manager;
  SettingsManager *settingsManager;
  ScaleLibrary *scaleLibrary = nullptr;
  enum class SelectionKind { None, Layout, Mapping };
  SelectionKind selectionKind = SelectionKind::None;
  int selectedLayoutIndex = -1;
  int selectedMappingIndex = -1;
  TouchpadMixerConfig currentConfig;
  TouchpadMappingConfig currentMapping;

  std::vector<UiRow> uiRows;

  std::unique_ptr<TouchpadRelayoutDialog> relayoutDialog;

  class SeparatorComponent : public juce::Component {
  public:
    SeparatorComponent(const juce::String &label,
                       juce::Justification justification);
    void paint(juce::Graphics &g) override;

  private:
    juce::String labelText;
    juce::Justification textAlign;
  };

  // LAF so combo popups parent to top-level (not clipped by Viewport)
  struct ComboPopupLAF : juce::LookAndFeel_V4 {
    juce::PopupMenu::Options
    getOptionsForComboBoxPopupMenu(juce::ComboBox &box,
                                   juce::Label &label) override;
  } comboPopupLAF;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerEditorComponent)
};
