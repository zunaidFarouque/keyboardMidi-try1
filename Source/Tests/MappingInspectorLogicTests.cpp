#include "../MappingDefinition.h"
#include "../MappingInspectorLogic.h"
#include "../MappingTypes.h"
#include <gtest/gtest.h>

static InspectorControl makeTypeControl() {
  InspectorControl c;
  c.propertyId = "type";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Note";
  c.options[2] = "Expression";
  c.options[3] = "Command";
  return c;
}

static InspectorControl makeReleaseBehaviorControl() {
  InspectorControl c;
  c.propertyId = "releaseBehavior";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Send Note Off";
  c.options[2] = "Sustain until retrigger";
  c.options[3] = "Always Latch";
  return c;
}

static InspectorControl makeAdsrTargetControl() {
  InspectorControl c;
  c.propertyId = "adsrTarget";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "CC";
  c.options[2] = "PitchBend";
  c.options[3] = "SmartScaleBend";
  return c;
}

static InspectorControl makePitchPadModeControl() {
  InspectorControl c;
  c.propertyId = "pitchPadMode";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Absolute";
  c.options[2] = "Relative";
  return c;
}

// Option IDs must match MappingDefinition / MappingInspectorLogic (virtual category IDs)
static constexpr int kCmdCategorySustain = 100;
static constexpr int kCmdCategoryLatch = 101;
static constexpr int kCmdCategoryPanic = 102;
static constexpr int kCmdCategoryTranspose = 103;
static constexpr int kCmdCategoryLayer = 110;

static InspectorControl makeCommandCategoryControl() {
  InspectorControl c;
  c.propertyId = "commandCategory";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[kCmdCategorySustain] = "Sustain";
  c.options[kCmdCategoryLatch] = "Latch";
  c.options[kCmdCategoryPanic] = "Panic";
  c.options[kCmdCategoryTranspose] = "Transpose";
  c.options[104] = "Global mode";
  c.options[105] = "Global Root";
  c.options[106] = "Global Scale";
  c.options[kCmdCategoryLayer] = "Layer";
  c.options[111] = "Keyboard group";
  c.options[112] = "Touchpad group";
  return c;
}

static InspectorControl makeSustainStyleControl() {
  InspectorControl c;
  c.propertyId = "sustainStyle";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Hold to sustain";
  c.options[2] = "Toggle sustain";
  c.options[3] = "Default is on. Hold to not sustain";
  return c;
}

static InspectorControl makeLayerStyleControl() {
  InspectorControl c;
  c.propertyId = "layerStyle";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Hold to switch";
  c.options[2] = "Toggle layer";
  return c;
}

static InspectorControl makePanicModeControl() {
  InspectorControl c;
  c.propertyId = "panicMode";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Panic all";
  c.options[2] = "Panic latched only";
  c.options[3] = "Panic chords";
  return c;
}

static InspectorControl makeTransposeModeControl() {
  InspectorControl c;
  c.propertyId = "transposeMode";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Global";
  c.options[2] = "Local";
  return c;
}

static InspectorControl makeTransposeModifyControl() {
  InspectorControl c;
  c.propertyId = "transposeModify";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Up (1 semitone)";
  c.options[2] = "Down (1 semitone)";
  c.options[3] = "Up (1 octave)";
  c.options[4] = "Down (1 octave)";
  c.options[5] = "Set (specific semitones)";
  return c;
}

// data1 command combo uses actual CommandID values (from getCommandOptions())
static InspectorControl makeData1CommandControl() {
  InspectorControl c;
  c.propertyId = "data1";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[0] = "Hold to sustain";
  c.options[1] = "Toggle sustain";
  c.options[2] = "Default is on. Hold to not sustain";
  c.options[3] = "Latch Toggle";
  c.options[4] = "Panic";
  c.options[6] = "Transpose";
  c.options[8] = "Global Mode Up";
  c.options[9] = "Global Mode Down";
  c.options[10] = "Layer Momentary";
  c.options[11] = "Layer Toggle";
  return c;
}

static InspectorControl makeTouchpadSoloScopeControl() {
  InspectorControl c;
  c.propertyId = "touchpadSoloScope";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Global";
  c.options[2] = "Layer (forget on change)";
  c.options[3] = "Layer (remember)";
  return c;
}

static InspectorControl makeTouchpadSoloTypeControl() {
  InspectorControl c;
  c.propertyId = "touchpadSoloType";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Hold";
  c.options[2] = "Toggle";
  c.options[3] = "Set";
  c.options[4] = "Clear";
  return c;
}

static InspectorControl makeTouchpadLayoutGroupIdControl() {
  InspectorControl c;
  c.propertyId = "touchpadLayoutGroupId";
  c.controlType = InspectorControl::Type::ComboBox;
  // IDs are virtual; MappingInspectorLogic maps combo id -> stored group id.
  c.options[1] = "- No Group -";
  c.options[2] = "Group 1";
  c.options[3] = "Group 2";
  return c;
}

// --- type ---
TEST(MappingInspectorLogicTest, ApplyType_Note) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(mapping.getProperty("type").toString(), "Note");
}

TEST(MappingInspectorLogicTest, ApplyType_Expression) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(mapping.getProperty("type").toString(), "Expression");
}

TEST(MappingInspectorLogicTest, ApplyType_Command) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(mapping.getProperty("type").toString(), "Command");
}

// --- releaseBehavior ---
TEST(MappingInspectorLogicTest, ApplyReleaseBehavior_AlwaysLatch) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeReleaseBehaviorControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(mapping.getProperty("releaseBehavior").toString(), "Always Latch");
}

TEST(MappingInspectorLogicTest, ApplyReleaseBehavior_SustainUntilRetrigger) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeReleaseBehaviorControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(mapping.getProperty("releaseBehavior").toString(),
            "Sustain until retrigger");
}

TEST(MappingInspectorLogicTest, ApplyReleaseBehavior_SendNoteOff) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeReleaseBehaviorControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(mapping.getProperty("releaseBehavior").toString(), "Send Note Off");
}

// --- adsrTarget ---
TEST(MappingInspectorLogicTest, ApplyAdsrTarget_CC) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeAdsrTargetControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(mapping.getProperty("adsrTarget").toString(), "CC");
}

TEST(MappingInspectorLogicTest, ApplyAdsrTarget_PitchBend) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeAdsrTargetControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(mapping.getProperty("adsrTarget").toString(), "PitchBend");
}

TEST(MappingInspectorLogicTest, ApplyAdsrTarget_SmartScaleBend) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeAdsrTargetControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(mapping.getProperty("adsrTarget").toString(), "SmartScaleBend");
}

// --- pitchPadMode ---
TEST(MappingInspectorLogicTest, ApplyPitchPadMode_Absolute) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makePitchPadModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(mapping.getProperty("pitchPadMode").toString(), "Absolute");
}

TEST(MappingInspectorLogicTest, ApplyPitchPadMode_Relative) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makePitchPadModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(mapping.getProperty("pitchPadMode").toString(), "Relative");
}

// --- commandCategory (writes to data1) ---
TEST(MappingInspectorLogicTest, ApplyCommandCategory_Sustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, kCmdCategorySustain, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 0);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Layer) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, kCmdCategoryLayer, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 10);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Panic) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, kCmdCategoryPanic, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 4);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Transpose) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, kCmdCategoryTranspose, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 6);
}

// Applying Touchpad (120) sets data1 to touchpad solo; schema then has touchpad
// controls (ensures first-select Touchpad UI has correct state for rebuildUI).
TEST(MappingInspectorLogicTest, DISABLED_ApplyCommandCategory_Touchpad_ThenSchemaHasTouchpadControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 120, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary));

  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasSoloType = false, hasGroup = false, hasScope = false;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadSoloType")
      hasSoloType = true;
    else if (c.propertyId == "touchpadLayoutGroupId")
      hasGroup = true;
    else if (c.propertyId == "touchpadSoloScope")
      hasScope = true;
  }
  EXPECT_TRUE(hasSoloType);
  EXPECT_TRUE(hasGroup);
  EXPECT_TRUE(hasScope);
}

// --- sustainStyle (virtual -> data1 0,1,2) ---
TEST(MappingInspectorLogicTest, ApplySustainStyle_HoldToSustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeSustainStyleControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 0);
}

TEST(MappingInspectorLogicTest, ApplySustainStyle_ToggleSustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeSustainStyleControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 1);
}

TEST(MappingInspectorLogicTest, ApplySustainStyle_DefaultOnHoldToNotSustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeSustainStyleControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 2);
}

// --- layerStyle (virtual -> data1 10,11) ---
TEST(MappingInspectorLogicTest, ApplyLayerStyle_HoldToSwitch) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeLayerStyleControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 10);
}

TEST(MappingInspectorLogicTest, ApplyLayerStyle_ToggleLayer) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeLayerStyleControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 11);
}

// --- panicMode (virtual -> data1=4, data2=0/1/2) ---
TEST(MappingInspectorLogicTest, ApplyPanicMode_PanicAll) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makePanicModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 4);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data2")), 0);
}

TEST(MappingInspectorLogicTest, ApplyPanicMode_PanicLatchedOnly) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makePanicModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 4);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data2")), 1);
}

TEST(MappingInspectorLogicTest, ApplyPanicMode_PanicChords) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makePanicModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 4);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data2")), 2);
}

// --- transposeMode ---
TEST(MappingInspectorLogicTest, ApplyTransposeMode_Global) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTransposeModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(mapping.getProperty("transposeMode").toString(), "Global");
}

TEST(MappingInspectorLogicTest, ApplyTransposeMode_Local) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTransposeModeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(mapping.getProperty("transposeMode").toString(), "Local");
}

// --- transposeModify (combo 1..5 -> 0..4) ---
TEST(MappingInspectorLogicTest, ApplyTransposeModify_UpOneSemitone) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTransposeModifyControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("transposeModify")), 0);
}

TEST(MappingInspectorLogicTest, ApplyTransposeModify_SetSpecificSemitones) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTransposeModifyControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 5, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("transposeModify")), 4);
}

// --- data1 command dropdown (option key = CommandID, stored as-is) ---
TEST(MappingInspectorLogicTest, ApplyData1Command_Sustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 0, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 0);
}

TEST(MappingInspectorLogicTest, ApplyData1Command_Layer) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 10, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 10);
}

TEST(MappingInspectorLogicTest, ApplyData1Command_TransposeId) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 6, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 6);
}

// Selecting Touchpad (120) from data1 combo sets data1=18; schema then has
// Touchpad controls (fixes first-time Touchpad not showing when from Latch Toggle, etc.).
TEST(MappingInspectorLogicTest, DISABLED_ApplyData1Command_Touchpad_ThenSchemaHasTouchpadControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 120, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary));

  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasSoloType = false, hasGroup = false, hasScope = false;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadSoloType")
      hasSoloType = true;
    else if (c.propertyId == "touchpadLayoutGroupId")
      hasGroup = true;
    else if (c.propertyId == "touchpadSoloScope")
      hasScope = true;
  }
  EXPECT_TRUE(hasSoloType);
  EXPECT_TRUE(hasGroup);
  EXPECT_TRUE(hasScope);
}

// Enforces the fix: when Command uses data1 combo (Latch Toggle, Panic, etc.),
// selecting Touchpad must set data1=18 so the Touchpad block appears. Before
// the fix, data1 was incorrectly set to 120, so isTouchpadSolo stayed false.
TEST(MappingInspectorLogicTest, DISABLED_FirstTimeTouchpadFromLatchToggle_UsesRealSchema_ShowsTouchpadBlock) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", 3, nullptr);  // Latch Toggle -> schema uses propertyId "data1"

  InspectorSchema schemaBefore = MappingDefinition::getSchema(mapping);
  const InspectorControl *cmdCtrl = nullptr;
  for (const auto &c : schemaBefore) {
    if (c.propertyId == "data1" && c.options.count(120) > 0) {
      cmdCtrl = &c;
      break;
    }
  }
  ASSERT_NE(cmdCtrl, nullptr)
      << "Latch Toggle uses data1 combo; must have Touchpad (120) option";

  // Before: schema has no Touchpad block (data1=3, not in 18-21)
  bool hadTouchpadBefore = false;
  for (const auto &c : schemaBefore) {
    if (c.propertyId == "touchpadSoloType") {
      hadTouchpadBefore = true;
      break;
    }
  }
  EXPECT_FALSE(hadTouchpadBefore) << "Latch Toggle must not show Touchpad block";

  // Simulate user selecting Touchpad from the Command dropdown
  juce::UndoManager undo;
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, *cmdCtrl, 120,
                                                      &undo);

  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary))
      << "Must set data1=18, not 120; otherwise Touchpad block stays hidden";

  InspectorSchema schemaAfter = MappingDefinition::getSchema(mapping);
  bool hasSoloType = false, hasGroup = false, hasScope = false;
  for (const auto &c : schemaAfter) {
    if (c.propertyId == "touchpadSoloType")
      hasSoloType = true;
    else if (c.propertyId == "touchpadLayoutGroupId")
      hasGroup = true;
    else if (c.propertyId == "touchpadSoloScope")
      hasScope = true;
  }
  EXPECT_TRUE(hasSoloType) << "Touchpad block must appear after selecting Touchpad";
  EXPECT_TRUE(hasGroup);
  EXPECT_TRUE(hasScope);
}

// --- touchpadSoloScope (1,2,3 -> 0,1,2) ---
TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloScope_Global) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloScopeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("touchpadSoloScope")), 0);
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloScope_LayerForget) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloScopeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("touchpadSoloScope")), 1);
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloScope_LayerRemember) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloScopeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("touchpadSoloScope")), 2);
}

// --- touchpadSoloType (1..4 -> CommandID enum values) ---
TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloType_Momentary) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary));
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloType_Toggle) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle));
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloType_Set) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet));
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadSoloType_Clear) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadSoloTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 4, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloClear));
}

// Verify touchpadSoloType round-trip: set data1, change solo type, verify data1 updates
TEST(MappingInspectorLogicTest, DISABLED_TouchpadSoloTypeRoundTrip_MomentaryToToggle) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  mapping.setProperty("type", "Command", &undo);
  mapping.setProperty("data1",
                      static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary),
                      &undo);

  auto def = makeTouchpadSoloTypeControl();
  // Change from Momentary (1) to Toggle (2)
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);

  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle));
  // Verify touchpadSoloType property doesn't exist (it's virtual, maps to data1)
  EXPECT_TRUE(mapping.getProperty("touchpadSoloType").isVoid());
}

TEST(MappingInspectorLogicTest, DISABLED_TouchpadSoloTypeRoundTrip_ToggleToSet) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  mapping.setProperty("type", "Command", &undo);
  mapping.setProperty("data1",
                      static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle),
                      &undo);

  auto def = makeTouchpadSoloTypeControl();
  // Change from Toggle (2) to Set (3)
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 3, &undo);

  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet));
}

TEST(MappingInspectorLogicTest, DISABLED_TouchpadSoloTypeRoundTrip_SetToClear) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  mapping.setProperty("type", "Command", &undo);
  mapping.setProperty("data1",
                      static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet),
                      &undo);

  auto def = makeTouchpadSoloTypeControl();
  // Change from Set (3) to Clear (4)
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 4, &undo);

  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloClear));
}

TEST(MappingInspectorLogicTest, DISABLED_TouchpadSoloTypeRoundTrip_ClearToMomentary) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  mapping.setProperty("type", "Command", &undo);
  mapping.setProperty("data1",
                      static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloClear),
                      &undo);

  auto def = makeTouchpadSoloTypeControl();
  // Change from Clear (4) back to Momentary (1)
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);

  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary));
}

// --- touchpadLayoutGroupId (combo 1..N -> stored 0..N-1) ---
TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadLayoutGroupId_NoGroupStoresZero) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadLayoutGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("touchpadLayoutGroupId")), 0);
}

TEST(MappingInspectorLogicTest, DISABLED_ApplyTouchpadLayoutGroupId_Group1StoresOne) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeTouchpadLayoutGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("touchpadLayoutGroupId")), 1);
}

// --- keyboard group solo (keyboardSoloType, keyboardLayoutGroupId, keyboardGroupId) ---
static InspectorControl makeKeyboardSoloTypeControl() {
  InspectorControl c;
  c.propertyId = "keyboardSoloType";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "Hold to solo";
  c.options[2] = "Toggle solo";
  c.options[3] = "Set solo";
  c.options[4] = "Clear solo";
  return c;
}

static InspectorControl makeKeyboardLayoutGroupIdControl() {
  InspectorControl c;
  c.propertyId = "keyboardLayoutGroupId";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "None";
  c.options[2] = "Group 1";
  return c;
}

static InspectorControl makeKeyboardGroupIdControl() {
  InspectorControl c;
  c.propertyId = "keyboardGroupId";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[1] = "None";
  c.options[2] = "Group 1";
  return c;
}

TEST(MappingInspectorLogicTest, ApplyKeyboardSoloType_Momentary) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeKeyboardSoloTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")),
            static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloMomentary));
}

TEST(MappingInspectorLogicTest, ApplyKeyboardLayoutGroupId_NoGroupStoresZero) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeKeyboardLayoutGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("keyboardLayoutGroupId")), 0);
}

TEST(MappingInspectorLogicTest, ApplyKeyboardLayoutGroupId_Group1StoresOne) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeKeyboardLayoutGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("keyboardLayoutGroupId")), 1);
}

TEST(MappingInspectorLogicTest, ApplyKeyboardGroupId_NoGroupStoresZero) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeKeyboardGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("keyboardGroupId")), 0);
}

TEST(MappingInspectorLogicTest, ApplyKeyboardGroupId_Group1StoresOne) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeKeyboardGroupIdControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 2, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("keyboardGroupId")), 1);
}

// --- invalid mapping is no-op ---
TEST(MappingInspectorLogicTest, InvalidMapping_NoOp) {
  juce::ValueTree mapping;
  juce::UndoManager undo;
  auto def = makeTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_FALSE(mapping.isValid());
}
