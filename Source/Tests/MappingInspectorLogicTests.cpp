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

static InspectorControl makeCommandCategoryControl() {
  InspectorControl c;
  c.propertyId = "commandCategory";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[100] = "Sustain";
  c.options[3] = "Latch Toggle";
  c.options[4] = "Panic";
  c.options[6] = "Transpose";
  c.options[8] = "Global Mode Up";
  c.options[9] = "Global Mode Down";
  c.options[110] = "Layer";
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

static InspectorControl makeData1CommandControl() {
  InspectorControl c;
  c.propertyId = "data1";
  c.controlType = InspectorControl::Type::ComboBox;
  c.options[100] = "Sustain";
  c.options[3] = "Latch Toggle";
  c.options[4] = "Panic";
  c.options[6] = "Transpose";
  c.options[8] = "Global Mode Up";
  c.options[9] = "Global Mode Down";
  c.options[110] = "Layer";
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
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 100, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 0);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Layer) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 110, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 10);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Panic) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 4, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 4);
}

TEST(MappingInspectorLogicTest, ApplyCommandCategory_Transpose) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeCommandCategoryControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 6, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 6);
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

// --- data1 command dropdown (Sustain 100 -> 0, Layer 110 -> 10, else id) ---
TEST(MappingInspectorLogicTest, ApplyData1Command_Sustain) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 100, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 0);
}

TEST(MappingInspectorLogicTest, ApplyData1Command_Layer) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 110, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 10);
}

TEST(MappingInspectorLogicTest, ApplyData1Command_TransposeId) {
  juce::ValueTree mapping("Mapping");
  juce::UndoManager undo;
  auto def = makeData1CommandControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 6, &undo);
  EXPECT_EQ(static_cast<int>(mapping.getProperty("data1")), 6);
}

// --- invalid mapping is no-op ---
TEST(MappingInspectorLogicTest, InvalidMapping_NoOp) {
  juce::ValueTree mapping;
  juce::UndoManager undo;
  auto def = makeTypeControl();
  MappingInspectorLogic::applyComboSelectionToMapping(mapping, def, 1, &undo);
  EXPECT_FALSE(mapping.isValid());
}
