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
       def.propertyId == "layerStyle" || def.propertyId == "touchpadSoloType")
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
    // Virtual category selector: Sustain / Layer / Touchpad / direct commands.
    static constexpr int kSustainCategoryId = 100;
    static constexpr int kLayerCategoryId = 110;
    static constexpr int kTouchpadCategoryId = 120;

    int currentData1 = (int)mapping.getProperty("data1", 0);

    if (selectedId == kSustainCategoryId) {
      valueToSet = juce::var(0);
    } else if (selectedId == kLayerCategoryId) {
      valueToSet = juce::var(10);
    } else if (selectedId == kTouchpadCategoryId) {
      // When switching to Touchpad, preserve existing solo command if present,
      // otherwise default to Momentary.
      using MIDIQy::CommandID;
      const int soloMin =
          static_cast<int>(CommandID::TouchpadLayoutGroupSoloMomentary);
      const int soloMax =
          static_cast<int>(CommandID::TouchpadLayoutGroupSoloClear);
      int newCmd = currentData1;
      if (newCmd < soloMin || newCmd > soloMax)
        newCmd = soloMin;
      valueToSet = juce::var(newCmd);
    } else {
      valueToSet = juce::var(selectedId);
    }
  } else if (def.propertyId == "sustainStyle") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 3) ? (selectedId - 1) : 0);
  } else if (def.propertyId == "layerStyle") {
    valueToSet = juce::var((selectedId == 2) ? 11 : 10);
  } else if (def.propertyId == "transposeMode") {
    valueToSet = juce::var(selectedId == 2 ? "Local" : "Global");
  } else if (def.propertyId == "transposeModify") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 5) ? (selectedId - 1) : 0);
  } else if (def.propertyId == "touchpadSoloScope") {
    // Combo IDs 1,2,3 -> stored values 0,1,2
    int v = 0;
    if (selectedId == 2)
      v = 1;
    else if (selectedId == 3)
      v = 2;
    valueToSet = juce::var(v);
  } else if (def.propertyId == "touchpadSoloType") {
    // Map UI solo type (1-4) to concrete CommandID.
    using MIDIQy::CommandID;
    CommandID cmd = CommandID::TouchpadLayoutGroupSoloMomentary;
    switch (selectedId) {
    case 1:
      cmd = CommandID::TouchpadLayoutGroupSoloMomentary;
      break;
    case 2:
      cmd = CommandID::TouchpadLayoutGroupSoloToggle;
      break;
    case 3:
      cmd = CommandID::TouchpadLayoutGroupSoloSet;
      break;
    case 4:
      cmd = CommandID::TouchpadLayoutGroupSoloClear;
      break;
    default:
      break;
    }
    valueToSet = juce::var(static_cast<int>(cmd));
  } else if (def.propertyId == "touchpadLayoutGroupId") {
    // Combo ids are stored as groupId+1. 1 => 0 ("- No Group -").
    int groupId = selectedId - 1;
    if (groupId < 0)
      groupId = 0;
    valueToSet = juce::var(groupId);
  } else if (def.propertyId == "data1" &&
             (def.options.count(100) > 0 || def.options.count(110) > 0)) {
    valueToSet = juce::var((selectedId == 100) ? 0 : (selectedId == 110) ? 10 : selectedId);
  } else {
    valueToSet = juce::var(selectedId);
  }
  mapping.setProperty(actualProp, valueToSet, undoManager);
}

} // namespace MappingInspectorLogic
