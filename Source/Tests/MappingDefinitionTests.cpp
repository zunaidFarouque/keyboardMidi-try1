#include "../MappingDefinition.h"
#include "../MappingTypes.h"
#include <gtest/gtest.h>

// --- Touchpad event names and options (recent touchpad mapping feature) ---
TEST(MappingDefinitionTest, TouchpadEventNames) {
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(0), "Finger 1: Down");
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(1), "Finger 1: Up");
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(2), "Finger 1: X");
  EXPECT_EQ(
      MappingDefinition::getTouchpadEventName(TouchpadEvent::Finger1And2Dist),
      "Finger 1 & 2 dist");
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(TouchpadEvent::Count - 1),
            "Finger 1 & 2 avg Y");
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(-1), "Unknown");
  EXPECT_EQ(MappingDefinition::getTouchpadEventName(TouchpadEvent::Count),
            "Unknown");
}

TEST(MappingDefinitionTest, TouchpadEventOptions) {
  auto options = MappingDefinition::getTouchpadEventOptions();
  EXPECT_EQ(options.size(), static_cast<size_t>(TouchpadEvent::Count));
  EXPECT_EQ(options[0], "Finger 1: Down");
  EXPECT_EQ(options[TouchpadEvent::Finger2Up], "Finger 2: Up");
  EXPECT_EQ(options[TouchpadEvent::Finger1And2AvgY], "Finger 1 & 2 avg Y");
}

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

// Release "Send Note Off" must not be offered for Finger 1 Up / Finger 2 Up
TEST(MappingDefinitionTest, TouchpadFingerUpSchemaNoSendNoteOffOption) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1Up, nullptr);
  mapping.setProperty("type", "Note", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *releaseCtrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "releaseBehavior") {
      releaseCtrl = &c;
      break;
    }
  }
  ASSERT_NE(releaseCtrl, nullptr);
  // Option 1 (Send Note Off) must not be present for Up events
  auto it = releaseCtrl->options.find(1);
  EXPECT_TRUE(it == releaseCtrl->options.end())
      << "Finger 1 Up must not have 'Send Note Off' option (id 1)";
  EXPECT_NE(releaseCtrl->options.find(2), releaseCtrl->options.end())
      << "Sustain until retrigger (2) should be available";
  EXPECT_NE(releaseCtrl->options.find(3), releaseCtrl->options.end())
      << "Always Latch (3) should be available";
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
TEST(MappingDefinitionTest, TouchpadSoloCommandHasCategoryScopeTypeAndGroup) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty(
      "data1",
      (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary,
      nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *cmdCtrl = nullptr;
  const InspectorControl *scopeCtrl = nullptr;
  const InspectorControl *typeCtrl = nullptr;
  const InspectorControl *groupCtrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "commandCategory")
      cmdCtrl = &c;
    else if (c.propertyId == "touchpadSoloScope")
      scopeCtrl = &c;
    else if (c.propertyId == "touchpadSoloType")
      typeCtrl = &c;
    else if (c.propertyId == "touchpadLayoutGroupId")
      groupCtrl = &c;
  }

  ASSERT_NE(cmdCtrl, nullptr);
  EXPECT_EQ(cmdCtrl->label, "Command");
  ASSERT_NE(scopeCtrl, nullptr);
  ASSERT_NE(typeCtrl, nullptr);
  ASSERT_NE(groupCtrl, nullptr);
}

// Verify touchpadSoloType has correct options
TEST(MappingDefinitionTest, TouchpadSoloTypeHasCorrectOptions) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty(
      "data1",
      (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary,
      nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *typeCtrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadSoloType") {
      typeCtrl = &c;
      break;
    }
  }

  ASSERT_NE(typeCtrl, nullptr);
  EXPECT_EQ(typeCtrl->label, "Solo type");
  EXPECT_EQ(typeCtrl->controlType, InspectorControl::Type::ComboBox);
  EXPECT_EQ(typeCtrl->options.size(), 4u) << "Should have 4 solo type options";
  auto it1 = typeCtrl->options.find(1);
  ASSERT_NE(it1, typeCtrl->options.end());
  EXPECT_EQ(it1->second, "Hold");
  auto it2 = typeCtrl->options.find(2);
  ASSERT_NE(it2, typeCtrl->options.end());
  EXPECT_EQ(it2->second, "Toggle");
  auto it3 = typeCtrl->options.find(3);
  ASSERT_NE(it3, typeCtrl->options.end());
  EXPECT_EQ(it3->second, "Set");
  auto it4 = typeCtrl->options.find(4);
  ASSERT_NE(it4, typeCtrl->options.end());
  EXPECT_EQ(it4->second, "Clear");
}

// Verify touchpad solo controls appear for all touchpad solo command IDs
TEST(MappingDefinitionTest, TouchpadSoloControlsAppearForAllSoloTypes) {
  using MIDIQy::CommandID;
  std::vector<CommandID> soloCommands = {
      CommandID::TouchpadLayoutGroupSoloMomentary,
      CommandID::TouchpadLayoutGroupSoloToggle,
      CommandID::TouchpadLayoutGroupSoloSet,
      CommandID::TouchpadLayoutGroupSoloClear};

  for (auto cmd : soloCommands) {
    juce::ValueTree mapping("Mapping");
    mapping.setProperty("type", "Command", nullptr);
    mapping.setProperty("data1", static_cast<int>(cmd), nullptr);

    InspectorSchema schema = MappingDefinition::getSchema(mapping);

    bool hasSoloType = false;
    bool hasSoloScope = false;
    bool hasLayoutGroup = false;
    for (const auto &c : schema) {
      if (c.propertyId == "touchpadSoloType")
        hasSoloType = true;
      else if (c.propertyId == "touchpadSoloScope")
        hasSoloScope = true;
      else if (c.propertyId == "touchpadLayoutGroupId")
        hasLayoutGroup = true;
    }

    EXPECT_TRUE(hasSoloType) << "Should have touchpadSoloType for command "
                              << static_cast<int>(cmd);
    EXPECT_TRUE(hasSoloScope) << "Should have touchpadSoloScope for command "
                               << static_cast<int>(cmd);
    EXPECT_TRUE(hasLayoutGroup)
        << "Should have touchpadLayoutGroupId for command "
        << static_cast<int>(cmd);
  }
}

// Command schema includes Touchpad solo block only when data1 is 18-21.
// Assert absence when Sustain (data1=0), presence when Touchpad (data1=18).
TEST(MappingDefinitionTest, SchemaWhenSwitchingToTouchpad_NoControlsThenHasControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", 0, nullptr); // SustainMomentary, not touchpad

  InspectorSchema schemaBefore = MappingDefinition::getSchema(mapping);
  bool hasSoloTypeBefore = false, hasGroupBefore = false, hasScopeBefore = false;
  for (const auto &c : schemaBefore) {
    if (c.propertyId == "touchpadSoloType")
      hasSoloTypeBefore = true;
    else if (c.propertyId == "touchpadLayoutGroupId")
      hasGroupBefore = true;
    else if (c.propertyId == "touchpadSoloScope")
      hasScopeBefore = true;
  }
  EXPECT_FALSE(hasSoloTypeBefore) << "Touchpad solo controls should not be in schema when data1=0";
  EXPECT_FALSE(hasGroupBefore) << "Touchpad layout group should not be in schema when data1=0";
  EXPECT_FALSE(hasScopeBefore) << "Touchpad solo scope should not be in schema when data1=0";

  mapping.setProperty("data1",
                      (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary,
                      nullptr);
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
  EXPECT_TRUE(hasSoloType) << "Schema should have touchpadSoloType when data1=18";
  EXPECT_TRUE(hasGroup) << "Schema should have touchpadLayoutGroupId when data1=18";
  EXPECT_TRUE(hasScope) << "Schema should have touchpadSoloScope when data1=18";
}

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

TEST(MappingDefinitionTest,
     TouchpadContinuousPitchBendDisablesBendSemitonesSlider) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);

  const int pbRange = 12;
  InspectorSchema schema = MappingDefinition::getSchema(mapping, pbRange);

  bool hasData2 = false;
  for (const auto &c : schema) {
    if (c.propertyId == "data2") {
      hasData2 = true;
      break;
    }
  }
  EXPECT_FALSE(hasData2)
      << "Touchpad continuous PitchBend Expression should not show Bend "
         "(semitones) slider at all";
}

TEST(MappingDefinitionTest, TouchpadContinuousSmartScaleHidesScaleStepsSlider) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "SmartScaleBend", nullptr);
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping, 12);

  const InspectorControl *stepsCtrl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "smartStepShift") {
      stepsCtrl = &c;
      break;
    }
  }
  EXPECT_EQ(stepsCtrl, nullptr)
      << "Touchpad continuous SmartScaleBend should not show Scale Steps "
         "slider; range is defined by Output min/max (smart scale distance)";
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

// Phase 55.7 / 56.1: Expression (simple) with compact dependent layout -
// checkbox + slider
TEST(MappingDefinitionTest, CCCompactDependentLayout) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("useCustomEnvelope", false, nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  // Find sendReleaseValue toggle
  const InspectorControl *sendRelControl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "sendReleaseValue") {
      sendRelControl = &c;
      break;
    }
  }
  ASSERT_NE(sendRelControl, nullptr)
      << "Schema should have 'sendReleaseValue' control";
  EXPECT_EQ(sendRelControl->controlType, InspectorControl::Type::Toggle);
  EXPECT_FLOAT_EQ(sendRelControl->widthWeight, 0.45f)
      << "Toggle should have 0.45 width weight (Phase 56.2)";
  EXPECT_FALSE(sendRelControl->sameLine) << "Toggle should start a new row";

  // Find releaseValue slider
  const InspectorControl *releaseValControl = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "releaseValue") {
      releaseValControl = &c;
      break;
    }
  }
  ASSERT_NE(releaseValControl, nullptr)
      << "Schema should have 'releaseValue' control";
  EXPECT_EQ(releaseValControl->controlType, InspectorControl::Type::Slider);
  EXPECT_TRUE(releaseValControl->label.isEmpty())
      << "Slider should have empty label for compact layout";
  EXPECT_TRUE(releaseValControl->sameLine)
      << "Slider should be on same line as toggle";
  EXPECT_FLOAT_EQ(releaseValControl->widthWeight, 0.55f)
      << "Slider should have 0.55 width weight (Phase 56.2)";
  EXPECT_EQ(releaseValControl->enabledConditionProperty, "sendReleaseValue")
      << "Slider should be dependent on sendReleaseValue property";
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

// --- Touchpad continuous Note: threshold + trigger above/below ---
TEST(MappingDefinitionTest, TouchpadContinuousNoteHasThresholdAndTrigger) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  mapping.setProperty("type", "Note", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasThreshold = false;
  bool hasTrigger = false;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadThreshold")
      hasThreshold = true;
    if (c.propertyId == "touchpadTriggerAbove")
      hasTrigger = true;
  }
  EXPECT_TRUE(hasThreshold)
      << "Touchpad continuous Note should have threshold control";
  EXPECT_TRUE(hasTrigger)
      << "Touchpad continuous Note should have trigger above/below control";
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
TEST(MappingDefinitionTest, ExpressionPitchBendDisablesCustomEnvelopeControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  const InspectorControl *useEnvCtrl = nullptr;
  for (const auto &c : schema)
    if (c.propertyId == "useCustomEnvelope") {
      useEnvCtrl = &c;
      break;
    }
  ASSERT_NE(useEnvCtrl, nullptr);
  EXPECT_FALSE(useEnvCtrl->isEnabled)
      << "PitchBend target should disable Use Custom ADSR control";
}

TEST(MappingDefinitionTest, ExpressionSmartScaleBendDisablesCustomEnvelopeControl) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "SmartScaleBend", nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  const InspectorControl *useEnvCtrl = nullptr;
  for (const auto &c : schema)
    if (c.propertyId == "useCustomEnvelope") {
      useEnvCtrl = &c;
      break;
    }
  ASSERT_NE(useEnvCtrl, nullptr);
  EXPECT_FALSE(useEnvCtrl->isEnabled)
      << "SmartScaleBend target should disable Use Custom ADSR control";
}

// --- Expression: Touchpad boolean + CC -> value when on/off ---
TEST(MappingDefinitionTest, ExpressionTouchpadBooleanCCHasValueWhenOnOff) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "CC", nullptr);
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1Down, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasValOn = false, hasValOff = false;
  for (const auto &c : schema) {
    if (c.propertyId == "touchpadValueWhenOn") hasValOn = true;
    if (c.propertyId == "touchpadValueWhenOff") hasValOff = true;
  }
  EXPECT_TRUE(hasValOn) << "Touchpad boolean CC Expression should have Value when On";
  EXPECT_TRUE(hasValOff) << "Touchpad boolean CC Expression should have Value when Off";
}

// --- Expression: Touchpad continuous + pitch -> pitch pad and zero range warning ---
TEST(MappingDefinitionTest, TouchpadContinuousPitchBendHasPitchPadControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  mapping.setProperty("touchpadOutputMin", -2, nullptr);
  mapping.setProperty("touchpadOutputMax", 2, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasMode = false, hasResting = false;
  for (const auto &c : schema) {
    if (c.propertyId == "pitchPadMode") hasMode = true;
    if (c.propertyId == "pitchPadRestingPercent") hasResting = true;
  }
  EXPECT_TRUE(hasMode) << "Touchpad continuous PitchBend should have Pitch Pad Mode";
  EXPECT_TRUE(hasResting) << "Touchpad continuous PitchBend should have resting %";
}

TEST(MappingDefinitionTest, TouchpadPitchPadZeroRangeShowsWarning) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);
  mapping.setProperty("inputAlias", "Touchpad", nullptr);
  mapping.setProperty("inputTouchpadEvent", TouchpadEvent::Finger1X, nullptr);
  mapping.setProperty("touchpadOutputMin", 0, nullptr);
  mapping.setProperty("touchpadOutputMax", 0, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasWarning = false;
  for (const auto &c : schema)
    if (c.propertyId == "touchpadPitchPadWarning") {
      hasWarning = true;
      break;
    }
  EXPECT_TRUE(hasWarning)
      << "Zero effective pitch range should show warning label";
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

// --- Command: Global Mode Up/Down -> no extra controls (just Command dropdown) ---
TEST(MappingDefinitionTest, GlobalModeUpHasNoExtraControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::GlobalModeUp, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  // Should have commandCategory/data1 as "Command" and no data2, no style, etc.
  size_t controlCount = 0;
  for (const auto &c : schema)
    if (c.propertyId == "data1" || c.propertyId == "commandCategory")
      controlCount++;
  EXPECT_GE(controlCount, 1u);
  bool hasData2 = false;
  for (const auto &c : schema)
    if (c.propertyId == "data2") hasData2 = true;
  EXPECT_FALSE(hasData2) << "Global Mode Up should not have Target Layer (data2)";
}

TEST(MappingDefinitionTest, GlobalModeDownHasNoExtraControls) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Command", nullptr);
  mapping.setProperty("data1", (int)MIDIQy::CommandID::GlobalModeDown, nullptr);
  InspectorSchema schema = MappingDefinition::getSchema(mapping);
  bool hasData2 = false;
  for (const auto &c : schema)
    if (c.propertyId == "data2") hasData2 = true;
  EXPECT_FALSE(hasData2) << "Global Mode Down should not have Target Layer (data2)";
}

// --- getTypeName for all ActionTypes ---
TEST(MappingDefinitionTest, GetTypeNameAllTypes) {
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Note), "Note");
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Expression), "Expression");
  EXPECT_EQ(MappingDefinition::getTypeName(ActionType::Command), "Command");
}
