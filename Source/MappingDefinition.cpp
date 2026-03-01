#include "MappingDefinition.h"
#include "MappingDefaults.h"
#include <unordered_map>

namespace {
const std::unordered_map<juce::String, juce::var> &getDefaultsMap() {
  using namespace MappingDefaults;
  static std::unordered_map<juce::String, juce::var> map;
  if (map.empty()) {
    map["enabled"] = juce::var(Enabled);
    map["channel"] = juce::var(Channel);
    map["data1"] = juce::var(Data1);
    map["data2"] = juce::var(Data2);
    map["velRandom"] = juce::var(VelRandom);
    map["releaseBehavior"] = juce::var(ReleaseBehaviorSendNoteOff);
    map["ccReleaseBehavior"] = juce::var(CcReleaseBehaviorInstant);
    map["touchpadHoldBehavior"] = juce::var(TouchpadHoldBehaviorHold);
    map["adsrTarget"] = juce::var(AdsrTargetCC);
    map["adsrAttack"] = juce::var(ADSRAttackMs);
    map["adsrDecay"] = juce::var(ADSRDecayMs);
    map["adsrSustain"] = juce::var(ADSRSustain);
    map["adsrRelease"] = juce::var(ADSRReleaseMs);
    map["touchpadValueWhenOn"] = juce::var(TouchpadValueWhenOn);
    map["touchpadValueWhenOff"] = juce::var(TouchpadValueWhenOff);
    map["inputTouchpadEvent"] = juce::var(InputTouchpadEvent);
    map["releaseValue"] = juce::var(ReleaseValue);
    map["sendReleaseValue"] = juce::var(true); // Reset pitch on release: true by default for PitchBend/SmartScaleBend
    map["touchpadLayoutGroupId"] = juce::var(TouchpadLayoutGroupId);
    map["touchpadSoloScope"] = juce::var(TouchpadSoloScope);
    map["keyboardGroupId"] = juce::var(KeyboardGroupId);
    map["keyboardLayoutGroupId"] = juce::var(KeyboardLayoutGroupId);
    map["keyboardSoloScope"] = juce::var(KeyboardSoloScope);
    map["touchpadThreshold"] = juce::var(static_cast<double>(TouchpadThreshold));
    map["touchpadTriggerAbove"] = juce::var(TouchpadTriggerAbove);
    map["touchpadInputMin"] = juce::var(static_cast<double>(TouchpadInputMin));
    map["touchpadInputMax"] = juce::var(static_cast<double>(TouchpadInputMax));
    map["touchpadOutputMin"] = juce::var(TouchpadOutputMin);
    map["touchpadOutputMax"] = juce::var(TouchpadOutputMax);
    map["pitchPadCustomStart"] = juce::var(static_cast<double>(PitchPadCustomStart));
    map["pitchPadRestingPercent"] = juce::var(static_cast<double>(PitchPadRestingPercent));
    map["pitchPadRestZonePercent"] = juce::var(static_cast<double>(PitchPadRestZonePercent));
    map["pitchPadTransitionZonePercent"] = juce::var(static_cast<double>(PitchPadTransitionZonePercent));
    map["pitchPadMode"] = juce::var("Absolute");
    map["pitchPadStart"] = juce::var("Center");
    map["pitchPadUseCustomRange"] = juce::var(false);
    map["pitchPadTouchGlideMs"] = juce::var(PitchPadTouchGlideMs);
    map["transposeModify"] = juce::var(TransposeModify);
    map["transposeSemitones"] = juce::var(TransposeSemitones);
    map["rootModify"] = juce::var(RootModify);
    map["rootNote"] = juce::var(RootNote);
    map["scaleModify"] = juce::var(ScaleModify);
    map["scaleIndex"] = juce::var(ScaleIndex);
    map["smartStepShift"] = juce::var(SmartStepShift);
    map["smartScaleFollowGlobal"] = juce::var(SmartScaleFollowGlobal);
    map["smartScaleName"] = juce::var(SmartScaleName);
    map["inputKey"] = juce::var(InputKey);
    map["expressionCCMode"] = juce::var(ExpressionCCModePosition);
    map["slideQuickPrecision"] = juce::var(SlideQuickPrecision);
    map["slideAbsRel"] = juce::var(SlideAbsRel);
    map["slideLockFree"] = juce::var(SlideLockFree);
    map["slideAxis"] = juce::var(SlideAxis);
    map["slideReturnOnRelease"] = juce::var(SlideReturnOnRelease);
    map["slideRestValue"] = juce::var(SlideRestValue);
    map["slideReturnGlideMs"] = juce::var(SlideReturnGlideMs);
    map["encoderAxis"] = juce::var(EncoderAxis);
    map["encoderSensitivity"] = juce::var(static_cast<double>(EncoderSensitivity));
    map["encoderStepSize"] = juce::var(EncoderStepSize);
    map["encoderStepSizeX"] = juce::var(EncoderStepSizeX);
    map["encoderStepSizeY"] = juce::var(EncoderStepSizeY);
    map["encoderOutputMode"] = juce::var(EncoderOutputModeAbsoluteStr);
    map["encoderRelativeEncoding"] = juce::var(EncoderRelativeEncoding);
    map["encoderWrap"] = juce::var(EncoderWrap);
    map["encoderInitialValue"] = juce::var(EncoderInitialValue);
    map["encoderNRPNNumber"] = juce::var(EncoderNRPNNumber);
    map["encoderPushDetection"] = juce::var(EncoderPushDetection);
    map["encoderPushOutputType"] = juce::var("CC");
    map["encoderPushMode"] = juce::var(EncoderPushMode);
    map["encoderPushCCNumber"] = juce::var(EncoderPushCCNumber);
    map["encoderPushValue"] = juce::var(EncoderPushValue);
    map["encoderPushNote"] = juce::var(EncoderPushNote);
    map["encoderPushProgram"] = juce::var(EncoderPushProgram);
    map["encoderPushChannel"] = juce::var(EncoderPushChannel);
    map["encoderDeadZone"] = juce::var(static_cast<double>(EncoderDeadZone));
    // Slide XY pad defaults
    map["slideCcNumberX"] = juce::var(ExpressionData1);
    map["slideCcNumberY"] = juce::var(ExpressionData1);
    map["slideSeparateAxisRanges"] = juce::var(false);
    map["touchpadInputMinX"] =
        juce::var(static_cast<double>(TouchpadInputMin));
    map["touchpadInputMaxX"] =
        juce::var(static_cast<double>(TouchpadInputMax));
    map["touchpadOutputMinX"] = juce::var(TouchpadOutputMin);
    map["touchpadOutputMaxX"] = juce::var(TouchpadOutputMax);
    map["touchpadInputMinY"] =
        juce::var(static_cast<double>(TouchpadInputMin));
    map["touchpadInputMaxY"] =
        juce::var(static_cast<double>(TouchpadInputMax));
    map["touchpadOutputMinY"] = juce::var(TouchpadOutputMin);
    map["touchpadOutputMaxY"] = juce::var(TouchpadOutputMax);
    // Command-type virtual defaults
    map["commandCategory"] = juce::var(100);      // Sustain
    map["globalModeDirection"] = juce::var(1);    // Up
    map["globalRootMode"] = juce::var(1);         // +1
    map["globalScaleMode"] = juce::var(1);        // Next
  }
  return map;
}

void setControlDefaultFromMap(InspectorControl &c) {
  if (c.propertyId.isEmpty())
    return;
  juce::var d = MappingDefinition::getDefaultValue(c.propertyId);
  if (!d.isVoid())
    c.defaultValue = d;
}
} // namespace

juce::var MappingDefinition::getDefaultValue(juce::StringRef propertyId) {
  const auto &map = getDefaultsMap();
  const juce::String key(propertyId);
  auto it = map.find(key);
  if (it != map.end())
    return it->second;
  return juce::var();
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
  using Cmd = MIDIQy::CommandID;
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
      {static_cast<int>(Cmd::GlobalRootUp), "Global Root +1"},
      {static_cast<int>(Cmd::GlobalRootDown), "Global Root -1"},
      {static_cast<int>(Cmd::GlobalRootSet), "Global Root Set"},
      {static_cast<int>(Cmd::GlobalScaleNext), "Global Scale Next"},
      {static_cast<int>(Cmd::GlobalScalePrev), "Global Scale Prev"},
      {static_cast<int>(Cmd::GlobalScaleSet), "Global Scale Set"},
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

bool MappingDefinition::isMappingEnabled(const juce::ValueTree &mapping) {
  return mapping.getProperty("enabled", MappingDefaults::Enabled);
}

InspectorSchema MappingDefinition::getSchema(const juce::ValueTree &mapping,
                                             int pitchBendRange,
                                             bool forTouchpadEditor) {
  InspectorSchema schema;

  // Alias context: many controls are shared between keyboard and touchpad
  // mappings, but some (Pitch pad, Slide, Encoder, touchpad hold behaviour,
  // etc.) only make sense for touchpad mappings. We derive a simple flag once
  // here and use it throughout the schema below. See MappingDefinition.h for
  // the full shared-schema contract.
  juce::String inputAlias =
      mapping.getProperty("inputAlias", "").toString().trim();
  const bool isTouchpadMapping =
      inputAlias.equalsIgnoreCase("Touchpad");

  // Enabled: mapping can be turned off without deleting (omit when in touchpad
  // header)
  if (!forTouchpadEditor) {
    InspectorControl enabledCtrl;
    enabledCtrl.propertyId = "enabled";
    enabledCtrl.label = "Enabled";
    enabledCtrl.controlType = InspectorControl::Type::Toggle;
    enabledCtrl.widthWeight = 0.5f;
    setControlDefaultFromMap(enabledCtrl);
    schema.push_back(enabledCtrl);
    schema.push_back(createSeparator("", juce::Justification::centred));
  }

  // Step 1: Common control – Type selector (ComboBox)
  InspectorControl typeCtrl;
  typeCtrl.propertyId = "type";
  typeCtrl.label = "Type";
  typeCtrl.controlType = InspectorControl::Type::ComboBox;
  typeCtrl.options[1] = "Note";
  typeCtrl.options[2] = "Expression";
  typeCtrl.options[3] = "Command";
  setControlDefaultFromMap(typeCtrl);
  schema.push_back(typeCtrl);

  schema.push_back(createSeparator("", juce::Justification::centred));

  // Step 2: Branch on current type
  juce::String typeStr = mapping.getProperty("type", "Note").toString().trim();

  if (typeStr.equalsIgnoreCase("Note")) {
    // Channel: Slider 1–16 (omit when in touchpad header)
    if (!forTouchpadEditor) {
      InspectorControl ch;
      ch.propertyId = "channel";
      ch.label = "Channel";
      ch.controlType = InspectorControl::Type::Slider;
      ch.min = 1.0;
      ch.max = 16.0;
      ch.step = 1.0;
      ch.valueFormat = InspectorControl::Format::Integer;
      setControlDefaultFromMap(ch);
      schema.push_back(ch);
    }

    // Data1 (Pitch): Slider 0–127, Label "Note", Format NoteName
    InspectorControl data1;
    data1.propertyId = "data1";
    data1.label = "Note";
    data1.controlType = InspectorControl::Type::Slider;
    data1.min = 0.0;
    data1.max = 127.0;
    data1.step = 1.0;
    data1.valueFormat = InspectorControl::Format::NoteName;
    setControlDefaultFromMap(data1);
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
    setControlDefaultFromMap(data2);
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
    setControlDefaultFromMap(velRand);
    schema.push_back(velRand);

    // Phase 55.9: Explicit separator before Note Settings
    schema.push_back(
        createSeparator("Note Settings", juce::Justification::centredLeft));

    InspectorControl releaseBehavior;
    releaseBehavior.propertyId = "releaseBehavior";
    releaseBehavior.label = "Release Behaviour";
    releaseBehavior.controlType = InspectorControl::Type::ComboBox;
    releaseBehavior.options[1] = "Send Note Off";
    releaseBehavior.options[2] = "Sustain until retrigger";
    releaseBehavior.options[3] = "Always Latch";
    setControlDefaultFromMap(releaseBehavior);
    schema.push_back(releaseBehavior);

    // Touchpad-specific: Hold behavior dropdown (only for touchpad Note
    // mappings)
    if (isTouchpadMapping) {
      InspectorControl holdBehavior;
      holdBehavior.propertyId = "touchpadHoldBehavior";
      holdBehavior.label = "Hold behaviour";
      holdBehavior.controlType = InspectorControl::Type::ComboBox;
      holdBehavior.options[1] = "Hold to not send note off immediately";
      holdBehavior.options[2] = "Ignore, send note off immediately";
      setControlDefaultFromMap(holdBehavior);
      schema.push_back(holdBehavior);
    }

    InspectorControl followTranspose;
    followTranspose.propertyId = "followTranspose";
    followTranspose.label = "Follow Global Transpose";
    followTranspose.controlType = InspectorControl::Type::Toggle;
    followTranspose.widthWeight = 0.5f;
    setControlDefaultFromMap(followTranspose);
    schema.push_back(followTranspose);
  } else if (typeStr.equalsIgnoreCase("Expression")) {
    juce::var adsrTargetVar = mapping.getProperty("adsrTarget", "CC");
    juce::String adsrTargetStr = adsrTargetVar.toString().trim();
    // Accept legacy integer combo IDs (1=CC, 2=PitchBend, 3=SmartScaleBend)
    if (adsrTargetVar.isInt()) {
      int id = (int)adsrTargetVar;
      if (id == 1) adsrTargetStr = "CC";
      else if (id == 2) adsrTargetStr = "PitchBend";
      else if (id == 3) adsrTargetStr = "SmartScaleBend";
    }
    bool useCustomEnvelope =
        (bool)mapping.getProperty("useCustomEnvelope", false);
    bool isPitchOrSmart = adsrTargetStr.equalsIgnoreCase("PitchBend") ||
                          adsrTargetStr.equalsIgnoreCase("SmartScaleBend");

    // 1. Basic Controls
    schema.push_back(
        createSeparator("Expression", juce::Justification::centredLeft));

    // Channel (omit when in touchpad header)
    if (!forTouchpadEditor) {
      InspectorControl ch;
      ch.propertyId = "channel";
      ch.label = "Channel";
      ch.controlType = InspectorControl::Type::Slider;
      ch.min = 1.0;
      ch.max = 16.0;
      ch.step = 1.0;
      ch.valueFormat = InspectorControl::Format::Integer;
      setControlDefaultFromMap(ch);
      schema.push_back(ch);
    }

    InspectorControl target;
    target.propertyId = "adsrTarget";
    target.label = "Target";
    target.controlType = InspectorControl::Type::ComboBox;
    target.options[1] = "CC";
    target.options[2] = "PitchBend";
    target.options[3] = "SmartScaleBend";
    setControlDefaultFromMap(target);
    schema.push_back(target);

    juce::String expressionCCModeStr =
        mapping
            .getProperty("expressionCCMode",
                         MappingDefaults::ExpressionCCModePosition)
            .toString()
            .trim();
    if (adsrTargetStr.equalsIgnoreCase("CC")) {
      InspectorControl data1;
      data1.propertyId = "data1";
      data1.label = "Controller";
      data1.controlType = InspectorControl::Type::Slider;
      data1.min = 0.0;
      data1.max = 127.0;
      data1.step = 1.0;
      setControlDefaultFromMap(data1);

      // When CC input mode is Slide and Axis is Both (XY pad), CC numbers are
      // managed in the XY pad section instead of the generic Controller
      // slider. Hide the Controller slider in that specific case.
      bool hideControllerForXYSlide = false;
      if (forTouchpadEditor &&
          expressionCCModeStr.equalsIgnoreCase("Slide")) {
        int slideAxisVal =
            (int)mapping.getProperty("slideAxis", MappingDefaults::SlideAxis);
        if (slideAxisVal == 2)
          hideControllerForXYSlide = true;
      }
      if (!hideControllerForXYSlide)
        schema.push_back(data1);

      // Touchpad-only: CC input mode (Position / Slide / Encoder). Keyboard
      // mappings behave like simple Position mode and never expose this
      // dropdown.
      if (isTouchpadMapping && forTouchpadEditor) {
        InspectorControl ccMode;
        ccMode.propertyId = "expressionCCMode";
        ccMode.label = "CC input mode";
        ccMode.controlType = InspectorControl::Type::ComboBox;
        ccMode.options[1] = "Position";
        ccMode.options[2] = "Slide";
        ccMode.options[3] = "Encoder";
        setControlDefaultFromMap(ccMode);
        schema.push_back(ccMode);
      }
    }
    const bool pitchPadUseCustomRange =
        (bool)mapping.getProperty("pitchPadUseCustomRange", false);
    if (adsrTargetStr.equalsIgnoreCase("PitchBend") && !pitchPadUseCustomRange) {
      InspectorControl bend;
      bend.propertyId = "data2";
      bend.label = "Bend (semitones)";
      bend.controlType = InspectorControl::Type::Slider;
      bend.min = -static_cast<double>(pitchBendRange);
      bend.max = static_cast<double>(pitchBendRange);
      bend.step = 1.0;
      bend.valueScaleRange = pitchBendRange;
      setControlDefaultFromMap(bend);
      schema.push_back(bend);
    }
    if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend") && !pitchPadUseCustomRange) {
      InspectorControl steps;
      steps.propertyId = "smartStepShift";
      steps.label = "Scale Steps";
      steps.controlType = InspectorControl::Type::Slider;
      // Keyboard / button mappings: allow negative and positive step shifts.
      // Touchpad tab: interpret this as a maximum range, so restrict to
      // positive values only.
      if (forTouchpadEditor && isTouchpadMapping) {
        steps.min = 1.0;
        steps.max = 12.0;
      } else {
        steps.min = -12.0;
        steps.max = 12.0;
      }
      steps.step = 1.0;
      setControlDefaultFromMap(steps);
      schema.push_back(steps);
    }

    if (forTouchpadEditor && isTouchpadMapping && isPitchOrSmart) {
      schema.push_back(
          createSeparator("Pitch pad", juce::Justification::centredLeft));

      // Touchpad-only: pitch-pad axis (Horizontal/Vertical). This is wired to
      // the underlying inputTouchpadEvent property in the Touchpad editor,
      // allowing pitch pads to run along X (horizontal) or Y (vertical) while
      // keeping keyboard mappings unaware of pitch-pad axis.
      InspectorControl pitchAxis;
      pitchAxis.propertyId = "pitchPadAxis";
      pitchAxis.label = "Axis";
      pitchAxis.controlType = InspectorControl::Type::ComboBox;
      pitchAxis.options[1] = "Horizontal (X)";
      pitchAxis.options[2] = "Vertical (Y)";
      setControlDefaultFromMap(pitchAxis);
      schema.push_back(pitchAxis);

      InspectorControl pitchMode;
      pitchMode.propertyId = "pitchPadMode";
      pitchMode.label = "Mode";
      pitchMode.controlType = InspectorControl::Type::ComboBox;
      pitchMode.options[1] = "Absolute";
      pitchMode.options[2] = "Relative";
      setControlDefaultFromMap(pitchMode);
      schema.push_back(pitchMode);
      juce::String pitchModeStr = mapping.getProperty("pitchPadMode", "Absolute").toString().trim();
      if (pitchModeStr.equalsIgnoreCase("Absolute")) {
        InspectorControl pitchStart;
        pitchStart.propertyId = "pitchPadStart";
        pitchStart.label = "Starting position";
        pitchStart.controlType = InspectorControl::Type::ComboBox;
        pitchStart.options[1] = "Left";
        pitchStart.options[2] = "Center";
        pitchStart.options[3] = "Right";
        pitchStart.options[4] = "Custom";
        setControlDefaultFromMap(pitchStart);
        schema.push_back(pitchStart);
        juce::String pitchStartStr = mapping.getProperty("pitchPadStart", "Center").toString().trim();
        if (pitchStartStr.equalsIgnoreCase("Custom")) {
          InspectorControl customStart;
          customStart.propertyId = "pitchPadCustomStart";
          customStart.label = "Custom start (X)";
          customStart.controlType = InspectorControl::Type::Slider;
          customStart.min = 0.0;
          customStart.max = 1.0;
          customStart.step = 0.01;
          setControlDefaultFromMap(customStart);
          schema.push_back(customStart);
        }
      }
      if (isPitchOrSmart) {
        InspectorControl customRangeToggle;
        customRangeToggle.propertyId = "pitchPadUseCustomRange";
        customRangeToggle.label = "Custom min/max";
        customRangeToggle.controlType = InspectorControl::Type::Toggle;
        customRangeToggle.widthWeight = 1.0f;
        setControlDefaultFromMap(customRangeToggle);
        schema.push_back(customRangeToggle);
      }
      const bool showPitchRangeSliders = pitchPadUseCustomRange;
      if (showPitchRangeSliders) {
        InspectorControl stepMin;
        stepMin.propertyId = "touchpadOutputMin";
        stepMin.controlType = InspectorControl::Type::Slider;
        stepMin.step = 1.0;
        setControlDefaultFromMap(stepMin);
        if (adsrTargetStr.equalsIgnoreCase("PitchBend")) {
          stepMin.label = "Pitch range min (semitones)";
          stepMin.min = -static_cast<double>(pitchBendRange);
          stepMin.max = static_cast<double>(pitchBendRange);
        } else {
          stepMin.label = "Scale step min";
          stepMin.min = -12.0;
          stepMin.max = 12.0;
        }
        schema.push_back(stepMin);
        InspectorControl stepMax;
        stepMax.propertyId = "touchpadOutputMax";
        stepMax.controlType = InspectorControl::Type::Slider;
        stepMax.step = 1.0;
        setControlDefaultFromMap(stepMax);
        if (adsrTargetStr.equalsIgnoreCase("PitchBend")) {
          stepMax.label = "Pitch range max (semitones)";
          stepMax.min = -static_cast<double>(pitchBendRange);
          stepMax.max = static_cast<double>(pitchBendRange);
        } else {
          stepMax.label = "Scale step max";
          stepMax.min = -12.0;
          stepMax.max = 12.0;
        }
        schema.push_back(stepMax);
      }
      InspectorControl restZone;
      restZone.propertyId = "pitchPadRestZonePercent";
      restZone.label = "Rest zone size";
      restZone.controlType = InspectorControl::Type::Slider;
      restZone.min = 0.0;
      restZone.max = 50.0;
      restZone.step = 1.0;
      setControlDefaultFromMap(restZone);
      schema.push_back(restZone);
      InspectorControl transitionZone;
      transitionZone.propertyId = "pitchPadTransitionZonePercent";
      transitionZone.label = "Transition zone size";
      transitionZone.controlType = InspectorControl::Type::Slider;
      transitionZone.min = 0.0;
      transitionZone.max = 50.0;
      transitionZone.step = 1.0;
      setControlDefaultFromMap(transitionZone);
      schema.push_back(transitionZone);
      InspectorControl touchGlide;
      touchGlide.propertyId = "pitchPadTouchGlideMs";
      touchGlide.label = "Touch glide (ms)";
      touchGlide.controlType = InspectorControl::Type::Slider;
      touchGlide.min = 0.0;
      touchGlide.max = 200.0;
      touchGlide.step = 5.0;
      touchGlide.suffix = " ms";
      setControlDefaultFromMap(touchGlide);
      schema.push_back(touchGlide);

      // SmartScaleBend always uses the global scale and root; per-mapping scale
      // selection is no longer supported. Show an informational label instead
      // of a per-mapping scale chooser.
      if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend")) {
        InspectorControl info;
        info.propertyId = "smartScaleInfo";
        info.label =
            "Smart Scale Bend always uses the global scale and root.\n"
            "Change them from the Global Scale settings.";
        info.controlType = InspectorControl::Type::LabelOnly;
        schema.push_back(info);
      }
    }

    if (adsrTargetStr.equalsIgnoreCase("CC") &&
        expressionCCModeStr.equalsIgnoreCase("Position")) {
      schema.push_back(createSeparator("Expression: Value when On / Off",
                                       juce::Justification::centredLeft));
      InspectorControl valOn;
      valOn.propertyId = "touchpadValueWhenOn";
      valOn.label = "Value when On";
      valOn.controlType = InspectorControl::Type::Slider;
      valOn.min = 0.0;
      valOn.max = 127.0;
      valOn.step = 1.0;
      setControlDefaultFromMap(valOn);
      schema.push_back(valOn);
      InspectorControl valOff;
      valOff.propertyId = "touchpadValueWhenOff";
      valOff.label = "Value when Off";
      valOff.controlType = InspectorControl::Type::Slider;
      valOff.min = 0.0;
      valOff.max = 127.0;
      valOff.step = 1.0;
      setControlDefaultFromMap(valOff);
      schema.push_back(valOff);

      // Touchpad-only: CC release behaviour (instant vs latch) for Position
      // mode.
      if (isTouchpadMapping && forTouchpadEditor) {
        InspectorControl ccRelease;
        ccRelease.propertyId = "ccReleaseBehavior";
        ccRelease.label = "Release behaviour";
        ccRelease.controlType = InspectorControl::Type::ComboBox;
        ccRelease.options[1] = "Send release (instant)";
        ccRelease.options[2] = "Always Latch";
        setControlDefaultFromMap(ccRelease);
        schema.push_back(ccRelease);
      }
    }

    if (isTouchpadMapping && forTouchpadEditor &&
        adsrTargetStr.equalsIgnoreCase("CC") &&
        expressionCCModeStr.equalsIgnoreCase("Slide")) {
      schema.push_back(createSeparator("Slide", juce::Justification::centredLeft));

      const int slideAxisVal =
          (int)mapping.getProperty("slideAxis", MappingDefaults::SlideAxis);
      const bool isXYPad = (slideAxisVal == 2);
      const bool separateRanges =
          (bool)mapping.getProperty("slideSeparateAxisRanges", false);
      const bool showSharedRanges = !(isXYPad && separateRanges);

      if (showSharedRanges) {
        InspectorControl inputMin;
        inputMin.propertyId = "touchpadInputMin";
        inputMin.label = "Input min";
        inputMin.controlType = InspectorControl::Type::Slider;
        inputMin.min = 0.0;
        inputMin.max = 1.0;
        inputMin.step = 0.01;
        setControlDefaultFromMap(inputMin);
        schema.push_back(inputMin);
        InspectorControl inputMax;
        inputMax.propertyId = "touchpadInputMax";
        inputMax.label = "Input max";
        inputMax.controlType = InspectorControl::Type::Slider;
        inputMax.min = 0.0;
        inputMax.max = 1.0;
        inputMax.step = 0.01;
        setControlDefaultFromMap(inputMax);
        schema.push_back(inputMax);
        InspectorControl outputMin;
        outputMin.propertyId = "touchpadOutputMin";
        outputMin.label = "Output min";
        outputMin.controlType = InspectorControl::Type::Slider;
        outputMin.min = 0.0;
        outputMin.max = 127.0;
        outputMin.step = 1.0;
        setControlDefaultFromMap(outputMin);
        schema.push_back(outputMin);
        InspectorControl outputMax;
        outputMax.propertyId = "touchpadOutputMax";
        outputMax.label = "Output max";
        outputMax.controlType = InspectorControl::Type::Slider;
        outputMax.min = 0.0;
        outputMax.max = 127.0;
        outputMax.step = 1.0;
        setControlDefaultFromMap(outputMax);
        schema.push_back(outputMax);
      }
      InspectorControl quickPrecision;
      quickPrecision.propertyId = "slideQuickPrecision";
      quickPrecision.label = "Quick / Precision";
      quickPrecision.controlType = InspectorControl::Type::ComboBox;
      quickPrecision.options[1] = "Quick";
      quickPrecision.options[2] = "Precision";
      setControlDefaultFromMap(quickPrecision);
      schema.push_back(quickPrecision);
      InspectorControl absRel;
      absRel.propertyId = "slideAbsRel";
      absRel.label = "Absolute / Relative";
      absRel.controlType = InspectorControl::Type::ComboBox;
      absRel.options[1] = "Absolute";
      absRel.options[2] = "Relative";
      setControlDefaultFromMap(absRel);
      schema.push_back(absRel);
      InspectorControl lockFree;
      lockFree.propertyId = "slideLockFree";
      lockFree.label = "Lock / Free";
      lockFree.controlType = InspectorControl::Type::ComboBox;
      lockFree.options[1] = "Lock";
      lockFree.options[2] = "Free";
      setControlDefaultFromMap(lockFree);
      schema.push_back(lockFree);
      InspectorControl slideAxis;
      slideAxis.propertyId = "slideAxis";
      slideAxis.label = "Axis";
      slideAxis.controlType = InspectorControl::Type::ComboBox;
      slideAxis.options[1] = "Vertical";
      slideAxis.options[2] = "Horizontal";
      slideAxis.options[3] = "Both (XY pad)";
      setControlDefaultFromMap(slideAxis);
      schema.push_back(slideAxis);
      InspectorControl returnToggle;
      returnToggle.propertyId = "slideReturnOnRelease";
      returnToggle.label = "Return to rest on finger release";
      returnToggle.controlType = InspectorControl::Type::Toggle;
      returnToggle.widthWeight = 1.0f;
      setControlDefaultFromMap(returnToggle);
      schema.push_back(returnToggle);
      InspectorControl restVal;
      restVal.propertyId = "slideRestValue";
      restVal.label = "Rest value";
      restVal.controlType = InspectorControl::Type::Slider;
      restVal.min = 0.0;
      restVal.max = 127.0;
      restVal.step = 1.0;
      restVal.enabledConditionProperty = "slideReturnOnRelease";
      setControlDefaultFromMap(restVal);
      schema.push_back(restVal);
      InspectorControl glideMs;
      glideMs.propertyId = "slideReturnGlideMs";
      glideMs.label = "Rest glide (ms)";
      glideMs.controlType = InspectorControl::Type::Slider;
      glideMs.min = 0.0;
      glideMs.max = 2000.0;
      glideMs.step = 10.0;
      glideMs.suffix = " ms";
      glideMs.enabledConditionProperty = "slideReturnOnRelease";
      setControlDefaultFromMap(glideMs);
      schema.push_back(glideMs);

      if (slideAxisVal == 2) {
        schema.push_back(createSeparator("XY pad",
                                         juce::Justification::centredLeft));
        InspectorControl ccX;
        ccX.propertyId = "slideCcNumberX";
        ccX.label = "Controller X (horizontal)";
        ccX.controlType = InspectorControl::Type::Slider;
        ccX.min = 0.0;
        ccX.max = 127.0;
        ccX.step = 1.0;
        setControlDefaultFromMap(ccX);
        schema.push_back(ccX);

        InspectorControl ccY;
        ccY.propertyId = "slideCcNumberY";
        ccY.label = "Controller Y (vertical)";
        ccY.controlType = InspectorControl::Type::Slider;
        ccY.min = 0.0;
        ccY.max = 127.0;
        ccY.step = 1.0;
        setControlDefaultFromMap(ccY);
        schema.push_back(ccY);

        InspectorControl separateRanges;
        separateRanges.propertyId = "slideSeparateAxisRanges";
        separateRanges.label = "Separate axis ranges";
        separateRanges.controlType = InspectorControl::Type::Toggle;
        separateRanges.widthWeight = 1.0f;
        setControlDefaultFromMap(separateRanges);
        schema.push_back(separateRanges);

        const bool useSeparate =
            (bool)mapping.getProperty("slideSeparateAxisRanges", false);
        if (useSeparate) {
          InspectorControl inMinX;
          inMinX.propertyId = "touchpadInputMinX";
          inMinX.label = "Input min (X)";
          inMinX.controlType = InspectorControl::Type::Slider;
          inMinX.min = 0.0;
          inMinX.max = 1.0;
          inMinX.step = 0.01;
          setControlDefaultFromMap(inMinX);
          schema.push_back(inMinX);

          InspectorControl inMaxX;
          inMaxX.propertyId = "touchpadInputMaxX";
          inMaxX.label = "Input max (X)";
          inMaxX.controlType = InspectorControl::Type::Slider;
          inMaxX.min = 0.0;
          inMaxX.max = 1.0;
          inMaxX.step = 0.01;
          setControlDefaultFromMap(inMaxX);
          schema.push_back(inMaxX);

          InspectorControl outMinX;
          outMinX.propertyId = "touchpadOutputMinX";
          outMinX.label = "Output min (X)";
          outMinX.controlType = InspectorControl::Type::Slider;
          outMinX.min = 0.0;
          outMinX.max = 127.0;
          outMinX.step = 1.0;
          setControlDefaultFromMap(outMinX);
          schema.push_back(outMinX);

          InspectorControl outMaxX;
          outMaxX.propertyId = "touchpadOutputMaxX";
          outMaxX.label = "Output max (X)";
          outMaxX.controlType = InspectorControl::Type::Slider;
          outMaxX.min = 0.0;
          outMaxX.max = 127.0;
          outMaxX.step = 1.0;
          setControlDefaultFromMap(outMaxX);
          schema.push_back(outMaxX);

          InspectorControl inMinY;
          inMinY.propertyId = "touchpadInputMinY";
          inMinY.label = "Input min (Y)";
          inMinY.controlType = InspectorControl::Type::Slider;
          inMinY.min = 0.0;
          inMinY.max = 1.0;
          inMinY.step = 0.01;
          setControlDefaultFromMap(inMinY);
          schema.push_back(inMinY);

          InspectorControl inMaxY;
          inMaxY.propertyId = "touchpadInputMaxY";
          inMaxY.label = "Input max (Y)";
          inMaxY.controlType = InspectorControl::Type::Slider;
          inMaxY.min = 0.0;
          inMaxY.max = 1.0;
          inMaxY.step = 0.01;
          setControlDefaultFromMap(inMaxY);
          schema.push_back(inMaxY);

          InspectorControl outMinY;
          outMinY.propertyId = "touchpadOutputMinY";
          outMinY.label = "Output min (Y)";
          outMinY.controlType = InspectorControl::Type::Slider;
          outMinY.min = 0.0;
          outMinY.max = 127.0;
          outMinY.step = 1.0;
          setControlDefaultFromMap(outMinY);
          schema.push_back(outMinY);

          InspectorControl outMaxY;
          outMaxY.propertyId = "touchpadOutputMaxY";
          outMaxY.label = "Output max (Y)";
          outMaxY.controlType = InspectorControl::Type::Slider;
          outMaxY.min = 0.0;
          outMaxY.max = 127.0;
          outMaxY.step = 1.0;
          setControlDefaultFromMap(outMaxY);
          schema.push_back(outMaxY);
        }
      }
    }

    if (isTouchpadMapping && forTouchpadEditor &&
        adsrTargetStr.equalsIgnoreCase("CC") &&
        expressionCCModeStr.equalsIgnoreCase("Encoder")) {
      schema.push_back(createSeparator("Encoder", juce::Justification::centredLeft));
      InspectorControl encAxis;
      encAxis.propertyId = "encoderAxis";
      encAxis.label = "Axis";
      encAxis.controlType = InspectorControl::Type::ComboBox;
      encAxis.options[1] = "Vertical";
      encAxis.options[2] = "Horizontal";
      encAxis.options[3] = "Both";
      setControlDefaultFromMap(encAxis);
      schema.push_back(encAxis);
      InspectorControl encSens;
      encSens.propertyId = "encoderSensitivity";
      encSens.label = "Movement per step";
      encSens.controlType = InspectorControl::Type::Slider;
      encSens.min = 0.1;
      encSens.max = 10.0;
      encSens.step = 0.1;
      setControlDefaultFromMap(encSens);
      schema.push_back(encSens);
      int encAxisVal = (int)mapping.getProperty("encoderAxis", MappingDefaults::EncoderAxis);
      if (encAxisVal == 2) {
        InspectorControl stepX;
        stepX.propertyId = "encoderStepSizeX";
        stepX.label = "Step size X";
        stepX.controlType = InspectorControl::Type::Slider;
        stepX.min = 1.0;
        stepX.max = 16.0;
        stepX.step = 1.0;
        setControlDefaultFromMap(stepX);
        schema.push_back(stepX);
        InspectorControl stepY;
        stepY.propertyId = "encoderStepSizeY";
        stepY.label = "Step size Y";
        stepY.controlType = InspectorControl::Type::Slider;
        stepY.min = 1.0;
        stepY.max = 16.0;
        stepY.step = 1.0;
        setControlDefaultFromMap(stepY);
        schema.push_back(stepY);
      } else {
        InspectorControl stepSize;
        stepSize.propertyId = "encoderStepSize";
        stepSize.label = "Step size";
        stepSize.controlType = InspectorControl::Type::Slider;
        stepSize.min = 1.0;
        stepSize.max = 16.0;
        stepSize.step = 1.0;
        setControlDefaultFromMap(stepSize);
        schema.push_back(stepSize);
      }
      schema.push_back(createSeparator("Output", juce::Justification::centredLeft));
      InspectorControl outMode;
      outMode.propertyId = "encoderOutputMode";
      outMode.label = "Output mode";
      outMode.controlType = InspectorControl::Type::ComboBox;
      outMode.options[1] = "Absolute";
      outMode.options[2] = "Relative";
      outMode.options[3] = "High-Resolution (NRPN)";
      setControlDefaultFromMap(outMode);
      schema.push_back(outMode);
      juce::String encOutModeStr = mapping.getProperty("encoderOutputMode", MappingDefaults::EncoderOutputModeAbsoluteStr).toString().trim();
      if (encOutModeStr.equalsIgnoreCase("Relative")) {
        InspectorControl relEnc;
        relEnc.propertyId = "encoderRelativeEncoding";
        relEnc.label = "Relative encoding";
        relEnc.controlType = InspectorControl::Type::ComboBox;
        relEnc.options[1] = "Offset Binary (64 ± n)";
        relEnc.options[2] = "Sign Magnitude";
        relEnc.options[3] = "Two's Complement";
        relEnc.options[4] = "Simple Incremental (0/127)";
        setControlDefaultFromMap(relEnc);
        schema.push_back(relEnc);
      }
      if (encOutModeStr.equalsIgnoreCase("Absolute")) {
        InspectorControl wrap;
        wrap.propertyId = "encoderWrap";
        wrap.label = "Wrap at 0/127";
        wrap.controlType = InspectorControl::Type::Toggle;
        setControlDefaultFromMap(wrap);
        schema.push_back(wrap);
        InspectorControl initVal;
        initVal.propertyId = "encoderInitialValue";
        initVal.label = "Initial value";
        initVal.controlType = InspectorControl::Type::Slider;
        initVal.min = 0.0;
        initVal.max = 127.0;
        initVal.step = 1.0;
        setControlDefaultFromMap(initVal);
        schema.push_back(initVal);
      }
      if (encOutModeStr.equalsIgnoreCase("NRPN")) {
        InspectorControl nrpn;
        nrpn.propertyId = "encoderNRPNNumber";
        nrpn.label = "NRPN Parameter Number";
        nrpn.controlType = InspectorControl::Type::Slider;
        nrpn.min = 0.0;
        nrpn.max = 16383.0;
        nrpn.step = 1.0;
        setControlDefaultFromMap(nrpn);
        schema.push_back(nrpn);
      }
      InspectorControl outMin;
      outMin.propertyId = "touchpadOutputMin";
      outMin.label = "Output min";
      outMin.controlType = InspectorControl::Type::Slider;
      outMin.min = 0.0;
      outMin.max = 127.0;
      outMin.step = 1.0;
      setControlDefaultFromMap(outMin);
      schema.push_back(outMin);
      InspectorControl outMax;
      outMax.propertyId = "touchpadOutputMax";
      outMax.label = "Output max";
      outMax.controlType = InspectorControl::Type::Slider;
      outMax.min = 0.0;
      outMax.max = 127.0;
      outMax.step = 1.0;
      setControlDefaultFromMap(outMax);
      schema.push_back(outMax);

      schema.push_back(createSeparator("Push Button", juce::Justification::centredLeft));
      InspectorControl pushDetect;
      pushDetect.propertyId = "encoderPushDetection";
      pushDetect.label = "Detection";
      pushDetect.controlType = InspectorControl::Type::ComboBox;
      pushDetect.options[1] = "Two-finger tap";
      pushDetect.options[2] = "Three-finger tap";
      pushDetect.options[3] = "Pressure threshold";
      setControlDefaultFromMap(pushDetect);
      schema.push_back(pushDetect);
      InspectorControl pushType;
      pushType.propertyId = "encoderPushOutputType";
      pushType.label = "Output type";
      pushType.controlType = InspectorControl::Type::ComboBox;
      pushType.options[1] = "Control Change (CC)";
      pushType.options[2] = "Note";
      pushType.options[3] = "Program Change";
      setControlDefaultFromMap(pushType);
      schema.push_back(pushType);
      InspectorControl pushMode;
      pushMode.propertyId = "encoderPushMode";
      pushMode.label = "Behavior";
      pushMode.controlType = InspectorControl::Type::ComboBox;
      pushMode.options[1] = "Off";
      pushMode.options[2] = "Momentary";
      pushMode.options[3] = "Toggle";
      pushMode.options[4] = "Trigger";
      setControlDefaultFromMap(pushMode);
      schema.push_back(pushMode);
      int encPushMode = (int)mapping.getProperty("encoderPushMode", MappingDefaults::EncoderPushMode);
      if (encPushMode != 0) {
        juce::String encPushTypeStr = mapping.getProperty("encoderPushOutputType", "CC").toString().trim();
        if (encPushTypeStr.equalsIgnoreCase("CC")) {
          InspectorControl pushCC;
          pushCC.propertyId = "encoderPushCCNumber";
          pushCC.label = "CC number";
          pushCC.controlType = InspectorControl::Type::Slider;
          pushCC.min = 0.0;
          pushCC.max = 127.0;
          pushCC.step = 1.0;
          setControlDefaultFromMap(pushCC);
          schema.push_back(pushCC);
          InspectorControl pushVal;
          pushVal.propertyId = "encoderPushValue";
          pushVal.label = "Push value";
          pushVal.controlType = InspectorControl::Type::Slider;
          pushVal.min = 0.0;
          pushVal.max = 127.0;
          pushVal.step = 1.0;
          setControlDefaultFromMap(pushVal);
          schema.push_back(pushVal);
        } else if (encPushTypeStr.equalsIgnoreCase("Note")) {
          InspectorControl pushNote;
          pushNote.propertyId = "encoderPushNote";
          pushNote.label = "Note";
          pushNote.controlType = InspectorControl::Type::Slider;
          pushNote.min = 0.0;
          pushNote.max = 127.0;
          pushNote.step = 1.0;
          pushNote.valueFormat = InspectorControl::Format::NoteName;
          setControlDefaultFromMap(pushNote);
          schema.push_back(pushNote);
          InspectorControl pushCh;
          pushCh.propertyId = "encoderPushChannel";
          pushCh.label = "Channel";
          pushCh.controlType = InspectorControl::Type::Slider;
          pushCh.min = 1.0;
          pushCh.max = 16.0;
          pushCh.step = 1.0;
          setControlDefaultFromMap(pushCh);
          schema.push_back(pushCh);
        } else if (encPushTypeStr.equalsIgnoreCase("ProgramChange")) {
          InspectorControl pushProg;
          pushProg.propertyId = "encoderPushProgram";
          pushProg.label = "Program";
          pushProg.controlType = InspectorControl::Type::Slider;
          pushProg.min = 0.0;
          pushProg.max = 127.0;
          pushProg.step = 1.0;
          setControlDefaultFromMap(pushProg);
          schema.push_back(pushProg);
          InspectorControl pushCh;
          pushCh.propertyId = "encoderPushChannel";
          pushCh.label = "Channel";
          pushCh.controlType = InspectorControl::Type::Slider;
          pushCh.min = 1.0;
          pushCh.max = 16.0;
          pushCh.step = 1.0;
          setControlDefaultFromMap(pushCh);
          schema.push_back(pushCh);
        }
      }

      schema.push_back(createSeparator("Advanced", juce::Justification::centredLeft));
      InspectorControl deadZone;
      deadZone.propertyId = "encoderDeadZone";
      deadZone.label = "Dead zone (ignore small movement from anchor)";
      deadZone.controlType = InspectorControl::Type::Slider;
      deadZone.min = 0.0;
      deadZone.max = 0.5;
      deadZone.step = 0.01;
      setControlDefaultFromMap(deadZone);
      schema.push_back(deadZone);
    }

    // 2. Mode Selection (Dynamics: only for Position mode or non-CC targets)
    const bool showDynamics =
        isPitchOrSmart ||
        (adsrTargetStr.equalsIgnoreCase("CC") && expressionCCModeStr.equalsIgnoreCase("Position"));
    if (showDynamics) {
    schema.push_back(
        createSeparator("Dynamics", juce::Justification::centredLeft));
    // Custom ADSR is only for CC; don't show it for PitchBend/SmartScaleBend (code ignores it).
    if (!isPitchOrSmart) {
      InspectorControl useEnv;
      useEnv.propertyId = "useCustomEnvelope";
      useEnv.label = "Use Custom ADSR Curve";
      useEnv.controlType = InspectorControl::Type::Toggle;
      useEnv.widthWeight = 1.0f;
      setControlDefaultFromMap(useEnv);
      schema.push_back(useEnv);
    }
    // ADSR sliders only when custom envelope is enabled (CC only).
    if (useCustomEnvelope && !isPitchOrSmart) {
      auto addAdsrSlider = [&schema](const juce::String &propId,
                                     const juce::String &lbl, double maxVal) {
        InspectorControl c;
        c.propertyId = propId;
        c.label = lbl;
        c.controlType = InspectorControl::Type::Slider;
        c.min = 0.0;
        c.max = maxVal;
        c.step = (maxVal <= 1.0) ? 0.01 : 1.0;
        setControlDefaultFromMap(c);
        schema.push_back(c);
      };
      addAdsrSlider("adsrAttack", "Attack (ms)", 5000.0);
      addAdsrSlider("adsrDecay", "Decay (ms)", 5000.0);
      addAdsrSlider("adsrSustain", "Sustain (0-1)", 1.0);
      addAdsrSlider("adsrRelease", "Release (ms)", 5000.0);
    } else if (isPitchOrSmart) {
      // PitchBend/SmartScaleBend: only "Reset pitch on release". CC has no toggle (always sends value when off).
      InspectorControl sendRel;
      sendRel.propertyId = "sendReleaseValue";
      sendRel.label = "Reset pitch on release";
      sendRel.controlType = InspectorControl::Type::Toggle;
      sendRel.widthWeight = 1.0f;
      setControlDefaultFromMap(sendRel);
      schema.push_back(sendRel);
    }
    } // showDynamics
  } else if (typeStr.equalsIgnoreCase("Command")) {
    // Command categories – virtual UI ids, mapped to concrete CommandIDs by
    // MappingInspectorLogic.
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

    int cmdId = (int)mapping.getProperty("data1", 0);
    const bool isSustain = (cmdId >= 0 && cmdId <= 2);
    const bool isLatch = (cmdId == (int)MIDIQy::CommandID::LatchToggle);
    const bool isPanic =
        (cmdId == (int)MIDIQy::CommandID::Panic ||
         cmdId == (int)MIDIQy::CommandID::PanicLatch);
    const bool isTranspose =
        (cmdId == (int)MIDIQy::CommandID::Transpose ||
         cmdId == (int)MIDIQy::CommandID::GlobalPitchDown);
    const bool isGlobalMode =
        (cmdId == (int)MIDIQy::CommandID::GlobalModeUp ||
         cmdId == (int)MIDIQy::CommandID::GlobalModeDown);
    const bool isGlobalRoot =
        (cmdId == (int)MIDIQy::CommandID::GlobalRootUp ||
         cmdId == (int)MIDIQy::CommandID::GlobalRootDown ||
         cmdId == (int)MIDIQy::CommandID::GlobalRootSet);
    const bool isGlobalScale =
        (cmdId == (int)MIDIQy::CommandID::GlobalScaleNext ||
         cmdId == (int)MIDIQy::CommandID::GlobalScalePrev ||
         cmdId == (int)MIDIQy::CommandID::GlobalScaleSet);
    const bool isLayer =
        (cmdId == (int)MIDIQy::CommandID::LayerMomentary ||
         cmdId == (int)MIDIQy::CommandID::LayerToggle);
    const bool isKeyboardGroupSolo =
        (cmdId == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloMomentary ||
         cmdId == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloToggle ||
         cmdId == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloSet ||
         cmdId == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloClear);
    const bool isTouchpadGroupSolo =
        (cmdId == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary ||
         cmdId == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle ||
         cmdId == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloSet ||
         cmdId == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloClear);

    InspectorControl cmdCtrl;
    cmdCtrl.propertyId = "commandCategory";
    cmdCtrl.label = "Command";
    cmdCtrl.controlType = InspectorControl::Type::ComboBox;
    cmdCtrl.options[kCmdCategorySustain] = "Sustain";
    cmdCtrl.options[kCmdCategoryLatch] = "Latch";
    cmdCtrl.options[kCmdCategoryPanic] = "Panic";
    cmdCtrl.options[kCmdCategoryTranspose] = "Transpose";
    cmdCtrl.options[kCmdCategoryGlobalMode] = "Global mode";
    cmdCtrl.options[kCmdCategoryGlobalRoot] = "Global Root";
    cmdCtrl.options[kCmdCategoryGlobalScale] = "Global Scale";
    cmdCtrl.options[kCmdCategoryLayer] = "Layer";
    cmdCtrl.options[kCmdCategoryKeyboardGroupSolo] = "Keyboard group";
    cmdCtrl.options[kCmdCategoryTouchpadGroupSolo] = "Touchpad group";
    setControlDefaultFromMap(cmdCtrl);
    schema.push_back(cmdCtrl);

    if (isSustain) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "sustainStyle"; // Virtual: maps to data1 (0,1,2)
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to sustain";
      styleCtrl.options[2] = "Toggle sustain";
      styleCtrl.options[3] = "Default is on. Hold to not sustain";
      setControlDefaultFromMap(styleCtrl);
      schema.push_back(styleCtrl);
    }

    if (isLayer) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "layerStyle"; // Virtual: maps to data1 (10,11)
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to switch";
      styleCtrl.options[2] = "Toggle layer";
      setControlDefaultFromMap(styleCtrl);
      schema.push_back(styleCtrl);
    }

    const int layerMomentary =
        static_cast<int>(MIDIQy::CommandID::LayerMomentary);
    const int layerToggle = static_cast<int>(MIDIQy::CommandID::LayerToggle);

    if (isLayer || cmdId == layerMomentary || cmdId == layerToggle) {
      InspectorControl data2;
      data2.propertyId = "data2";
      data2.label = "Target Layer";
      data2.controlType = InspectorControl::Type::ComboBox;
      data2.options = getLayerOptions();
      setControlDefaultFromMap(data2);
      schema.push_back(data2);
    }
    if (isKeyboardGroupSolo) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "keyboardSoloType";
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to solo";
      styleCtrl.options[2] = "Toggle solo";
      styleCtrl.options[3] = "Set solo";
      styleCtrl.options[4] = "Clear solo";
      setControlDefaultFromMap(styleCtrl);
      schema.push_back(styleCtrl);
      InspectorControl groupCtrl;
      groupCtrl.propertyId = "keyboardLayoutGroupId";
      groupCtrl.label = "Keyboard group";
      groupCtrl.controlType = InspectorControl::Type::ComboBox;
      groupCtrl.options[0] = "None";
      setControlDefaultFromMap(groupCtrl);
      schema.push_back(groupCtrl);
      InspectorControl scopeCtrl;
      scopeCtrl.propertyId = "keyboardSoloScope";
      scopeCtrl.label = "Scope";
      scopeCtrl.controlType = InspectorControl::Type::ComboBox;
      scopeCtrl.options[1] = "Global";
      scopeCtrl.options[2] = "Layer (forget on change)";
      scopeCtrl.options[3] = "Layer (remember)";
      setControlDefaultFromMap(scopeCtrl);
      schema.push_back(scopeCtrl);
    }
    if (isTouchpadGroupSolo) {
      InspectorControl styleCtrl;
      styleCtrl.propertyId = "touchpadSoloType";
      styleCtrl.label = "Style";
      styleCtrl.controlType = InspectorControl::Type::ComboBox;
      styleCtrl.options[1] = "Hold to solo";
      styleCtrl.options[2] = "Toggle solo";
      styleCtrl.options[3] = "Set solo";
      styleCtrl.options[4] = "Clear solo";
      setControlDefaultFromMap(styleCtrl);
      schema.push_back(styleCtrl);
      InspectorControl groupCtrl;
      groupCtrl.propertyId = "touchpadLayoutGroupId";
      groupCtrl.label = "Touchpad group";
      groupCtrl.controlType = InspectorControl::Type::ComboBox;
      groupCtrl.options[0] = "None";
      setControlDefaultFromMap(groupCtrl);
      schema.push_back(groupCtrl);
      InspectorControl scopeCtrl;
      scopeCtrl.propertyId = "touchpadSoloScope";
      scopeCtrl.label = "Scope";
      scopeCtrl.controlType = InspectorControl::Type::ComboBox;
      scopeCtrl.options[1] = "Global";
      scopeCtrl.options[2] = "Layer (forget on change)";
      scopeCtrl.options[3] = "Layer (remember)";
      setControlDefaultFromMap(scopeCtrl);
      schema.push_back(scopeCtrl);
    }
    const int latchToggle = static_cast<int>(MIDIQy::CommandID::LatchToggle);
    if (cmdId == latchToggle) {
      InspectorControl releaseLatched;
      releaseLatched.propertyId = "releaseLatchedOnToggleOff";
      releaseLatched.label = "Release latched when toggling off";
      releaseLatched.controlType = InspectorControl::Type::Toggle;
      setControlDefaultFromMap(releaseLatched);
      schema.push_back(releaseLatched);
    }
    const int panic = static_cast<int>(MIDIQy::CommandID::Panic);
    const int panicLatch = static_cast<int>(MIDIQy::CommandID::PanicLatch);
    if (cmdId == panic || cmdId == panicLatch) {
      InspectorControl panicMode;
      panicMode.propertyId = "panicMode"; // Virtual: maps to data1=4, data2
      panicMode.label = "Mode";
      panicMode.controlType = InspectorControl::Type::ComboBox;
      panicMode.options[1] = "Panic all";
      panicMode.options[2] = "Panic latched only";
      panicMode.options[3] = "Panic chords";
      setControlDefaultFromMap(panicMode);
      schema.push_back(panicMode);
    }
    if (isTranspose) {
      schema.push_back(
          createSeparator("Transpose", juce::Justification::centredLeft));
      InspectorControl modeCtrl;
      modeCtrl.propertyId = "transposeMode"; // "global" / "local"
      modeCtrl.label = "Mode";
      modeCtrl.controlType = InspectorControl::Type::ComboBox;
      modeCtrl.options[1] = "Global";
      modeCtrl.options[2] = "Local";
      setControlDefaultFromMap(modeCtrl);
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
      setControlDefaultFromMap(modifyCtrl);
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
        setControlDefaultFromMap(semitonesCtrl);
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

    if (isGlobalMode) {
      schema.push_back(
          createSeparator("Global mode", juce::Justification::centredLeft));
      InspectorControl dirCtrl;
      dirCtrl.propertyId = "globalModeDirection";
      dirCtrl.label = "Direction";
      dirCtrl.controlType = InspectorControl::Type::ComboBox;
      dirCtrl.options[1] = "Up";
      dirCtrl.options[2] = "Down";
      setControlDefaultFromMap(dirCtrl);
      schema.push_back(dirCtrl);
    }

    const int globalRootSet =
        static_cast<int>(MIDIQy::CommandID::GlobalRootSet);
    const int globalScaleSet =
        static_cast<int>(MIDIQy::CommandID::GlobalScaleSet);
    if (isGlobalRoot) {
      schema.push_back(
          createSeparator("Global Root", juce::Justification::centredLeft));
      InspectorControl rootMode;
      rootMode.propertyId = "globalRootMode";
      rootMode.label = "Mode";
      rootMode.controlType = InspectorControl::Type::ComboBox;
      rootMode.options[1] = "Root +1";
      rootMode.options[2] = "Root -1";
      rootMode.options[3] = "Set root note";
      setControlDefaultFromMap(rootMode);
      schema.push_back(rootMode);
      if (cmdId == globalRootSet) {
        InspectorControl rootNoteCtrl;
        rootNoteCtrl.propertyId = "rootNote";
        rootNoteCtrl.label = "Root note (0-127)";
        rootNoteCtrl.controlType = InspectorControl::Type::Slider;
        rootNoteCtrl.min = 0.0;
        rootNoteCtrl.max = 127.0;
        rootNoteCtrl.step = 1.0;
        rootNoteCtrl.valueFormat = InspectorControl::Format::Integer;
        setControlDefaultFromMap(rootNoteCtrl);
        schema.push_back(rootNoteCtrl);
      }
    }
    if (isGlobalScale) {
      schema.push_back(
          createSeparator("Global Scale", juce::Justification::centredLeft));
      InspectorControl scaleMode;
      scaleMode.propertyId = "globalScaleMode";
      scaleMode.label = "Mode";
      scaleMode.controlType = InspectorControl::Type::ComboBox;
      scaleMode.options[1] = "Next scale";
      scaleMode.options[2] = "Previous scale";
      scaleMode.options[3] = "Set scale index";
      setControlDefaultFromMap(scaleMode);
      schema.push_back(scaleMode);
      if (cmdId == globalScaleSet) {
        InspectorControl scaleIndexCtrl;
        scaleIndexCtrl.propertyId = "scaleIndex";
        scaleIndexCtrl.label = "Scale index (0-based)";
        scaleIndexCtrl.controlType = InspectorControl::Type::Slider;
        scaleIndexCtrl.min = 0.0;
        scaleIndexCtrl.max = 63.0;
        scaleIndexCtrl.step = 1.0;
        scaleIndexCtrl.valueFormat = InspectorControl::Format::Integer;
        setControlDefaultFromMap(scaleIndexCtrl);
        schema.push_back(scaleIndexCtrl);
      }
    }
  }

  return schema;
}
