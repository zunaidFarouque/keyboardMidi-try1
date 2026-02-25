#include "SettingsDefinition.h"

InspectorSchema SettingsDefinition::getSchema() {
  InspectorSchema schema;

  // --- Pitch section ---
  schema.push_back(
      MappingDefinition::createSeparator("Pitch",
                                         juce::Justification::centredLeft));
  InspectorControl pbCtrl;
  pbCtrl.propertyId = "pitchBendRange";
  pbCtrl.label = "Global Pitch Bend Range (+/- semitones)";
  pbCtrl.controlType = InspectorControl::Type::Slider;
  pbCtrl.min = 1.0;
  pbCtrl.max = 96.0;
  pbCtrl.step = 1.0;
  pbCtrl.valueFormat = InspectorControl::Format::Integer;
  schema.push_back(pbCtrl);

  // --- Delay MIDI (mapping helper) section ---
  schema.push_back(
      MappingDefinition::createSeparator("Delay MIDI (Mapping Helper)",
                                         juce::Justification::centredLeft));
  {
    InspectorControl delayToggle;
    delayToggle.propertyId = "delayMidiEnabled";
    delayToggle.label = "Delay MIDI message for mapping";
    delayToggle.controlType = InspectorControl::Type::Toggle;
    schema.push_back(delayToggle);

    InspectorControl delaySeconds;
    delaySeconds.propertyId = "delayMidiSeconds";
    delaySeconds.label = "Delay amount (seconds)";
    delaySeconds.controlType = InspectorControl::Type::Slider;
    delaySeconds.min = 1.0;
    delaySeconds.max = 10.0;
    delaySeconds.step = 1.0;
    delaySeconds.suffix = "s";
    delaySeconds.valueFormat = InspectorControl::Format::Integer;
    schema.push_back(delaySeconds);
  }

  // --- Visualizer section ---
  schema.push_back(
      MappingDefinition::createSeparator("Visualizer",
                                         juce::Justification::centredLeft));
  {
    InspectorControl xOpacity;
    xOpacity.propertyId = "visualizerXOpacityPercent";
    xOpacity.label = "Touchpad X bands opacity";
    xOpacity.controlType = InspectorControl::Type::Slider;
    xOpacity.min = 0.0;
    xOpacity.max = 100.0;
    xOpacity.step = 1.0;
    xOpacity.suffix = "%";
    schema.push_back(xOpacity);

    InspectorControl yOpacity;
    yOpacity.propertyId = "visualizerYOpacityPercent";
    yOpacity.label = "Touchpad Y bands opacity";
    yOpacity.controlType = InspectorControl::Type::Slider;
    yOpacity.min = 0.0;
    yOpacity.max = 100.0;
    yOpacity.step = 1.0;
    yOpacity.suffix = "%";
    schema.push_back(yOpacity);

    InspectorControl miniToggle;
    miniToggle.propertyId = "showTouchpadVisualizerInMiniWindow";
    miniToggle.label = "Show touchpad visualizer in mini window";
    miniToggle.controlType = InspectorControl::Type::Toggle;
    schema.push_back(miniToggle);

    InspectorControl hideCursor;
    hideCursor.propertyId = "hideCursorInPerformanceMode";
    hideCursor.label = "Hide cursor in performance mode";
    hideCursor.controlType = InspectorControl::Type::Toggle;
    schema.push_back(hideCursor);
  }

  // --- Performance / Studio section ---
  schema.push_back(
      MappingDefinition::createSeparator("Performance & Studio",
                                         juce::Justification::centredLeft));
  {
    InspectorControl studioMode;
    studioMode.propertyId = "studioMode";
    studioMode.label = "Studio Mode (Multi-Device Support)";
    studioMode.controlType = InspectorControl::Type::Toggle;
    schema.push_back(studioMode);

    InspectorControl capFps;
    capFps.propertyId = "capWindowRefresh30Fps";
    capFps.label = "Cap window refresh at 30 FPS";
    capFps.controlType = InspectorControl::Type::Toggle;
    schema.push_back(capFps);
  }

  // --- UI & Layout section ---
  schema.push_back(
      MappingDefinition::createSeparator("UI & Layout",
                                         juce::Justification::centredLeft));
  {
    InspectorControl rememberUi;
    rememberUi.propertyId = "rememberUiState";
    rememberUi.label = "Remember UI layout (windows, tabs, selections)";
    rememberUi.controlType = InspectorControl::Type::Toggle;
    schema.push_back(rememberUi);
  }

  // --- Debugging section ---
  schema.push_back(
      MappingDefinition::createSeparator("Debugging",
                                         juce::Justification::centredLeft));
  {
    InspectorControl debugMode;
    debugMode.propertyId = "debugModeEnabled";
    debugMode.label = "Enable Debug mode (write crash logs)";
    debugMode.controlType = InspectorControl::Type::Toggle;
    schema.push_back(debugMode);
  }

  return schema;
}

