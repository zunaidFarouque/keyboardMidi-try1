#include "MappingListPanel.h"

MappingListPanel::MappingListPanel(juce::TableListBox &tableRef,
                                   juce::TextButton &addBtn,
                                   juce::TextButton &removeBtn,
                                   juce::TextButton &moveToLayerBtn,
                                   juce::TextButton &duplicateBtn)
    : table(&tableRef), addButton(&addBtn), removeButton(&removeBtn),
      moveToLayerButton(&moveToLayerBtn), duplicateButton(&duplicateBtn) {}

void MappingListPanel::resized() {
  auto area = getLocalBounds().reduced(4);
  auto buttonArea = area.removeFromBottom(30);
  if (removeButton)
    removeButton->setBounds(buttonArea.removeFromRight(30));
  buttonArea.removeFromRight(4);
  if (addButton)
    addButton->setBounds(buttonArea.removeFromRight(30));
  buttonArea.removeFromRight(4);
  if (duplicateButton)
    duplicateButton->setBounds(buttonArea.removeFromRight(80));
  buttonArea.removeFromRight(4);
  if (moveToLayerButton)
    moveToLayerButton->setBounds(buttonArea.removeFromRight(110));
  area.removeFromBottom(4);
  if (table)
    table->setBounds(area);
}
