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
  mapping.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary,
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
    EXPECT_EQ(cmdCtrl->label, "Layer");
    ASSERT_NE(layerStyleCtrl, nullptr)
        << "Layer command should have layerStyle (Style dropdown)";
    EXPECT_EQ(layerStyleCtrl->options.size(), 2u)
        << "layerStyle should have Hold to switch, Toggle layer";
    ASSERT_NE(data2Ctrl, nullptr) << "Layer command should have Target Layer";
  }
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
