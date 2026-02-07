#include "TouchpadMixerDefinition.h"

InspectorSchema TouchpadMixerDefinition::getSchema() {
  InspectorSchema schema;

  InspectorControl nameCtrl;
  nameCtrl.propertyId = "name";
  nameCtrl.label = "Name";
  nameCtrl.controlType = InspectorControl::Type::TextEditor;
  schema.push_back(nameCtrl);

  InspectorControl layerCtrl;
  layerCtrl.propertyId = "layerId";
  layerCtrl.label = "Layer";
  layerCtrl.controlType = InspectorControl::Type::ComboBox;
  layerCtrl.options[1] = "Base";
  for (int i = 1; i <= 8; ++i)
    layerCtrl.options[i + 1] = "Layer " + juce::String(i);
  schema.push_back(layerCtrl);

  schema.push_back(MappingDefinition::createSeparator("", juce::Justification::centred));

  // Interaction matrix: each on its own row
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

  schema.push_back(MappingDefinition::createSeparator("", juce::Justification::centred));

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

  InspectorControl chCtrl;
  chCtrl.propertyId = "midiChannel";
  chCtrl.label = "Channel";
  chCtrl.controlType = InspectorControl::Type::Slider;
  chCtrl.min = 1.0;
  chCtrl.max = 16.0;
  chCtrl.step = 1.0;
  chCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(chCtrl);

  schema.push_back(MappingDefinition::createSeparator("", juce::Justification::centred));

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

  schema.push_back(MappingDefinition::createSeparator("", juce::Justification::centred));

  InspectorControl muteCtrl;
  muteCtrl.propertyId = "muteButtonsEnabled";
  muteCtrl.label = "Mute buttons";
  muteCtrl.controlType = InspectorControl::Type::Toggle;
  schema.push_back(muteCtrl);

  return schema;
}
