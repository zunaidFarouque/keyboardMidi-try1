#include "MappingInspectorLogic.h"

namespace MappingInspectorLogic {

void applyComboSelectionToMapping(juce::ValueTree &mapping,
                                  const InspectorControl &def, int selectedId,
                                  juce::UndoManager *undoManager) {
  // Category ids for Command-type mappings. These are virtual UI ids and do
  // not correspond 1:1 to MIDIQy::CommandID values.
  static constexpr int kCmdCategorySustain     = 100;
  static constexpr int kCmdCategoryLatch       = 101;
  static constexpr int kCmdCategoryPanic       = 102;
  static constexpr int kCmdCategoryTranspose   = 103;
  static constexpr int kCmdCategoryGlobalMode  = 104;
  static constexpr int kCmdCategoryGlobalRoot  = 105;
  static constexpr int kCmdCategoryGlobalScale = 106;
  static constexpr int kCmdCategoryLayer       = 110;
  static constexpr int kCmdCategoryKeyboardGroupSolo = 111;
  static constexpr int kCmdCategoryTouchpadGroupSolo = 112;

  if (!mapping.isValid())
    return;
  const juce::Identifier propId(def.propertyId.toStdString());
  juce::Identifier actualProp =
      (def.propertyId == "sustainStyle" || def.propertyId == "panicMode" ||
       def.propertyId == "layerStyle" || def.propertyId == "keyboardSoloType" ||
       def.propertyId == "touchpadSoloType")
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
    // Map category choice to an underlying CommandID in data1 and initialise
    // any related virtual properties.
    using Cmd = MIDIQy::CommandID;

    switch (selectedId) {
    case kCmdCategorySustain:
      // Default to SustainMomentary; sustainStyle combo will refine it.
      mapping.setProperty("data1", (int)Cmd::SustainMomentary, undoManager);
      break;
    case kCmdCategoryLatch:
      mapping.setProperty("data1", (int)Cmd::LatchToggle, undoManager);
      break;
    case kCmdCategoryPanic:
      mapping.setProperty("data1", (int)Cmd::Panic, undoManager);
      break;
    case kCmdCategoryTranspose:
      mapping.setProperty("data1", (int)Cmd::Transpose, undoManager);
      break;
    case kCmdCategoryGlobalMode:
      mapping.setProperty("data1", (int)Cmd::GlobalModeUp, undoManager);
      break;
    case kCmdCategoryGlobalRoot:
      mapping.setProperty("data1", (int)Cmd::GlobalRootUp, undoManager);
      break;
    case kCmdCategoryGlobalScale:
      mapping.setProperty("data1", (int)Cmd::GlobalScaleNext, undoManager);
      break;
    case kCmdCategoryLayer:
      mapping.setProperty("data1", (int)Cmd::LayerMomentary, undoManager);
      break;
    case kCmdCategoryKeyboardGroupSolo:
      mapping.setProperty("data1", (int)Cmd::KeyboardLayoutGroupSoloMomentary, undoManager);
      break;
    case kCmdCategoryTouchpadGroupSolo:
      mapping.setProperty("data1", (int)Cmd::TouchpadLayoutGroupSoloMomentary, undoManager);
      break;
    default:
      break;
    }
    // Also store the chosen category id (optional but useful for defaults).
    mapping.setProperty("commandCategory", selectedId, undoManager);
    return;
  } else if (def.propertyId == "keyboardSoloType") {
    using Cmd = MIDIQy::CommandID;
    int data1 = (int)Cmd::KeyboardLayoutGroupSoloMomentary;
    if (selectedId == 2) data1 = (int)Cmd::KeyboardLayoutGroupSoloToggle;
    else if (selectedId == 3) data1 = (int)Cmd::KeyboardLayoutGroupSoloSet;
    else if (selectedId == 4) data1 = (int)Cmd::KeyboardLayoutGroupSoloClear;
    mapping.setProperty("data1", data1, undoManager);
    return;
  } else if (def.propertyId == "keyboardLayoutGroupId") {
    mapping.setProperty("keyboardLayoutGroupId", selectedId >= 1 ? selectedId - 1 : 0, undoManager);
    return;
  } else if (def.propertyId == "keyboardSoloScope") {
    mapping.setProperty("keyboardSoloScope", selectedId >= 1 && selectedId <= 3 ? selectedId - 1 : 0, undoManager);
    return;
  } else if (def.propertyId == "keyboardGroupId") {
    mapping.setProperty("keyboardGroupId", selectedId >= 1 ? selectedId - 1 : 0, undoManager);
    return;
  } else if (def.propertyId == "touchpadSoloType") {
    using Cmd = MIDIQy::CommandID;
    int data1 = (int)Cmd::TouchpadLayoutGroupSoloMomentary;
    if (selectedId == 2) data1 = (int)Cmd::TouchpadLayoutGroupSoloToggle;
    else if (selectedId == 3) data1 = (int)Cmd::TouchpadLayoutGroupSoloSet;
    else if (selectedId == 4) data1 = (int)Cmd::TouchpadLayoutGroupSoloClear;
    mapping.setProperty("data1", data1, undoManager);
    return;
  } else if (def.propertyId == "touchpadLayoutGroupId") {
    mapping.setProperty("touchpadLayoutGroupId", selectedId >= 1 ? selectedId - 1 : 0, undoManager);
    return;
  } else if (def.propertyId == "touchpadSoloScope") {
    mapping.setProperty("touchpadSoloScope", selectedId >= 1 && selectedId <= 3 ? selectedId - 1 : 0, undoManager);
    return;
  } else if (def.propertyId == "sustainStyle") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 3)
                               ? (selectedId - 1)
                               : 0);
  } else if (def.propertyId == "layerStyle") {
    valueToSet = juce::var((selectedId == 2) ? 11 : 10);
  } else if (def.propertyId == "globalModeDirection") {
    // Virtual: maps to GlobalModeUp / GlobalModeDown
    using Cmd = MIDIQy::CommandID;
    auto cmd = (selectedId == 2) ? Cmd::GlobalModeDown : Cmd::GlobalModeUp;
    mapping.setProperty("data1", (int)cmd, undoManager);
    mapping.setProperty("commandCategory", kCmdCategoryGlobalMode, undoManager);
    return;
  } else if (def.propertyId == "globalRootMode") {
    using Cmd = MIDIQy::CommandID;
    Cmd cmd = Cmd::GlobalRootUp;
    if (selectedId == 2)
      cmd = Cmd::GlobalRootDown;
    else if (selectedId == 3)
      cmd = Cmd::GlobalRootSet;
    mapping.setProperty("data1", (int)cmd, undoManager);
    mapping.setProperty("commandCategory", kCmdCategoryGlobalRoot, undoManager);
    return;
  } else if (def.propertyId == "globalScaleMode") {
    using Cmd = MIDIQy::CommandID;
    Cmd cmd = Cmd::GlobalScaleNext;
    if (selectedId == 2)
      cmd = Cmd::GlobalScalePrev;
    else if (selectedId == 3)
      cmd = Cmd::GlobalScaleSet;
    mapping.setProperty("data1", (int)cmd, undoManager);
    mapping.setProperty("commandCategory", kCmdCategoryGlobalScale,
                        undoManager);
    return;
  } else if (def.propertyId == "transposeMode") {
    valueToSet = juce::var(selectedId == 2 ? "Local" : "Global");
  } else if (def.propertyId == "transposeModify") {
    valueToSet = juce::var((selectedId >= 1 && selectedId <= 5)
                               ? (selectedId - 1)
                               : 0);
  } else {
    valueToSet = juce::var(selectedId);
  }
  mapping.setProperty(actualProp, valueToSet, undoManager);
}

} // namespace MappingInspectorLogic
