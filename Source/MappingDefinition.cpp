#include "MappingDefinition.h"

namespace {
const char *kTouchpadEventNames[] = {
    "Finger 1: Down",     "Finger 1: Up",      "Finger 1: X",
    "Finger 1: Y",        "Finger 2: Down",    "Finger 2: Up",
    "Finger 2: X",        "Finger 2: Y",       "Finger 1 & 2 dist",
    "Finger 1 & 2 avg X", "Finger 1 & 2 avg Y"};
}

juce::String MappingDefinition::getTouchpadEventName(int eventId) {
  if (eventId >= 0 && eventId < TouchpadEvent::Count)
    return juce::String(kTouchpadEventNames[eventId]);
  return "Unknown";
}

std::map<int, juce::String> MappingDefinition::getTouchpadEventOptions() {
  std::map<int, juce::String> m;
  for (int i = 0; i < TouchpadEvent::Count; ++i)
    m[i] = kTouchpadEventNames[i];
  return m;
}

juce::String MappingDefinition::getTypeName(ActionType type) {
  switch (type) {
  case ActionType::Note:
    return "Note";
  case ActionType::Expression:
    return "Expression";
  case ActionType::Command:
    return "Command";
  default:
    return "Unknown";
  }
}

std::map<int, juce::String> MappingDefinition::getCommandOptions() {
  using Cmd = OmniKey::CommandID;
  return {
      {static_cast<int>(Cmd::SustainMomentary), "Hold to sustain"},
      {static_cast<int>(Cmd::SustainToggle), "Toggle sustain"},
      {static_cast<int>(Cmd::SustainInverse),
       "Default is on. Hold to not sustain"},
      {static_cast<int>(Cmd::LatchToggle), "Latch Toggle"},
      {static_cast<int>(Cmd::Panic), "Panic"},
      {static_cast<int>(Cmd::Transpose), "Transpose"},
      {static_cast<int>(Cmd::GlobalModeUp), "Global Mode Up"},
      {static_cast<int>(Cmd::GlobalModeDown), "Global Mode Down"},
      {static_cast<int>(Cmd::LayerMomentary), "Layer Momentary"},
      {static_cast<int>(Cmd::LayerToggle), "Layer Toggle"},
  };
}

std::map<int, juce::String> MappingDefinition::getLayerOptions() {
  std::map<int, juce::String> m;
  m[0] = "Base";
  for (int i = 1; i <= 8; ++i)
    m[i] = "Layer " + juce::String(i);
  return m;
}

InspectorControl MappingDefinition::createSeparator(const juce::String &label,
                                                    juce::Justification align) {
  InspectorControl c;
  c.controlType = InspectorControl::Type::Separator;
  c.label = label;
  c.separatorAlign = align;
  return c;
}

static bool isTouchpadMapping(const juce::ValueTree &mapping) {
  return mapping.getProperty("inputAlias", "")
      .toString()
      .trim()
      .equalsIgnoreCase("Touchpad");
}

static bool isTouchpadEventBoolean(int eventId) {
  return (eventId == TouchpadEvent::Finger1Down ||
          eventId == TouchpadEvent::Finger1Up ||
          eventId == TouchpadEvent::Finger2Down ||
          eventId == TouchpadEvent::Finger2Up);
}

InspectorSchema MappingDefinition::getSchema(const juce::ValueTree &mapping,
                                             int pitchBendRange) {
  InspectorSchema schema;

  // Step 1: Common control – Type selector (ComboBox)
  InspectorControl typeCtrl;
  typeCtrl.propertyId = "type";
  typeCtrl.label = "Type";
  typeCtrl.controlType = InspectorControl::Type::ComboBox;
  typeCtrl.options[1] = "Note";
  typeCtrl.options[2] = "Expression";
  typeCtrl.options[3] = "Command";
  schema.push_back(typeCtrl);

  schema.push_back(createSeparator("", juce::Justification::centred));

  // Step 2: Branch on current type
  juce::String typeStr = mapping.getProperty("type", "Note").toString().trim();
  const bool touchpad = isTouchpadMapping(mapping);
  const int touchpadEvent =
      touchpad ? (int)mapping.getProperty("inputTouchpadEvent", 0) : 0;
  const bool touchpadInputBool =
      touchpad && isTouchpadEventBoolean(touchpadEvent);

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
    schema.push_back(
        createSeparator("Note Settings", juce::Justification::centredLeft));

    // Release Behavior: Send Note Off | Sustain until retrigger | Always Latch
    // For Finger 1 Up / Finger 2 Up there is no "release after" — don't offer
    // Send Note Off
    const bool touchpadUpEvent =
        touchpad && (touchpadEvent == TouchpadEvent::Finger1Up ||
                     touchpadEvent == TouchpadEvent::Finger2Up);
    InspectorControl releaseBehavior;
    releaseBehavior.propertyId = "releaseBehavior";
    releaseBehavior.label = "Release Behaviour";
    releaseBehavior.controlType = InspectorControl::Type::ComboBox;
    if (!touchpadUpEvent) {
      releaseBehavior.options[1] = "Send Note Off";
      releaseBehavior.options[2] = "Sustain until retrigger";
      releaseBehavior.options[3] = "Always Latch";
    } else {
      releaseBehavior.options[2] = "Sustain until retrigger";
      releaseBehavior.options[3] = "Always Latch";
    }
    schema.push_back(releaseBehavior);

    // Follow Global Transpose (default on)
    InspectorControl followTranspose;
    followTranspose.propertyId = "followTranspose";
    followTranspose.label = "Follow Global Transpose";
    followTranspose.controlType = InspectorControl::Type::Toggle;
    followTranspose.widthWeight = 0.5f;
    schema.push_back(followTranspose);

    if (touchpad && !touchpadInputBool) {
      schema.push_back(createSeparator("Touchpad: Continuous to Note",
                                       juce::Justification::centredLeft));
      InspectorControl thresh;
      thresh.propertyId = "touchpadThreshold";
      thresh.label = "Threshold";
      thresh.controlType = InspectorControl::Type::Slider;
      thresh.min = 0.0;
      thresh.max = 1.0;
      thresh.step = 0.01;
      schema.push_back(thresh);
      InspectorControl trigger;
      trigger.propertyId = "touchpadTriggerAbove";
      trigger.label = "Trigger";
      trigger.controlType = InspectorControl::Type::ComboBox;
      trigger.options[1] = "Below threshold";
      trigger.options[2] = "Above threshold";
      schema.push_back(trigger);
    }
  } else if (typeStr.equalsIgnoreCase("Expression")) {
    juce::String adsrTargetStr =
        mapping.getProperty("adsrTarget", "CC").toString().trim();
    bool useCustomEnvelope =
        (bool)mapping.getProperty("useCustomEnvelope", false);
    bool isPitchOrSmart = adsrTargetStr.equalsIgnoreCase("PitchBend") ||
                          adsrTargetStr.equalsIgnoreCase("SmartScaleBend");

    // 1. Basic Controls
    schema.push_back(
        createSeparator("Expression", juce::Justification::centredLeft));

    InspectorControl ch;
    ch.propertyId = "channel";
    ch.label = "Channel";
    ch.controlType = InspectorControl::Type::Slider;
    ch.min = 1.0;
    ch.max = 16.0;
    ch.step = 1.0;
    ch.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(ch);

    InspectorControl target;
    target.propertyId = "adsrTarget";
    target.label = "Target";
    target.controlType = InspectorControl::Type::ComboBox;
    target.options[1] = "CC";
    target.options[2] = "PitchBend";
    target.options[3] = "SmartScaleBend";
    schema.push_back(target);

    if (adsrTargetStr.equalsIgnoreCase("CC")) {
      InspectorControl data1;
      data1.propertyId = "data1";
      data1.label = "Controller";
      data1.controlType = InspectorControl::Type::Slider;
      data1.min = 0.0;
      data1.max = 127.0;
      data1.step = 1.0;
      schema.push_back(data1);
    }
    if (adsrTargetStr.equalsIgnoreCase("PitchBend")) {
      InspectorControl bend;
      bend.propertyId = "data2";
      bend.label = "Bend (semitones)";
      bend.controlType = InspectorControl::Type::Slider;
      bend.min = -static_cast<double>(pitchBendRange);
      bend.max = static_cast<double>(pitchBendRange);
      bend.step = 1.0;
      bend.valueScaleRange = pitchBendRange;
      schema.push_back(bend);
    }
    if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend")) {
      InspectorControl steps;
      steps.propertyId = "smartStepShift";
      steps.label = "Scale Steps";
      steps.controlType = InspectorControl::Type::Slider;
      steps.min = -12.0;
      steps.max = 12.0;
      steps.step = 1.0;
      schema.push_back(steps);
    }

    // Value when On / Value when Off: CC only. PitchBend/SmartScaleBend use
    // Bend (semitones) / Scale Steps and return to neutral.
    if (adsrTargetStr.equalsIgnoreCase("CC") &&
        !(touchpad && touchpadInputBool)) {
      schema.push_back(createSeparator("Expression: Value when On / Off",
                                       juce::Justification::centredLeft));
      InspectorControl valOn;
      valOn.propertyId = "touchpadValueWhenOn";
      valOn.label = "Value when On";
      valOn.controlType = InspectorControl::Type::Slider;
      valOn.min = 0.0;
      valOn.max = 127.0;
      valOn.step = 1.0;
      schema.push_back(valOn);
      InspectorControl valOff;
      valOff.propertyId = "touchpadValueWhenOff";
      valOff.label = "Value when Off";
      valOff.controlType = InspectorControl::Type::Slider;
      valOff.min = 0.0;
      valOff.max = 127.0;
      valOff.step = 1.0;
      schema.push_back(valOff);
    }

    // 2. Mode Selection
    schema.push_back(
        createSeparator("Dynamics", juce::Justification::centredLeft));
    InspectorControl useEnv;
    useEnv.propertyId = "useCustomEnvelope";
    useEnv.label = "Use Custom ADSR Curve";
    useEnv.controlType = InspectorControl::Type::Toggle;
    useEnv.widthWeight = 1.0f;
    schema.push_back(useEnv);

    // 3. Conditional Branch
    if (useCustomEnvelope) {
      auto addAdsrSlider = [&schema](const juce::String &propId,
                                     const juce::String &lbl, double maxVal) {
        InspectorControl c;
        c.propertyId = propId;
        c.label = lbl;
        c.controlType = InspectorControl::Type::Slider;
        c.min = 0.0;
        c.max = maxVal;
        c.step = (maxVal <= 1.0) ? 0.01 : 1.0;
        schema.push_back(c);
      };
      addAdsrSlider("adsrAttack", "Attack (ms)", 5000.0);
      addAdsrSlider("adsrDecay", "Decay (ms)", 5000.0);
      addAdsrSlider("adsrSustain", "Sustain (0-1)", 1.0);
      addAdsrSlider("adsrRelease", "Release (ms)", 5000.0);
    } else {
      InspectorControl sendRel;
      sendRel.propertyId = "sendReleaseValue";
      sendRel.label =
          isPitchOrSmart ? "Reset pitch on release" : "Send Value on Release";
      sendRel.controlType = InspectorControl::Type::Toggle;
      sendRel.widthWeight = isPitchOrSmart ? 1.0f : 0.45f;
      sendRel.sameLine = false;
      schema.push_back(sendRel);

      // Release value slider only for CC; for PitchBend/SmartScaleBend release
      // is always 8192
      if (!isPitchOrSmart) {
        InspectorControl relVal;
        relVal.propertyId = "releaseValue";
        relVal.label = "";
        relVal.controlType = InspectorControl::Type::Slider;
        relVal.min = 0.0;
        relVal.max = 127.0;
        relVal.step = 1.0;
        relVal.sameLine = true;
        relVal.widthWeight = 0.55f;
        relVal.enabledConditionProperty = "sendReleaseValue";
        schema.push_back(relVal);
      }
    }

    // Touchpad boolean: Value when On/Off only for CC. PitchBend/SmartScaleBend
    // use Bend (semitones) / Scale Steps and return to neutral (no value
    // sliders).
    if (touchpad && touchpadInputBool && adsrTargetStr.equalsIgnoreCase("CC")) {
      schema.push_back(createSeparator("Touchpad: Boolean to Expression",
                                       juce::Justification::centredLeft));
      InspectorControl valOn;
      valOn.propertyId = "touchpadValueWhenOn";
      valOn.label = "Value when On";
      valOn.controlType = InspectorControl::Type::Slider;
      valOn.min = 0.0;
      valOn.max = 127.0;
      valOn.step = 1.0;
      schema.push_back(valOn);
      InspectorControl valOff;
      valOff.propertyId = "touchpadValueWhenOff";
      valOff.label = "Value when Off";
      valOff.controlType = InspectorControl::Type::Slider;
      valOff.min = 0.0;
      valOff.max = 127.0;
      valOff.step = 1.0;
      schema.push_back(valOff);
    }
    if (touchpad && !touchpadInputBool) {
      schema.push_back(
          createSeparator("Touchpad: Range", juce::Justification::centredLeft));
      InspectorControl inMin;
      inMin.propertyId = "touchpadInputMin";
      inMin.label = "Input min";
      inMin.controlType = InspectorControl::Type::Slider;
      inMin.min = 0.0;
      inMin.max = 1.0;
      inMin.step = 0.01;
      inMin.widthWeight = 0.5f;
      inMin.sameLine = false;
      schema.push_back(inMin);
      InspectorControl inMax;
      inMax.propertyId = "touchpadInputMax";
      inMax.label = "Input max";
      inMax.controlType = InspectorControl::Type::Slider;
      inMax.min = 0.0;
      inMax.max = 1.0;
      inMax.step = 0.01;
      inMax.widthWeight = 0.5f;
      inMax.sameLine = true;
      schema.push_back(inMax);
      const bool rangeIsSteps =
          adsrTargetStr.equalsIgnoreCase("PitchBend") ||
          adsrTargetStr.equalsIgnoreCase("SmartScaleBend");
      const double stepMax = rangeIsSteps ? 12.0 : 127.0;
      const double stepMin = rangeIsSteps ? -12.0 : 0.0;
      juce::String outLabel =
          rangeIsSteps ? "Output min (steps)" : "Output min";
      InspectorControl outMin;
      outMin.propertyId = "touchpadOutputMin";
      outMin.label = outLabel;
      outMin.controlType = InspectorControl::Type::Slider;
      outMin.min = stepMin;
      outMin.max = stepMax;
      outMin.step = 1.0;
      outMin.widthWeight = 0.5f;
      outMin.sameLine = false;
      schema.push_back(outMin);
      InspectorControl outMax;
      outMax.propertyId = "touchpadOutputMax";
      outMax.label = rangeIsSteps ? "Output max (steps)" : "Output max";
      outMax.controlType = InspectorControl::Type::Slider;
      outMax.min = stepMin;
      outMax.max = stepMax;
      outMax.step = 1.0;
      outMax.widthWeight = 0.5f;
      outMax.sameLine = true;
      schema.push_back(outMax);
    }
  } else if (typeStr.equalsIgnoreCase("Command")) {
    // Sustain: one control with Style dropdown when Sustain; Layer: same
    static constexpr int kSustainCategoryId = 100;
    static constexpr int kLayerCategoryId = 110;
    int cmdId = (int)mapping.getProperty("data1", 0);
    const bool isSustain = (cmdId >= 0 && cmdId <= 2);
    const bool isLayer = (cmdId == 10 || cmdId == 11);

    InspectorControl cmdCtrl;
    cmdCtrl.propertyId = (isSustain || isLayer) ? "commandCategory" : "data1";
    cmdCtrl.label = isSustain ? "Sustain" : (isLayer ? "Layer" : "Command");
    cmdCtrl.controlType = InspectorControl::Type::ComboBox;
    cmdCtrl.options[kSustainCategoryId] = "Sustain";
    cmdCtrl.options[3] = "Latch Toggle";
    cmdCtrl.options[4] = "Panic";
    cmdCtrl.options[6] = "Transpose";
    cmdCtrl.options[8] = "Global Mode Up";
    cmdCtrl.options[9] = "Global Mode Down";
    cmdCtrl.options[kLayerCategoryId] = "Layer";
    schema.push_back(cmdCtrl);

    if (isSustain) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "sustainStyle"; // Virtual: maps to data1 (0,1,2)
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to sustain";
      styleCtrl.options[2] = "Toggle sustain";
      styleCtrl.options[3] = "Default is on. Hold to not sustain";
      schema.push_back(styleCtrl);
    }

    if (isLayer) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "layerStyle"; // Virtual: maps to data1 (10,11)
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to switch";
      styleCtrl.options[2] = "Toggle layer";
      schema.push_back(styleCtrl);
    }

    cmdId = (int)mapping.getProperty("data1", 0);
    const int layerMomentary =
        static_cast<int>(OmniKey::CommandID::LayerMomentary);
    const int layerToggle = static_cast<int>(OmniKey::CommandID::LayerToggle);

    if (isLayer || cmdId == layerMomentary || cmdId == layerToggle) {
      InspectorControl data2;
      data2.propertyId = "data2";
      data2.label = "Target Layer";
      data2.controlType = InspectorControl::Type::ComboBox;
      data2.options = getLayerOptions();
      schema.push_back(data2);
    }
    const int latchToggle = static_cast<int>(OmniKey::CommandID::LatchToggle);
    if (cmdId == latchToggle) {
      InspectorControl releaseLatched;
      releaseLatched.propertyId = "releaseLatchedOnToggleOff";
      releaseLatched.label = "Release latched when toggling off";
      releaseLatched.controlType = InspectorControl::Type::Toggle;
      schema.push_back(releaseLatched);
    }
    const int panic = static_cast<int>(OmniKey::CommandID::Panic);
    const int panicLatch = static_cast<int>(OmniKey::CommandID::PanicLatch);
    if (cmdId == panic || cmdId == panicLatch) {
      InspectorControl panicMode;
      panicMode.propertyId = "panicMode"; // Virtual: maps to data1=4, data2
      panicMode.label = "Mode";
      panicMode.controlType = InspectorControl::Type::ComboBox;
      panicMode.options[1] = "Panic all";
      panicMode.options[2] = "Panic latched only";
      panicMode.options[3] = "Panic chords";
      schema.push_back(panicMode);
    }
    const int transpose = static_cast<int>(OmniKey::CommandID::Transpose);
    const int globalPitchDown =
        static_cast<int>(OmniKey::CommandID::GlobalPitchDown);
    if (cmdId == transpose || cmdId == globalPitchDown) {
      schema.push_back(
          createSeparator("Transpose", juce::Justification::centredLeft));
      InspectorControl modeCtrl;
      modeCtrl.propertyId = "transposeMode"; // "global" / "local"
      modeCtrl.label = "Mode";
      modeCtrl.controlType = InspectorControl::Type::ComboBox;
      modeCtrl.options[1] = "Global";
      modeCtrl.options[2] = "Local";
      schema.push_back(modeCtrl);

      InspectorControl modifyCtrl;
      modifyCtrl.propertyId = "transposeModify"; // 0-4, use 1-based for combo
      modifyCtrl.label = "Modify";
      modifyCtrl.controlType = InspectorControl::Type::ComboBox;
      modifyCtrl.options[1] = "Up (1 semitone)";
      modifyCtrl.options[2] = "Down (1 semitone)";
      modifyCtrl.options[3] = "Up (1 octave)";
      modifyCtrl.options[4] = "Down (1 octave)";
      modifyCtrl.options[5] = "Set (specific semitones)";
      schema.push_back(modifyCtrl);

      int modifyVal = (int)mapping.getProperty("transposeModify", 0);
      if (modifyVal == 4) { // Set
        InspectorControl semitonesCtrl;
        semitonesCtrl.propertyId = "transposeSemitones";
        semitonesCtrl.label = "Semitones";
        semitonesCtrl.controlType = InspectorControl::Type::Slider;
        semitonesCtrl.min = -48.0;
        semitonesCtrl.max = 48.0;
        semitonesCtrl.step = 1.0;
        semitonesCtrl.valueFormat = InspectorControl::Format::Integer;
        schema.push_back(semitonesCtrl);
      }

      juce::String modeStr =
          mapping.getProperty("transposeMode", "Global").toString();
      if (modeStr.equalsIgnoreCase("Local")) {
        InspectorControl zonePlaceholder;
        zonePlaceholder.propertyId = "transposeZonesPlaceholder";
        zonePlaceholder.label = "Affected zones";
        zonePlaceholder.controlType = InspectorControl::Type::LabelOnly;
        schema.push_back(zonePlaceholder);
        InspectorControl zoneNote;
        zoneNote.propertyId = "";
        zoneNote.label = "(Zone selector – coming soon)";
        zoneNote.controlType = InspectorControl::Type::LabelOnly;
        schema.push_back(zoneNote);
      }
    }
  }

  return schema;
}
