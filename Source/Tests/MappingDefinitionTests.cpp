#include "../MappingDefinition.h"
#include "../MappingTypes.h"
#include <gtest/gtest.h>

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

// Phase 55.2 / 56.1: Expression with custom envelope, PitchBend -> data2 (Peak) has max 16383
TEST(MappingDefinitionTest, EnvelopeContext) {
  juce::ValueTree mapping("Mapping");
  mapping.setProperty("type", "Expression", nullptr);
  mapping.setProperty("useCustomEnvelope", true, nullptr);
  mapping.setProperty("adsrTarget", "PitchBend", nullptr);

  InspectorSchema schema = MappingDefinition::getSchema(mapping);

  const InspectorControl *data2Control = nullptr;
  for (const auto &c : schema) {
    if (c.propertyId == "data2") {
      data2Control = &c;
      break;
    }
  }
  ASSERT_NE(data2Control, nullptr) << "Schema should have a 'data2' control";
  EXPECT_DOUBLE_EQ(data2Control->max, 16383.0)
      << "Envelope PitchBend data2 (Peak) should have max 16383";
}

// Phase 55.7 / 56.1: Expression (simple) with compact dependent layout - checkbox + slider
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
  ASSERT_NE(sendRelControl, nullptr) << "Schema should have 'sendReleaseValue' control";
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
  ASSERT_NE(releaseValControl, nullptr) << "Schema should have 'releaseValue' control";
  EXPECT_EQ(releaseValControl->controlType, InspectorControl::Type::Slider);
  EXPECT_TRUE(releaseValControl->label.isEmpty()) << "Slider should have empty label for compact layout";
  EXPECT_TRUE(releaseValControl->sameLine) << "Slider should be on same line as toggle";
  EXPECT_FLOAT_EQ(releaseValControl->widthWeight, 0.55f)
      << "Slider should have 0.55 width weight (Phase 56.2)";
  EXPECT_EQ(releaseValControl->enabledConditionProperty, "sendReleaseValue") 
      << "Slider should be dependent on sendReleaseValue property";
}
