#pragma once
#include "MappingDefinition.h"
#include <JuceHeader.h>

namespace KeyboardMappingInspectorLogic {

// Applies a ComboBox selection to a single mapping ValueTree (same rules as
// KeyboardMappingInspector combo onChange). Use from UI when user picks a combo option;
// testable without MessageManager or GUI.
void applyComboSelectionToMapping(juce::ValueTree &mapping,
                                  const InspectorControl &def, int selectedId,
                                  juce::UndoManager *undoManager);

} // namespace KeyboardMappingInspectorLogic
