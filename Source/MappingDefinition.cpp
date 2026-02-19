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
    map["touchpadLayoutGroupId"] = juce::var(TouchpadLayoutGroupId);
    map["touchpadSoloScope"] = juce::var(TouchpadSoloScope);
    map["touchpadThreshold"] = juce::var(static_cast<double>(TouchpadThreshold));
    map["touchpadTriggerAbove"] = juce::var(TouchpadTriggerAbove);
    map["touchpadInputMin"] = juce::var(static_cast<double>(TouchpadInputMin));
    map["touchpadInputMax"] = juce::var(static_cast<double>(TouchpadInputMax));
    map["touchpadOutputMin"] = juce::var(TouchpadOutputMin);
    map["touchpadOutputMax"] = juce::var(TouchpadOutputMax);
    map["pitchPadCustomStart"] = juce::var(static_cast<double>(PitchPadCustomStart));
    map["pitchPadRestingPercent"] = juce::var(static_cast<double>(PitchPadRestingPercent));
    map["transposeModify"] = juce::var(TransposeModify);
    map["transposeSemitones"] = juce::var(TransposeSemitones);
    map["rootModify"] = juce::var(RootModify);
    map["rootNote"] = juce::var(RootNote);
    map["scaleModify"] = juce::var(ScaleModify);
    map["scaleIndex"] = juce::var(ScaleIndex);
    map["smartStepShift"] = juce::var(SmartStepShift);
    map["inputKey"] = juce::var(InputKey);
    map["expressionCCMode"] = juce::var(ExpressionCCModePosition);
    map["slideQuickPrecision"] = juce::var(SlideQuickPrecision);
    map["slideAbsRel"] = juce::var(SlideAbsRel);
    map["slideLockFree"] = juce::var(SlideLockFree);
    map["slideAxis"] = juce::var(SlideAxis);
    map["encoderStepSize"] = juce::var(EncoderStepSize);
    map["encoderStepSizeX"] = juce::var(EncoderStepSizeX);
    map["encoderStepSizeY"] = juce::var(EncoderStepSizeY);
    map["encoderWrap"] = juce::var(EncoderWrap);
    map["encoderAxis"] = juce::var(EncoderAxis);
    map["encoderPushMode"] = juce::var(EncoderPushMode);
    map["encoderPushValue"] = juce::var(EncoderPushValue);
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

    // Touchpad-specific: Hold behavior dropdown (only for touchpad Note mappings)
    juce::String inputAlias =
        mapping.getProperty("inputAlias", "").toString().trim();
    bool isTouchpadMapping =
        inputAlias.equalsIgnoreCase("Touchpad");
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
        mapping.getProperty("expressionCCMode", MappingDefaults::ExpressionCCModePosition).toString().trim();
    if (adsrTargetStr.equalsIgnoreCase("CC")) {
      InspectorControl data1;
      data1.propertyId = "data1";
      data1.label = "Controller";
      data1.controlType = InspectorControl::Type::Slider;
      data1.min = 0.0;
      data1.max = 127.0;
      data1.step = 1.0;
      setControlDefaultFromMap(data1);
      schema.push_back(data1);
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
    if (adsrTargetStr.equalsIgnoreCase("PitchBend")) {
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
    if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend")) {
      InspectorControl steps;
      steps.propertyId = "smartStepShift";
      steps.label = "Scale Steps";
      steps.controlType = InspectorControl::Type::Slider;
      steps.min = -12.0;
      steps.max = 12.0;
      steps.step = 1.0;
      setControlDefaultFromMap(steps);
      schema.push_back(steps);
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
    }

    if (adsrTargetStr.equalsIgnoreCase("CC") &&
        expressionCCModeStr.equalsIgnoreCase("Slide")) {
      schema.push_back(createSeparator("Slide", juce::Justification::centredLeft));
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
      setControlDefaultFromMap(slideAxis);
      schema.push_back(slideAxis);
    }

    if (adsrTargetStr.equalsIgnoreCase("CC") &&
        expressionCCModeStr.equalsIgnoreCase("Encoder")) {
      schema.push_back(createSeparator("Encoder", juce::Justification::centredLeft));
      InspectorControl stepSize;
      stepSize.propertyId = "encoderStepSize";
      stepSize.label = "Step size";
      stepSize.controlType = InspectorControl::Type::Slider;
      stepSize.min = 1.0;
      stepSize.max = 16.0;
      stepSize.step = 1.0;
      setControlDefaultFromMap(stepSize);
      schema.push_back(stepSize);
      InspectorControl stepSizeX;
      stepSizeX.propertyId = "encoderStepSizeX";
      stepSizeX.label = "Step size X (horizontal)";
      stepSizeX.controlType = InspectorControl::Type::Slider;
      stepSizeX.min = 1.0;
      stepSizeX.max = 16.0;
      stepSizeX.step = 1.0;
      setControlDefaultFromMap(stepSizeX);
      schema.push_back(stepSizeX);
      InspectorControl stepSizeY;
      stepSizeY.propertyId = "encoderStepSizeY";
      stepSizeY.label = "Step size Y (vertical)";
      stepSizeY.controlType = InspectorControl::Type::Slider;
      stepSizeY.min = 1.0;
      stepSizeY.max = 16.0;
      stepSizeY.step = 1.0;
      setControlDefaultFromMap(stepSizeY);
      schema.push_back(stepSizeY);
      InspectorControl wrap;
      wrap.propertyId = "encoderWrap";
      wrap.label = "Wrap at 0/127";
      wrap.controlType = InspectorControl::Type::Toggle;
      wrap.widthWeight = 1.0f;
      setControlDefaultFromMap(wrap);
      schema.push_back(wrap);
      InspectorControl axis;
      axis.propertyId = "encoderAxis";
      axis.label = "Axis";
      axis.controlType = InspectorControl::Type::ComboBox;
      axis.options[1] = "Vertical";
      axis.options[2] = "Horizontal";
      axis.options[3] = "Both";
      setControlDefaultFromMap(axis);
      schema.push_back(axis);
      InspectorControl pushMode;
      pushMode.propertyId = "encoderPushMode";
      pushMode.label = "Push mode";
      pushMode.controlType = InspectorControl::Type::ComboBox;
      pushMode.options[1] = "Off";
      pushMode.options[2] = "Momentary";
      pushMode.options[3] = "Toggle";
      pushMode.options[4] = "Trigger";
      setControlDefaultFromMap(pushMode);
      schema.push_back(pushMode);
      InspectorControl pushVal;
      pushVal.propertyId = "encoderPushValue";
      pushVal.label = "Push value";
      pushVal.controlType = InspectorControl::Type::Slider;
      pushVal.min = 0.0;
      pushVal.max = 127.0;
      pushVal.step = 1.0;
      setControlDefaultFromMap(pushVal);
      schema.push_back(pushVal);
    }

    // 2. Mode Selection (Dynamics: only for Position mode or non-CC targets)
    const bool showDynamics =
        isPitchOrSmart ||
        (adsrTargetStr.equalsIgnoreCase("CC") && expressionCCModeStr.equalsIgnoreCase("Position"));
    if (showDynamics) {
    schema.push_back(
        createSeparator("Dynamics", juce::Justification::centredLeft));
    InspectorControl useEnv;
    useEnv.propertyId = "useCustomEnvelope";
    useEnv.label = "Use Custom ADSR Curve";
    useEnv.controlType = InspectorControl::Type::Toggle;
    useEnv.widthWeight = 1.0f;
    // Disable custom envelope for PitchBend and SmartScaleBend (not supported)
    useEnv.isEnabled = !isPitchOrSmart;
    setControlDefaultFromMap(useEnv);
    schema.push_back(useEnv);

    // 3. Conditional Branch
    // Only show ADSR sliders if custom envelope is enabled AND not pitch bend
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
    } else {
      // Only PitchBend/SmartScaleBend get "Reset pitch on release". CC always
      // sends Value when Off on release (no toggle/slider).
      if (isPitchOrSmart) {
        InspectorControl sendRel;
        sendRel.propertyId = "sendReleaseValue";
        sendRel.label = "Reset pitch on release";
        sendRel.controlType = InspectorControl::Type::Toggle;
        sendRel.widthWeight = 1.0f;
        setControlDefaultFromMap(sendRel);
        schema.push_back(sendRel);
      }
    }
    } // showDynamics
  } else if (typeStr.equalsIgnoreCase("Command")) {
    static constexpr int kSustainCategoryId = 100;
    static constexpr int kLayerCategoryId = 110;
    int cmdId = (int)mapping.getProperty("data1", 0);
    const bool isSustain = (cmdId >= 0 && cmdId <= 2);
    const bool isLayer = (cmdId == 10 || cmdId == 11);

    InspectorControl cmdCtrl;
    cmdCtrl.propertyId =
        (isSustain || isLayer) ? "commandCategory" : "data1";
    cmdCtrl.label = "Command";
    cmdCtrl.controlType = InspectorControl::Type::ComboBox;
    cmdCtrl.options[kSustainCategoryId] = "Sustain";
    cmdCtrl.options[3] = "Latch Toggle";
    cmdCtrl.options[4] = "Panic";
    cmdCtrl.options[6] = "Transpose";
    cmdCtrl.options[8] = "Global Mode Up";
    cmdCtrl.options[9] = "Global Mode Down";
    cmdCtrl.options[12] = "Global Root +1";
    cmdCtrl.options[13] = "Global Root -1";
    cmdCtrl.options[14] = "Global Root Set";
    cmdCtrl.options[15] = "Global Scale Next";
    cmdCtrl.options[16] = "Global Scale Prev";
    cmdCtrl.options[17] = "Global Scale Set";
    cmdCtrl.options[kLayerCategoryId] = "Layer";
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

    cmdId = (int)mapping.getProperty("data1", 0);
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
    const int transpose = static_cast<int>(MIDIQy::CommandID::Transpose);
    const int globalPitchDown =
        static_cast<int>(MIDIQy::CommandID::GlobalPitchDown);
    if (cmdId == transpose || cmdId == globalPitchDown) {
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

    const int globalRootSet = static_cast<int>(MIDIQy::CommandID::GlobalRootSet);
    const int globalScaleSet =
        static_cast<int>(MIDIQy::CommandID::GlobalScaleSet);
    if (cmdId == static_cast<int>(MIDIQy::CommandID::GlobalRootUp) ||
        cmdId == static_cast<int>(MIDIQy::CommandID::GlobalRootDown) ||
        cmdId == globalRootSet) {
      schema.push_back(
          createSeparator("Global Root", juce::Justification::centredLeft));
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
    if (cmdId == static_cast<int>(MIDIQy::CommandID::GlobalScaleNext) ||
        cmdId == static_cast<int>(MIDIQy::CommandID::GlobalScalePrev) ||
        cmdId == globalScaleSet) {
      schema.push_back(
          createSeparator("Global Scale", juce::Justification::centredLeft));
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
