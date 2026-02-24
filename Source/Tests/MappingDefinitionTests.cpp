#include "../MappingDefinition.h"
#include "../MappingDefaults.h"
#include "../MappingTypes.h"
#include <gtest/gtest.h>

// Touchpad mappings are edited in the Touchpad tab; schema in Mappings tab is
// keyboard-only. Note schema still has releaseBehaviour and Note control.
TEST(MappingDefinitionTest, TouchpadMappingSchemaHasNoteAndReleaseControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", 0, nullptr); // Finger 1: Down
  mapping.setProperty("type", "Note", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  bool hasReleaseBehavior = false;
  bool hasNoteControl = false;
  for (const auto &c : schema) {
    if (c.propertyId == "releaseBehavior")
      hasReleaseBehavior = true;
    if (c.propertyId == "data1" && c.label == "Note")
      hasNoteControl = true;
  }
  EXPECT_TRUE(hasReleaseBehavior)
      << "Touchpad Note schema should have releaseBehavior (release behaviour)";
  EXPECT_TRUE(hasNoteControl)
      << "Touchpad Note schema should have Note (data1) control";
}

TEST(MappingDefinitionTest, SchemaGeneration) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Note", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  ASSERT_GT(schema.size(), 0u) << "Schema should have at least one control";

  // Find control with property "type" – should be ComboBox
  const InspectorControl *typeControl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "type") {
      typeControl = &c;
      break;
    }
  }
  ASSERT_NE(typeControl, nullptr) << "Schema should have a 'type' control";
  EXPECT_EQ(typeControl->controlType, InspectorControl::Type::ComboBox);

  // Find control with property "data1" – label "Note", format NoteName
  const InspectorControl *data1Control = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "data1") {
      data1Control = &c;
      break;
    }
  }
  ASSERT_NE(data1Control, nullptr) << "Schema should have a 'data1' control";
  EXPECT_EQ(data1Control->label, "Note");
  EXPECT_EQ(data1Control->valueFormat, InspectorControl::Format::NoteName);
}

// Phase 55.2: Command with Layer command -> data2 is ComboBox (Target Layer)
TEST(MappingDefinitionTest, CommandContext) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::LayerMomentary,
                      nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *data2Control = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "data2") {
      data2Control = &c;
      break;
    }
  }
  ASSERT_NE(data2Control, nullptr) << "Schema should have a 'data2' control";
  EXPECT_EQ(data2Control->controlType, InspectorControl::Type::ComboBox)
      << "data2 for Layer command should be ComboBox (Layer Selector)";
}

// Layer unified UI: data1 10 or 11 -> commandCategory, layerStyle, data2
TEST(MappingDefinitionTest, LayerUnifiedUI) {
  for (int data1 : {10, 11}) {
    juce::ValueTree mapping("Mapping");
    mapping.setProperty("type", "Command", nullptr);
    mapping.setProperty("data1", data1, nullptr);
    mapping.setProperty("data2", 1, nullptr);

    InspectorSchema schema = MappingDefinition::getSchema(mapping);

    const InspectorControl *cmdCtrl = nullptr;
    const InspectorControl *layerStyleCtrl = nullptr;
    const InspectorControl *data2Ctrl = nullptr;
    for (const auto &c : schema) {
      if (c.propertyId == "commandCategory")
        cmdCtrl = &c;
      else if (c.propertyId == "layerStyle")
        layerStyleCtrl = &c;
      else if (c.propertyId == "data2")
        data2Ctrl = &c;
    }
    ASSERT_NE(cmdCtrl, nullptr) << "Layer command should use commandCategory";
    EXPECT_EQ(cmdCtrl->label, "Command");
    ASSERT_NE(layerStyleCtrl, nullptr)
        << "Layer command should have layerStyle (Style dropdown)";
    EXPECT_EQ(layerStyleCtrl->options.size(), 2u)
        << "layerStyle should have Hold to switch, Toggle layer";
    ASSERT_NE(data2Ctrl, nullptr) << "Layer command should have Target Layer";
  }
}

// Touchpad solo command: unified UI with category + scope + type + group id

// CC Expression uses Value when On / Value when Off (no data2 peak slider)
TEST(MappingDefinitionTest, EnvelopeContext) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("useCustomEnvelope", true, nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping, 12);

  const InspectorControl *valOn = nullptr;
  const InspectorControl *valOff = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadValueWhenOn")
      valOn = &c;
    if (c.propertyId == "touchpadValueWhenOff")
      valOff = &c;
  }
  ASSERT_NE(valOn, nullptr) << "CC Expression should have Value when On";
  ASSERT_NE(valOff, nullptr) << "CC Expression should have Value when Off";
  EXPECT_DOUBLE_EQ(valOn->min, 0.0);
  EXPECT_DOUBLE_EQ(valOn->max, 127.0);
}

// PitchBend Expression uses Bend (semitones) = data2; no Value when On/Off
TEST(MappingDefinitionTest, PitchBendHasBendSemitonesNoValueWhenOnOff) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);

  const int pbRange = 12;
  InspectorSchema schema = MappingDefinition::getSchema(mapping, pbRange);

  const InspectorControl *data2Ctrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "data2") {
      data2Ctrl = &c;
      break;
    }
  }
  ASSERT_NE(data2Ctrl, nullptr) << "PitchBend should have Bend (semitones)";
  EXPECT_DOUBLE_EQ(data2Ctrl->min, -static_cast<double>(pbRange));
  EXPECT_DOUBLE_EQ(data2Ctrl->max, static_cast<double>(pbRange));
  bool hasValueWhenOn = false;
  for (const auto &c : schema)
    if (c.propertyId == "touchpadValueWhenOn")
      hasValueWhenOn = true;
  EXPECT_FALSE(hasValueWhenOn) << "PitchBend should not have Value when On";
}

// SmartScaleBend uses Scale Steps only; no data2, no Value when On/Off
TEST(MappingDefinitionTest, SmartScaleBendNoPeakSlider) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "SmartScaleBend", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping, 12);

  for (const auto &c : schema) {
    EXPECT_NE(c.propertyId, "data2")
        << "SmartScaleBend uses Scale Steps only, not data2";
    EXPECT_NE(c.propertyId, "touchpadValueWhenOn")
        << "SmartScaleBend should not have Value when On";
    EXPECT_NE(c.propertyId, "touchpadValueWhenOff")
        << "SmartScaleBend should not have Value when Off";
  }
}

// CC Expression: no sendReleaseValue/releaseValue (Value when Off always sent)
TEST(MappingDefinitionTest, CCExpressionNoSendReleaseValueControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);
  mapping.setProperty("useCustomEnvelope", false, nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  for (const auto &c : schema) {
    EXPECT_NE(c.propertyId, "sendReleaseValue")
        << "CC Expression should not have Send value on Release toggle";
    EXPECT_NE(c.propertyId, "releaseValue")
        << "CC Expression should not have release value slider";
  }
}

// PitchBend Expression: has "Reset pitch on release" toggle only
TEST(MappingDefinitionTest, PitchBendHasResetPitchOnReleaseToggle) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  mapping.setProperty("useCustomEnvelope", false, nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *sendRelControl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "sendReleaseValue") {
      sendRelControl = &c;
      break;
    }
  }
  ASSERT_NE(sendRelControl, nullptr)
      << "PitchBend should have Reset pitch on release toggle";
  EXPECT_EQ(sendRelControl->controlType, InspectorControl::Type::Toggle);
  EXPECT_EQ(sendRelControl->label, "Reset pitch on release");
  EXPECT_FLOAT_EQ(sendRelControl->widthWeight, 1.0f);
}

// Touchpad editor schema: no Enabled (in header), no channel in Expression body
TEST(MappingDefinitionTest, TouchpadEditorSchemaOmitsEnabledAndChannel) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping, 12, true);

  bool hasEnabled = false;
  bool hasChannel = false;
  for (const auto &c : schema) {
    if (c.propertyId == "enabled") hasEnabled = true;
    if (c.propertyId == "channel") hasChannel = true;
  }
  EXPECT_FALSE(hasEnabled) << "Touchpad schema should omit Enabled (lives in header)";
  EXPECT_FALSE(hasChannel) << "Touchpad schema should omit channel (lives in header)";
}

// --- Enabled toggle and isMappingEnabled ---
TEST(MappingDefinitionTest, SchemaHasEnabledToggle) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Note", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasEnabled = false;
  for (const auto &c : schema)
    if (c.propertyId == "enabled" && c.label == "Enabled") {
      hasEnabled = true;
      break;
    }
  EXPECT_TRUE(hasEnabled) << "Schema should have Enabled toggle";
}

TEST(MappingDefinitionTest, IsMappingEnabledDefaultTrue) {
  juce::ValueTree mapping("Mapping");
  EXPECT_TRUE(MappingDefinition::isMappingEnabled(mapping));
}

TEST(MappingDefinitionTest, IsMappingEnabledFalseWhenSet) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("enabled", false, nullptr);
  EXPECT_FALSE(MappingDefinition::isMappingEnabled(mapping));
}

TEST(MappingDefinitionTest, IsMappingEnabledTrueWhenSet) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("enabled", true, nullptr);
  EXPECT_TRUE(MappingDefinition::isMappingEnabled(mapping));
}

// --- Keyboard Note: followTranspose in schema ---
TEST(MappingDefinitionTest, KeyboardNoteHasFollowTranspose) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Note", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasFollowTranspose = false;
  for (const auto &c : schema)
    if (c.propertyId == "followTranspose") {
      hasFollowTranspose = true;
      break;
    }
  EXPECT_TRUE(hasFollowTranspose)
      << "Keyboard Note schema should have followTranspose";
}

// --- Keyboard Note: releaseBehaviour has all 3 options when not touchpad Up ---
TEST(MappingDefinitionTest, KeyboardNoteReleaseBehaviorHasAllThreeOptions) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Note", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  const InspectorControl *releaseCtrl = nullptr;
  for (const auto &c : schema)
    if (c.propertyId == "releaseBehavior") {
      releaseCtrl = &c;
      break;
    }
  ASSERT_NE(releaseCtrl, nullptr);
  EXPECT_NE(releaseCtrl->options.find(1), releaseCtrl->options.end())
      << "Send Note Off (1)";
  EXPECT_NE(releaseCtrl->options.find(2), releaseCtrl->options.end())
      << "Sustain until retrigger (2)";
  EXPECT_NE(releaseCtrl->options.find(3), releaseCtrl->options.end())
      << "Always Latch (3)";
}

// --- Expression: Use Custom Envelope true + CC -> ADSR sliders ---
TEST(MappingDefinitionTest, ExpressionCCCustomEnvelopeShowsAdsrSliders) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);
  mapping.setProperty("useCustomEnvelope", true, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasAttack = false, hasDecay = false, hasSustain = false, hasRelease = false;
  for (const auto &c : schema) {
    if (c.propertyId == "adsrAttack") hasAttack = true;
    if (c.propertyId == "adsrDecay") hasDecay = true;
    if (c.propertyId == "adsrSustain") hasSustain = true;
    if (c.propertyId == "adsrRelease") hasRelease = true;
  }
  EXPECT_TRUE(hasAttack) << "CC with custom envelope should have Attack";
  EXPECT_TRUE(hasDecay) << "CC with custom envelope should have Decay";
  EXPECT_TRUE(hasSustain) << "CC with custom envelope should have Sustain";
  EXPECT_TRUE(hasRelease) << "CC with custom envelope should have Release";
}

// --- Expression: PitchBend/SmartScaleBend -> custom envelope disabled in schema ---
TEST(MappingDefinitionTest, ExpressionPitchBendHidesCustomEnvelopeControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasUseCustomEnvelope = false;
  for (const auto &c : schema)
    if (c.propertyId == "useCustomEnvelope") {
      hasUseCustomEnvelope = true;
      break;
    }
  EXPECT_FALSE(hasUseCustomEnvelope)
      << "PitchBend target should not show Use Custom ADSR (code ignores it)";
}

TEST(MappingDefinitionTest, ExpressionSmartScaleBendHidesCustomEnvelopeControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "SmartScaleBend", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasUseCustomEnvelope = false;
  for (const auto &c : schema)
    if (c.propertyId == "useCustomEnvelope") {
      hasUseCustomEnvelope = true;
      break;
    }
  EXPECT_FALSE(hasUseCustomEnvelope)
      << "SmartScaleBend target should not show Use Custom ADSR (code ignores it)";
}

// --- Expression: CC -> value when on/off (used for keyboard CC in Mappings tab) ---
TEST(MappingDefinitionTest, ExpressionCCHasValueWhenOnOff) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasValOn = false, hasValOff = false;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadValueWhenOn") hasValOn = true;
    if (c.propertyId == "touchpadValueWhenOff") hasValOff = true;
  }
  EXPECT_TRUE(hasValOn) << "CC Expression should have Value when On";
  EXPECT_TRUE(hasValOff) << "CC Expression should have Value when Off";
}

// --- Command: Sustain -> commandCategory and sustainStyle ---
TEST(MappingDefinitionTest, SustainCommandHasCategoryAndStyle) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::SustainMomentary, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  const InspectorControl *cmdCtrl = nullptr;
  const InspectorControl *styleCtrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "commandCategory") cmdCtrl = &c;
    if (c.propertyId == "sustainStyle") styleCtrl = &c;
  }
  ASSERT_NE(cmdCtrl, nullptr);
  EXPECT_EQ(cmdCtrl->label, "Command");
  ASSERT_NE(styleCtrl, nullptr);
  EXPECT_EQ(styleCtrl->options.size(), 3u)
      << "Sustain style: Hold, Toggle, Default on hold to not sustain";
}

// --- Command: Latch Toggle -> releaseLatchedOnToggleOff ---
TEST(MappingDefinitionTest, LatchToggleHasReleaseLatchedControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::LatchToggle, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasReleaseLatched = false;
  for (const auto &c : schema)
    if (c.propertyId == "releaseLatchedOnToggleOff") {
      hasReleaseLatched = true;
      break;
    }
  EXPECT_TRUE(hasReleaseLatched)
      << "Latch Toggle should have Release latched when toggling off";
}

// --- Command: Panic -> panicMode ---
TEST(MappingDefinitionTest, PanicCommandHasPanicMode) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::Panic, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  const InspectorControl *panicModeCtrl = nullptr;
  for (const auto &c : schema)
    if (c.propertyId == "panicMode") {
      panicModeCtrl = &c;
      break;
    }
  ASSERT_NE(panicModeCtrl, nullptr);
  EXPECT_EQ(panicModeCtrl->label, "Mode");
  EXPECT_NE(panicModeCtrl->options.find(1), panicModeCtrl->options.end());
  EXPECT_NE(panicModeCtrl->options.find(2), panicModeCtrl->options.end());
  EXPECT_NE(panicModeCtrl->options.find(3), panicModeCtrl->options.end());
}

// --- Command: Transpose -> mode, modify, semitones when Set, Local placeholder ---
TEST(MappingDefinitionTest, TransposeCommandHasModeModifySemitones) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::Transpose, nullptr);
  mapping.setProperty("transposeModify", 4, nullptr); // Set
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasMode = false, hasModify = false, hasSemitones = false;
  for (const auto &c : schema) {
    if (c.propertyId == "transposeMode") hasMode = true;
    if (c.propertyId == "transposeModify") hasModify = true;
    if (c.propertyId == "transposeSemitones") hasSemitones = true;
  }
  EXPECT_TRUE(hasMode) << "Transpose should have Mode (Global/Local)";
  EXPECT_TRUE(hasModify) << "Transpose should have Modify";
  EXPECT_TRUE(hasSemitones) << "Transpose Set should show Semitones slider";
}

TEST(MappingDefinitionTest, TransposeLocalShowsZonePlaceholder) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::Transpose, nullptr);
  mapping.setProperty("transposeMode", "Local", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasPlaceholder = false;
  for (const auto &c : schema)
    if (c.propertyId == "transposeZonesPlaceholder") {
      hasPlaceholder = true;
      break;
    }
  EXPECT_TRUE(hasPlaceholder)
      << "Transpose Local should show Affected zones placeholder";
}

// --- Command: Global Mode Up/Down -> uses Global mode section with direction ---
TEST(MappingDefinitionTest, GlobalModeUpHasDirectionControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::GlobalModeUp, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasData2 = false;
  bool hasDirection = false;
  for (const auto &c : schema)
    if (c.propertyId == "data2")
      hasData2 = true;
    else if (c.propertyId == "globalModeDirection")
      hasDirection = true;
  EXPECT_FALSE(hasData2) << "Global Mode Up should not have Target Layer (data2)";
  EXPECT_TRUE(hasDirection) << "Global Mode Up should have Global mode direction control";
}

TEST(MappingDefinitionTest, GlobalModeDownHasDirectionControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::GlobalModeDown, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasData2 = false;
  bool hasDirection = false;
  for (const auto &c : schema)
    if (c.propertyId == "data2")
      hasData2 = true;
    else if (c.propertyId == "globalModeDirection")
      hasDirection = true;
  EXPECT_FALSE(hasData2) << "Global Mode Down should not have Target Layer (data2)";
  EXPECT_TRUE(hasDirection) << "Global Mode Down should have Global mode direction control";
}

// --- getTypeName for all ActionTypes ---
TEST(MappingDefinitionTest, GetTypeNameAllTypes) {
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Note), "Note");
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Expression), "Expression");
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Command), "Command");
}

// --- Centralized mapping defaults ---
TEST(MappingDefinitionTest, GetDefaultValueAdsrMatchesConstants) {
  juce::var attackDef = MappingDefinition::getDefaultValue("adsrAttack");
  juce::var decayDef = MappingDefinition::getDefaultValue("adsrDecay");
  juce::var sustainDef = MappingDefinition::getDefaultValue("adsrSustain");
  juce::var releaseDef = MappingDefinition::getDefaultValue("adsrRelease");
  ASSERT_FALSE(attackDef.isVoid());
  ASSERT_FALSE(decayDef.isVoid());
  ASSERT_FALSE(sustainDef.isVoid());
  ASSERT_FALSE(releaseDef.isVoid());
  EXPECT_EQ(static_cast<int>(attackDef), MappingDefaults::ADSRAttackMs);
  EXPECT_EQ(static_cast<int>(decayDef), MappingDefaults::ADSRDecayMs);
  EXPECT_DOUBLE_EQ(static_cast<double>(sustainDef), MappingDefaults::ADSRSustain);
  EXPECT_EQ(static_cast<int>(releaseDef), MappingDefaults::ADSRReleaseMs);
}

TEST(MappingDefinitionTest, ExpressionSchemaAdsrControlsHaveDefaultValue) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);
  mapping.setProperty("useCustomEnvelope", true, nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  for (const auto &c : schema) {
    if (c.propertyId == "adsrAttack" || c.propertyId == "adsrDecay" ||
        c.propertyId == "adsrSustain" || c.propertyId == "adsrRelease") {
      EXPECT_FALSE(c.defaultValue.isVoid())
          << "ADSR control " << c.propertyId.toStdString()
          << " should have defaultValue set";
    }
  }
}
