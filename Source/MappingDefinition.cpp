#include "MappingDefinition.h"

juce::String MappingDefinition::getTypeName(ActionType type) {
  switch (type) {
  case ActionType::Note:
    return "Note";
  case ActionType::CC:
    return "CC";
  case ActionType::Command:
    return "Command";
  case ActionType::Macro:
    return "Macro";
  case ActionType::Envelope:
    return "Envelope";
  default:
    return "Unknown";
  }
}

std::map<int, juce::String> MappingDefinition::getCommandOptions() {
  using Cmd = OmniKey::CommandID;
  return {
      {static_cast<int>(Cmd::SustainMomentary), "Sustain Momentary"},
      {static_cast<int>(Cmd::SustainToggle), "Sustain Toggle"},
      {static_cast<int>(Cmd::SustainInverse), "Sustain Inverse"},
      {static_cast<int>(Cmd::LatchToggle), "Latch Toggle"},
      {static_cast<int>(Cmd::Panic), "Panic"},
      {static_cast<int>(Cmd::PanicLatch), "Panic Latch"},
      {static_cast<int>(Cmd::GlobalPitchUp), "Global Pitch Up"},
      {static_cast<int>(Cmd::GlobalPitchDown), "Global Pitch Down"},
      {static_cast<int>(Cmd::GlobalModeUp), "Global Mode Up"},
      {static_cast<int>(Cmd::GlobalModeDown), "Global Mode Down"},
      {static_cast<int>(Cmd::LayerMomentary), "Layer Momentary"},
      {static_cast<int>(Cmd::LayerToggle), "Layer Toggle"},
      {static_cast<int>(Cmd::LayerSolo), "Layer Solo"},
  };
}

std::map<int, juce::String> MappingDefinition::getLayerOptions() {
  std::map<int, juce::String> m;
  m[0] = "Base";
  for (int i = 1; i <= 8; ++i)
    m[i] = "Layer " + juce::String(i);
  return m;
}

InspectorControl MappingDefinition::createSeparator(
    const juce::String &label,
    juce::Justification align) {
  InspectorControl c;
  c.controlType = InspectorControl::Type::Separator;
  c.label = label;
  c.separatorAlign = align;
  return c;
}

InspectorSchema MappingDefinition::getSchema(const juce::ValueTree &mapping) {
  InspectorSchema schema;

  // Step 1: Common control – Type selector (ComboBox)
  InspectorControl typeCtrl;
  typeCtrl.propertyId = "type";
  typeCtrl.label = "Type";
  typeCtrl.controlType = InspectorControl::Type::ComboBox;
  typeCtrl.options[1] = "Note";
  typeCtrl.options[2] = "CC";
  typeCtrl.options[3] = "Command";
  typeCtrl.options[4] = "Envelope";
  schema.push_back(typeCtrl);

  // Step 2: Branch on current type
  juce::String typeStr =
      mapping.getProperty("type", "Note").toString().trim();

  if (typeStr.equalsIgnoreCase("Note")) {
    // Channel: Slider 1–16
    InspectorControl ch;
    ch.propertyId = "channel";
    ch.label = "Channel";
    ch.controlType = InspectorControl::Type::Slider;
    ch.min = 1.0;
    ch.max = 16.0;
    ch.step = 1.0;
    ch.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(ch);

    // Data1 (Pitch): Slider 0–127, Label "Note", Format NoteName
    InspectorControl data1;
    data1.propertyId = "data1";
    data1.label = "Note";
    data1.controlType = InspectorControl::Type::Slider;
    data1.min = 0.0;
    data1.max = 127.0;
    data1.step = 1.0;
    data1.valueFormat = InspectorControl::Format::NoteName;
    schema.push_back(data1);

    // Data2 (Velocity): Slider 0–127
    InspectorControl data2;
    data2.propertyId = "data2";
    data2.label = "Velocity";
    data2.controlType = InspectorControl::Type::Slider;
    data2.min = 0.0;
    data2.max = 127.0;
    data2.step = 1.0;
    data2.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(data2);

    // Vel Random +/-: Slider 0–64
    InspectorControl velRand;
    velRand.propertyId = "velRandom";
    velRand.label = "Vel Random +/-";
    velRand.controlType = InspectorControl::Type::Slider;
    velRand.min = 0.0;
    velRand.max = 64.0;
    velRand.step = 1.0;
    velRand.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(velRand);

    // Phase 55.9: Explicit separator before Note Settings
    schema.push_back(createSeparator("Note Settings",
                                    juce::Justification::centredLeft));

    // Follow Global Transpose (default false = do not follow)
    InspectorControl followTranspose;
    followTranspose.propertyId = "followTranspose";
    followTranspose.label = "Follow Global Transpose";
    followTranspose.controlType = InspectorControl::Type::Toggle;
    followTranspose.widthWeight = 0.5f;
    schema.push_back(followTranspose);

    // Send Note Off on Release (default true) – same line as above
    InspectorControl sendNoteOff;
    sendNoteOff.propertyId = "sendNoteOff";
    sendNoteOff.label = "Send Note Off on Release";
    sendNoteOff.controlType = InspectorControl::Type::Toggle;
    sendNoteOff.sameLine = true;
    sendNoteOff.widthWeight = 0.5f;
    schema.push_back(sendNoteOff);
  } else if (typeStr.equalsIgnoreCase("CC")) {
    // Channel: Slider 1–16
    InspectorControl ch;
    ch.propertyId = "channel";
    ch.label = "Channel";
    ch.controlType = InspectorControl::Type::Slider;
    ch.min = 1.0;
    ch.max = 16.0;
    ch.step = 1.0;
    ch.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(ch);

    InspectorControl data1;
    data1.propertyId = "data1";
    data1.label = "CC Number";
    data1.controlType = InspectorControl::Type::Slider;
    data1.min = 0.0;
    data1.max = 127.0;
    data1.step = 1.0;
    schema.push_back(data1);

    InspectorControl data2;
    data2.propertyId = "data2";
    data2.label = "Press Value";
    data2.controlType = InspectorControl::Type::Slider;
    data2.min = 0.0;
    data2.max = 127.0;
    data2.step = 1.0;
    schema.push_back(data2);

    // Phase 55.9: Line-only separator before Release Mode
    schema.push_back(createSeparator());

    InspectorControl sendRel;
    sendRel.propertyId = "sendReleaseValue";
    sendRel.label = "Send Value on Release";
    sendRel.controlType = InspectorControl::Type::Toggle;
    sendRel.autoWidth = true;   // Phase 55.10: fit to content
    sendRel.widthWeight = 0.0f; // ignored when autoWidth
    sendRel.sameLine = false;
    schema.push_back(sendRel);

    InspectorControl relVal;
    relVal.propertyId = "releaseValue";
    relVal.label = "";
    relVal.controlType = InspectorControl::Type::Slider;
    relVal.min = 0.0;
    relVal.max = 127.0;
    relVal.step = 1.0;
    relVal.sameLine = true;
    relVal.autoWidth = false;
    relVal.widthWeight = 1.0f; // Phase 55.10: take all remaining space
    relVal.enabledConditionProperty = "sendReleaseValue";
    schema.push_back(relVal);
  } else if (typeStr.equalsIgnoreCase("Command")) {
    InspectorControl data1;
    data1.propertyId = "data1";
    data1.label = "Command";
    data1.controlType = InspectorControl::Type::ComboBox;
    data1.options = getCommandOptions();
    schema.push_back(data1);

    int cmdId = (int)mapping.getProperty("data1", 0);
    const int layerMomentary = static_cast<int>(OmniKey::CommandID::LayerMomentary);
    const int layerToggle = static_cast<int>(OmniKey::CommandID::LayerToggle);
    const int layerSolo = static_cast<int>(OmniKey::CommandID::LayerSolo);
    const int globalPitchUp = static_cast<int>(OmniKey::CommandID::GlobalPitchUp);
    const int globalPitchDown = static_cast<int>(OmniKey::CommandID::GlobalPitchDown);
    const int globalModeUp = static_cast<int>(OmniKey::CommandID::GlobalModeUp);
    const int globalModeDown = static_cast<int>(OmniKey::CommandID::GlobalModeDown);
    const int panic = static_cast<int>(OmniKey::CommandID::Panic);
    const int panicLatch = static_cast<int>(OmniKey::CommandID::PanicLatch);

    // Data2: only for Layer Command (ComboBox) or default (Slider); omit for
    // Global Pitch/Mode and Panic
    if (cmdId == layerMomentary || cmdId == layerToggle || cmdId == layerSolo) {
      InspectorControl data2;
      data2.propertyId = "data2";
      data2.label = "Target Layer";
      data2.controlType = InspectorControl::Type::ComboBox;
      data2.options = getLayerOptions();
      schema.push_back(data2);
    } else if (cmdId != globalPitchUp && cmdId != globalPitchDown &&
               cmdId != globalModeUp && cmdId != globalModeDown &&
               cmdId != panic && cmdId != panicLatch) {
      InspectorControl data2;
      data2.propertyId = "data2";
      data2.label = "Data2";
      data2.controlType = InspectorControl::Type::Slider;
      data2.min = 0.0;
      data2.max = 127.0;
      data2.step = 1.0;
      schema.push_back(data2);
    }
  } else if (typeStr.equalsIgnoreCase("Envelope")) {
    InspectorControl ch;
    ch.propertyId = "channel";
    ch.label = "Channel";
    ch.controlType = InspectorControl::Type::Slider;
    ch.min = 1.0;
    ch.max = 16.0;
    ch.step = 1.0;
    schema.push_back(ch);

    InspectorControl target;
    target.propertyId = "adsrTarget";
    target.label = "Target";
    target.controlType = InspectorControl::Type::ComboBox;
    target.options[1] = "CC";
    target.options[2] = "PitchBend";
    target.options[3] = "SmartScaleBend";
    schema.push_back(target);

    juce::String adsrTargetStr =
        mapping.getProperty("adsrTarget", "CC").toString().trim();
    bool isPitchOrSmart =
        adsrTargetStr.equalsIgnoreCase("PitchBend") ||
        adsrTargetStr.equalsIgnoreCase("SmartScaleBend");

    if (adsrTargetStr.equalsIgnoreCase("CC")) {
      InspectorControl data1;
      data1.propertyId = "data1";
      data1.label = "CC Number";
      data1.controlType = InspectorControl::Type::Slider;
      data1.min = 0.0;
      data1.max = 127.0;
      data1.step = 1.0;
      schema.push_back(data1);
    }

    InspectorControl data2;
    data2.propertyId = "data2";
    data2.label = "Peak Value";
    data2.controlType = InspectorControl::Type::Slider;
    data2.min = 0.0;
    data2.max = isPitchOrSmart ? 16383.0 : 127.0;
    data2.step = 1.0;
    schema.push_back(data2);

    auto addAdsrSlider = [&schema](const juce::String &propId,
                                   const juce::String &lbl, double maxVal) {
      InspectorControl c;
      c.propertyId = propId;
      c.label = lbl;
      c.controlType = InspectorControl::Type::Slider;
      c.min = 0.0;
      c.max = maxVal;
      c.step = 1.0;
      schema.push_back(c);
    };
    // Phase 55.9: Line-only separator before ADSR sliders
    schema.push_back(createSeparator());
    addAdsrSlider("adsrAttack", "Attack (ms)", 5000.0);
    addAdsrSlider("adsrDecay", "Decay (ms)", 5000.0);
    addAdsrSlider("adsrSustain", "Sustain (0-1)", 1.0);
    addAdsrSlider("adsrRelease", "Release (ms)", 5000.0);
  }

  return schema;
}
