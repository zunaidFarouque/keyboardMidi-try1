#include "TouchpadMixerDefinition.h"

namespace {
constexpr int kTypeMixerId = 1;
constexpr int kTypeDrumPadId = 2;
constexpr int kTypeChordPadId = 3;
} // namespace

InspectorSchema TouchpadMixerDefinition::getCommonLayoutHeader() {
  InspectorSchema schema;
  InspectorControl nameCtrl;
  nameCtrl.propertyId = "name";
  nameCtrl.label = "Name";
  nameCtrl.controlType = InspectorControl::Type::TextEditor;
  schema.push_back(nameCtrl);
  InspectorControl typeCtrl;
  typeCtrl.propertyId = "type";
  typeCtrl.label = "Type";
  typeCtrl.controlType = InspectorControl::Type::ComboBox;
  typeCtrl.options[kTypeMixerId] = "Mixer";
  typeCtrl.options[kTypeDrumPadId] = "Drum Pad / Launcher";
  typeCtrl.options[kTypeChordPadId] = "Chord Pad";
  schema.push_back(typeCtrl);
  InspectorControl layerCtrl;
  layerCtrl.propertyId = "layerId";
  layerCtrl.label = "Layer";
  layerCtrl.controlType = InspectorControl::Type::ComboBox;
  layerCtrl.options[1] = "Base";
  for (int i = 1; i <= 8; ++i)
    layerCtrl.options[i + 1] = "Layer " + juce::String(i);
  schema.push_back(layerCtrl);

  // Optional layout group (drop-down, populated from registry).
  InspectorControl groupIdCtrl;
  groupIdCtrl.propertyId = "layoutGroupId";
  groupIdCtrl.label = "Touchpad group";
  groupIdCtrl.controlType = InspectorControl::Type::ComboBox;
  groupIdCtrl.options[0] = "- No Group -"; // Actual group list filled in editor
  schema.push_back(groupIdCtrl);
  InspectorControl chCtrl;
  chCtrl.propertyId = "midiChannel";
  chCtrl.label = "Channel";
  chCtrl.controlType = InspectorControl::Type::Slider;
  chCtrl.min = 1.0;
  chCtrl.max = 16.0;
  chCtrl.step = 1.0;
  chCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(chCtrl);
  InspectorControl zIndexCtrl;
  zIndexCtrl.propertyId = "zIndex";
  zIndexCtrl.label = "Z-index";
  zIndexCtrl.controlType = InspectorControl::Type::Slider;
  zIndexCtrl.min = -100.0;
  zIndexCtrl.max = 100.0;
  zIndexCtrl.step = 1.0;
  zIndexCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(zIndexCtrl);
  return schema;
}

InspectorSchema TouchpadMixerDefinition::getCommonLayoutControls() {
  InspectorSchema schema;
  schema.push_back(
      MappingDefinition::createSeparator("Region", juce::Justification::centredLeft));
  // Region sliders and a convenience relayout button (handled specially by the
  // editor component).
  InspectorControl regionLeft;
  regionLeft.propertyId = "regionLeft";
  regionLeft.label = "Region left";
  regionLeft.controlType = InspectorControl::Type::Slider;
  regionLeft.min = 0.0;
  regionLeft.max = 1.0;
  regionLeft.step = 0.01;
  regionLeft.sameLine = false;
  regionLeft.widthWeight = 0.5f;
  schema.push_back(regionLeft);
  InspectorControl regionRight;
  regionRight.propertyId = "regionRight";
  regionRight.label = "Region right";
  regionRight.controlType = InspectorControl::Type::Slider;
  regionRight.min = 0.0;
  regionRight.max = 1.0;
  regionRight.step = 0.01;
  regionRight.sameLine = true;
  regionRight.widthWeight = 0.5f;
  schema.push_back(regionRight);
  InspectorControl regionTop;
  regionTop.propertyId = "regionTop";
  regionTop.label = "Region top";
  regionTop.controlType = InspectorControl::Type::Slider;
  regionTop.min = 0.0;
  regionTop.max = 1.0;
  regionTop.step = 0.01;
  regionTop.sameLine = false;
  regionTop.widthWeight = 0.5f;
  schema.push_back(regionTop);
  InspectorControl regionBottom;
  regionBottom.propertyId = "regionBottom";
  regionBottom.label = "Region bottom";
  regionBottom.controlType = InspectorControl::Type::Slider;
  regionBottom.min = 0.0;
  regionBottom.max = 1.0;
  regionBottom.step = 0.01;
  regionBottom.sameLine = true;
  regionBottom.widthWeight = 0.5f;
  schema.push_back(regionBottom);
  InspectorControl regionLockCtrl;
  regionLockCtrl.propertyId = "regionLock";
  regionLockCtrl.label = "Region lock";
  regionLockCtrl.controlType = InspectorControl::Type::Toggle;
  schema.push_back(regionLockCtrl);

  // Pseudo-control used by TouchpadMixerEditorComponent to show a relayout
  // button. It is not bound directly to a config property.
  InspectorControl relayoutCtrl;
  relayoutCtrl.propertyId = "relayoutRegion";
  relayoutCtrl.label = "Quick relayout";
  relayoutCtrl.controlType = InspectorControl::Type::Button;
  schema.push_back(relayoutCtrl);
  return schema;
}

InspectorSchema TouchpadMixerDefinition::getSchema(TouchpadType type) {
  InspectorSchema schema;
  for (const auto &c : getCommonLayoutHeader())
    schema.push_back(c);
  schema.push_back(
      MappingDefinition::createSeparator("", juce::Justification::centred));

  if (type == TouchpadType::DrumPad) {
    // Drum Pad / Harmonic Grid controls
    InspectorControl rowsCtrl;
    rowsCtrl.propertyId = "drumPadRows";
    rowsCtrl.label = "Rows";
    rowsCtrl.controlType = InspectorControl::Type::Slider;
    rowsCtrl.min = 1.0;
    rowsCtrl.max = 8.0;
    rowsCtrl.step = 1.0;
    rowsCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(rowsCtrl);

    InspectorControl colsCtrl;
    colsCtrl.propertyId = "drumPadColumns";
    colsCtrl.label = "Columns";
    colsCtrl.controlType = InspectorControl::Type::Slider;
    colsCtrl.min = 1.0;
    colsCtrl.max = 16.0;
    colsCtrl.step = 1.0;
    colsCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(colsCtrl);

    InspectorControl noteStartCtrl;
    noteStartCtrl.propertyId = "drumPadMidiNoteStart";
    noteStartCtrl.label = "MIDI note start";
    noteStartCtrl.controlType = InspectorControl::Type::Slider;
    noteStartCtrl.min = 0.0;
    noteStartCtrl.max = 127.0;
    noteStartCtrl.step = 1.0;
    noteStartCtrl.valueFormat = InspectorControl::Format::NoteName;
    schema.push_back(noteStartCtrl);

    InspectorControl baseVelCtrl;
    baseVelCtrl.propertyId = "drumPadBaseVelocity";
    baseVelCtrl.label = "Base velocity";
    baseVelCtrl.controlType = InspectorControl::Type::Slider;
    baseVelCtrl.min = 1.0;
    baseVelCtrl.max = 127.0;
    baseVelCtrl.step = 1.0;
    baseVelCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(baseVelCtrl);

    InspectorControl velRandCtrl;
    velRandCtrl.propertyId = "drumPadVelocityRandom";
    velRandCtrl.label = "Velocity random";
    velRandCtrl.controlType = InspectorControl::Type::Slider;
    velRandCtrl.min = 0.0;
    velRandCtrl.max = 127.0;
    velRandCtrl.step = 1.0;
    velRandCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(velRandCtrl);

    // Note layout / Grid mode
    InspectorControl layoutModeCtrl;
    layoutModeCtrl.propertyId = "drumPadLayoutMode";
    layoutModeCtrl.label = "Note layout";
    layoutModeCtrl.controlType = InspectorControl::Type::ComboBox;
    layoutModeCtrl.options[1] = "Classic";
    layoutModeCtrl.options[2] = "Harmonic";
    schema.push_back(layoutModeCtrl);

    // Harmonic parameters (used when layoutMode == Harmonic)
    InspectorControl intervalCtrl;
    intervalCtrl.propertyId = "harmonicRowInterval";
    intervalCtrl.label = "Row interval (semitones)";
    intervalCtrl.controlType = InspectorControl::Type::Slider;
    intervalCtrl.min = -12.0;
    intervalCtrl.max = 12.0;
    intervalCtrl.step = 1.0;
    intervalCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(intervalCtrl);

    InspectorControl scaleFilterCtrl;
    scaleFilterCtrl.propertyId = "harmonicUseScaleFilter";
    scaleFilterCtrl.label = "Use scale filter";
    scaleFilterCtrl.controlType = InspectorControl::Type::Toggle;
    schema.push_back(scaleFilterCtrl);

    for (const auto &c : getCommonLayoutControls())
      schema.push_back(c);

    return schema;
  } else if (type == TouchpadType::ChordPad) {
    // Chord Pad controls
    InspectorControl rowsCtrl;
    rowsCtrl.propertyId = "drumPadRows";
    rowsCtrl.label = "Rows";
    rowsCtrl.controlType = InspectorControl::Type::Slider;
    rowsCtrl.min = 1.0;
    rowsCtrl.max = 8.0;
    rowsCtrl.step = 1.0;
    rowsCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(rowsCtrl);

    InspectorControl colsCtrl;
    colsCtrl.propertyId = "drumPadColumns";
    colsCtrl.label = "Columns";
    colsCtrl.controlType = InspectorControl::Type::Slider;
    colsCtrl.min = 1.0;
    colsCtrl.max = 16.0;
    colsCtrl.step = 1.0;
    colsCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(colsCtrl);

    InspectorControl rootCtrl;
    rootCtrl.propertyId = "drumPadMidiNoteStart";
    rootCtrl.label = "Base root note";
    rootCtrl.controlType = InspectorControl::Type::Slider;
    rootCtrl.min = 0.0;
    rootCtrl.max = 127.0;
    rootCtrl.step = 1.0;
    rootCtrl.valueFormat = InspectorControl::Format::NoteName;
    schema.push_back(rootCtrl);

    InspectorControl presetCtrl;
    presetCtrl.propertyId = "chordPadPreset";
    presetCtrl.label = "Preset";
    presetCtrl.controlType = InspectorControl::Type::Slider;
    presetCtrl.min = 0.0;
    presetCtrl.max = 2.0;
    presetCtrl.step = 1.0;
    presetCtrl.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(presetCtrl);

    InspectorControl latchCtrl;
    latchCtrl.propertyId = "chordPadLatchMode";
    latchCtrl.label = "Latch mode";
    latchCtrl.controlType = InspectorControl::Type::Toggle;
    schema.push_back(latchCtrl);

    for (const auto &c : getCommonLayoutControls())
      schema.push_back(c);

    return schema;
  }

  // Mixer controls
  InspectorControl qpCtrl;
  qpCtrl.propertyId = "quickPrecision";
  qpCtrl.label = "Quick / Precision";
  qpCtrl.controlType = InspectorControl::Type::ComboBox;
  qpCtrl.options[1] = "Quick";
  qpCtrl.options[2] = "Precision";
  schema.push_back(qpCtrl);

  InspectorControl arCtrl;
  arCtrl.propertyId = "absRel";
  arCtrl.label = "Absolute / Relative";
  arCtrl.controlType = InspectorControl::Type::ComboBox;
  arCtrl.options[1] = "Absolute";
  arCtrl.options[2] = "Relative";
  schema.push_back(arCtrl);

  InspectorControl lfCtrl;
  lfCtrl.propertyId = "lockFree";
  lfCtrl.label = "Lock / Free";
  lfCtrl.controlType = InspectorControl::Type::ComboBox;
  lfCtrl.options[1] = "Lock";
  lfCtrl.options[2] = "Free";
  schema.push_back(lfCtrl);

  schema.push_back(
      MappingDefinition::createSeparator("", juce::Justification::centred));

  InspectorControl numFadersCtrl;
  numFadersCtrl.propertyId = "numFaders";
  numFadersCtrl.label = "Num faders";
  numFadersCtrl.controlType = InspectorControl::Type::Slider;
  numFadersCtrl.min = 1.0;
  numFadersCtrl.max = 32.0;
  numFadersCtrl.step = 1.0;
  numFadersCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(numFadersCtrl);

  InspectorControl ccStartCtrl;
  ccStartCtrl.propertyId = "ccStart";
  ccStartCtrl.label = "CC start";
  ccStartCtrl.controlType = InspectorControl::Type::Slider;
  ccStartCtrl.min = 0.0;
  ccStartCtrl.max = 127.0;
  ccStartCtrl.step = 1.0;
  ccStartCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(ccStartCtrl);

  schema.push_back(
      MappingDefinition::createSeparator("", juce::Justification::centred));

  InspectorControl inMinCtrl;
  inMinCtrl.propertyId = "inputMin";
  inMinCtrl.label = "Input Y min";
  inMinCtrl.controlType = InspectorControl::Type::Slider;
  inMinCtrl.min = 0.0;
  inMinCtrl.max = 1.0;
  inMinCtrl.step = 0.01;
  inMinCtrl.sameLine = false;
  inMinCtrl.widthWeight = 0.5f;
  schema.push_back(inMinCtrl);

  InspectorControl inMaxCtrl;
  inMaxCtrl.propertyId = "inputMax";
  inMaxCtrl.label = "Input Y max";
  inMaxCtrl.controlType = InspectorControl::Type::Slider;
  inMaxCtrl.min = 0.0;
  inMaxCtrl.max = 1.0;
  inMaxCtrl.step = 0.01;
  inMaxCtrl.sameLine = true;
  inMaxCtrl.widthWeight = 0.5f;
  schema.push_back(inMaxCtrl);

  InspectorControl outMinCtrl;
  outMinCtrl.propertyId = "outputMin";
  outMinCtrl.label = "Output min";
  outMinCtrl.controlType = InspectorControl::Type::Slider;
  outMinCtrl.min = 0.0;
  outMinCtrl.max = 127.0;
  outMinCtrl.step = 1.0;
  outMinCtrl.sameLine = false;
  outMinCtrl.widthWeight = 0.5f;
  schema.push_back(outMinCtrl);

  InspectorControl outMaxCtrl;
  outMaxCtrl.propertyId = "outputMax";
  outMaxCtrl.label = "Output max";
  outMaxCtrl.controlType = InspectorControl::Type::Slider;
  outMaxCtrl.min = 0.0;
  outMaxCtrl.max = 127.0;
  outMaxCtrl.step = 1.0;
  outMaxCtrl.sameLine = true;
  outMaxCtrl.widthWeight = 0.5f;
  schema.push_back(outMaxCtrl);

  schema.push_back(
      MappingDefinition::createSeparator("", juce::Justification::centred));

  InspectorControl muteCtrl;
  muteCtrl.propertyId = "muteButtonsEnabled";
  muteCtrl.label = "Mute buttons";
  muteCtrl.controlType = InspectorControl::Type::Toggle;
  schema.push_back(muteCtrl);

  for (const auto &c : getCommonLayoutControls())
    schema.push_back(c);

  return schema;
}
