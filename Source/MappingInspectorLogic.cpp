#include "MappingInspectorLogic.h"

namespace MappingInspectorLogic {

void applyComboSelectionToMapping(juce::ValueTree &mapping,
                                  const InspectorControl &def, int selectedId,
                                  juce::UndoManager *undoManager) {
  if (!mapping.isValid())
    return;
  const juce::Identifier propId(def.propertyId.toStdString());
  juce::Identifier actualProp =
      (def.propertyId == "commandCategory" ||
       def.propertyId == "sustainStyle" || def.propertyId == "panicMode" ||
       def.propertyId == "layerStyle")
          ? juce::Identifier("data1")
          : propId;

  if (def.propertyId == "panicMode") {
    int mode = (selectedId == 2) ? 1 : (selectedId == 3) ? 2 : 0;
    mapping.setProperty("data1", 4, undoManager);
    mapping.setProperty("data2", mode, undoManager);
    return;
  }

  juce::var valueToSet;
  if (def.propertyId == "type" || def.propertyId == "adsrTarget" ||
      def.propertyId == "releaseBehavior" ||
      def.propertyId == "pitchPadMode") {
    auto it = def.options.find(selectedId);
    valueToSet =
        (it != def.options.end()) ? juce::var(it->second) : juce::var();
  } else if (def.propertyId == "commandCategory") {
    valueToSet = (selectedId == 100)   ? juce::var(0)
                 : (selectedId == 110) ? juce::var(10)
                                      : juce::var(selectedId);
  } else if (def.propertyId == "sustainStyle") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 3) ? (selectedId - 1) : 0);
  } else if (def.propertyId == "layerStyle") {
    valueToSet = juce::var((selectedId == 2) ? 11 : 10);
  } else if (def.propertyId == "transposeMode") {
    valueToSet = juce::var(selectedId == 2 ? "Local" : "Global");
  } else if (def.propertyId == "transposeModify") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 5) ? (selectedId - 1) : 0);
  } else if (def.propertyId == "data1" &&
             (def.options.count(100) > 0 || def.options.count(110) > 0)) {
    valueToSet = juce::var((selectedId == 100) ? 0 : (selectedId == 110) ? 10 : selectedId);
  } else {
    valueToSet = juce::var(selectedId);
  }
  mapping.setProperty(actualProp, valueToSet, undoManager);
}

} // namespace MappingInspectorLogic
