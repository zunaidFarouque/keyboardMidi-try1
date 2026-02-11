#pragma once
#include "MappingDefinition.h"
#include "TouchpadMixerManager.h"
#include "TouchpadMixerTypes.h"
#include "TouchpadRelayoutDialog.h"
#include <JuceHeader.h>
#include <memory>
#include <vector>

class TouchpadMixerEditorComponent : public juce::Component {
public:
  explicit TouchpadMixerEditorComponent(TouchpadMixerManager *mgr);
  ~TouchpadMixerEditorComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void setLayout(int index, const TouchpadMixerConfig *config);

  /// Height needed to show all schema rows (used by parent for viewport sizing).
  int getPreferredContentHeight() const;

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
  juce::var getConfigValue(const juce::String &propertyId) const;
  void applyConfigValue(const juce::String &propertyId, const juce::var &value);

  TouchpadMixerManager *manager;
  int selectedIndex = -1;
  TouchpadMixerConfig currentConfig;

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
