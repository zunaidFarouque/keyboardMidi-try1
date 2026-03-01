#pragma once
#include <JuceHeader.h>

/// Container for the keyboard mappings table and entry-management buttons
/// (Add, Remove, Move to layer, Duplicate). Used as the center panel in the
/// Mappings tab layout.
class MappingListPanel : public juce::Component {
public:
  MappingListPanel(juce::TableListBox &tableRef, juce::TextButton &addBtn,
                  juce::TextButton &removeBtn, juce::TextButton &moveToLayerBtn,
                  juce::TextButton &duplicateBtn);
  void resized() override;

private:
  juce::TableListBox *table;
  juce::TextButton *addButton;
  juce::TextButton *removeButton;
  juce::TextButton *moveToLayerButton;
  juce::TextButton *duplicateButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingListPanel)
};
