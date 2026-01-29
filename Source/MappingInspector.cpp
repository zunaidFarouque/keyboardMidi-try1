#include "MappingInspector.h"
#include "DeviceManager.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"

MappingInspector::MappingInspector(juce::UndoManager &undoMgr,
                                   DeviceManager &deviceMgr)
    : undoManager(undoMgr), deviceManager(deviceMgr) {
  // Setup Labels
  typeLabel.setText("Type:", juce::dontSendNotification);
  typeLabel.attachToComponent(&typeSelector, true);
  addAndMakeVisible(typeLabel);
  addAndMakeVisible(typeSelector);

  channelLabel.setText("Channel:", juce::dontSendNotification);
  channelLabel.attachToComponent(&channelSlider, true);
  addAndMakeVisible(channelLabel);
  addAndMakeVisible(channelSlider);

  data1Label.setText("Data1:", juce::dontSendNotification);
  data1Label.attachToComponent(&data1Slider, true);
  addAndMakeVisible(data1Label);
  addAndMakeVisible(data1Slider);

  data2Label.setText("Data2:", juce::dontSendNotification);
  data2Label.attachToComponent(&data2Slider, true);
  addAndMakeVisible(data2Label);
  addAndMakeVisible(data2Slider);

  randVelLabel.setText("Velocity Random:", juce::dontSendNotification);
  randVelLabel.attachToComponent(&randVelSlider, true);
  addAndMakeVisible(randVelLabel);
  addAndMakeVisible(randVelSlider);
  randVelSlider.setVisible(false);
  randVelLabel.setVisible(false);

  commandLabel.setText("Command:", juce::dontSendNotification);
  commandLabel.attachToComponent(&commandSelector, true);
  addAndMakeVisible(commandLabel);
  addAndMakeVisible(commandSelector);
  commandSelector.setVisible(false);
  commandLabel.setVisible(false);

  targetLayerLabel.setText("Target Layer:", juce::dontSendNotification);
  targetLayerLabel.attachToComponent(&targetLayerSelector, true);
  addAndMakeVisible(targetLayerLabel);
  addAndMakeVisible(targetLayerSelector);
  targetLayerSelector.setVisible(false);
  targetLayerLabel.setVisible(false);
  for (int i = 0; i <= 8; ++i)
    targetLayerSelector.addItem(
        i == 0 ? "0: Base" : (juce::String(i) + ": Layer " + juce::String(i)),
        i + 1);
  targetLayerSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;
    int layerId = targetLayerSelector.getSelectedId() - 1;
    if (layerId < 0 || layerId > 8)
      return;
    undoManager.beginNewTransaction("Change Target Layer");
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("data2", layerId, &undoManager);
    }
  };

  aliasLabel.setText("Alias:", juce::dontSendNotification);
  aliasLabel.attachToComponent(&aliasSelector, true);
  addAndMakeVisible(aliasLabel);
  addAndMakeVisible(aliasSelector);

  // Setup ADSR controls (initially hidden)
  attackLabel.setText("A:", juce::dontSendNotification);
  attackLabel.attachToComponent(&attackSlider, true);
  addAndMakeVisible(attackLabel);
  addAndMakeVisible(attackSlider);
  attackSlider.setRange(0, 5000, 1);
  attackSlider.setTextValueSuffix(" ms");
  attackSlider.setVisible(false);
  attackLabel.setVisible(false);

  decayLabel.setText("D:", juce::dontSendNotification);
  decayLabel.attachToComponent(&decaySlider, true);
  addAndMakeVisible(decayLabel);
  addAndMakeVisible(decaySlider);
  decaySlider.setRange(0, 5000, 1);
  decaySlider.setTextValueSuffix(" ms");
  decaySlider.setVisible(false);
  decayLabel.setVisible(false);

  sustainLabel.setText("S:", juce::dontSendNotification);
  sustainLabel.attachToComponent(&sustainSlider, true);
  addAndMakeVisible(sustainLabel);
  addAndMakeVisible(sustainSlider);
  sustainSlider.setRange(0, 127, 1);
  sustainSlider.setTextValueSuffix("");
  sustainSlider.setVisible(false);
  sustainLabel.setVisible(false);

  releaseLabel.setText("R:", juce::dontSendNotification);
  releaseLabel.attachToComponent(&releaseSlider, true);
  addAndMakeVisible(releaseLabel);
  addAndMakeVisible(releaseSlider);
  releaseSlider.setRange(0, 5000, 1);
  releaseSlider.setTextValueSuffix(" ms");
  releaseSlider.setVisible(false);
  releaseLabel.setVisible(false);

  envTargetLabel.setText("Target:", juce::dontSendNotification);
  envTargetLabel.attachToComponent(&envTargetSelector, true);
  addAndMakeVisible(envTargetLabel);
  addAndMakeVisible(envTargetSelector);
  envTargetSelector.addItem("CC", 1);
  envTargetSelector.addItem("Pitch Bend", 2);
  envTargetSelector.addItem("Smart Scale Bend", 3);
  envTargetSelector.setVisible(false);
  envTargetLabel.setVisible(false);

  // ADSR slider callbacks
  attackSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (attackSlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change ADSR Attack");
    int value = static_cast<int>(attackSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("adsrAttack", value, &undoManager);
    }
  };

  decaySlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (decaySlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change ADSR Decay");
    int value = static_cast<int>(decaySlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("adsrDecay", value, &undoManager);
    }
  };

  sustainSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (sustainSlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change ADSR Sustain");
    int value = static_cast<int>(sustainSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("adsrSustain", value, &undoManager);
    }
  };

  releaseSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (releaseSlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change ADSR Release");
    int value = static_cast<int>(releaseSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("adsrRelease", value, &undoManager);
    }
  };

  envTargetSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;
    if (envTargetSelector.getSelectedId() < 1 ||
        envTargetSelector.getSelectedId() > 3)
      return;
    undoManager.beginNewTransaction("Change Envelope Target");
    juce::String targetStr;
    if (envTargetSelector.getSelectedId() == 1)
      targetStr = "CC";
    else if (envTargetSelector.getSelectedId() == 2)
      targetStr = "PitchBend";
    else if (envTargetSelector.getSelectedId() == 3)
      targetStr = "SmartScaleBend";
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("adsrTarget", targetStr, &undoManager);
    }
    // Update visibility when target changes
    updateControlsFromSelection();
  };

  // Setup Pitch Bend musical controls (initially hidden)
  pbShiftLabel.setText("PB Shift:", juce::dontSendNotification);
  pbShiftLabel.attachToComponent(&pbShiftSlider, true);
  addAndMakeVisible(pbShiftLabel);
  addAndMakeVisible(pbShiftSlider);
  pbShiftSlider.setRange(-24, 24, 1);
  pbShiftSlider.setTextValueSuffix(" semitones");
  pbShiftSlider.setVisible(false);
  pbShiftLabel.setVisible(false);

  // "Uses Global Range" label
  pbGlobalRangeLabel.setText("Uses Global Range", juce::dontSendNotification);
  pbGlobalRangeLabel.setColour(juce::Label::textColourId,
                               juce::Colours::lightgrey);
  addAndMakeVisible(pbGlobalRangeLabel);
  pbGlobalRangeLabel.setVisible(false);

  // Setup Smart Scale Bend controls (initially hidden)
  smartStepLabel.setText("Scale Steps:", juce::dontSendNotification);
  smartStepLabel.attachToComponent(&smartStepSlider, true);
  addAndMakeVisible(smartStepLabel);
  addAndMakeVisible(smartStepSlider);
  smartStepSlider.setRange(-7, 7, 1);
  smartStepSlider.setTextValueSuffix(" steps");
  smartStepSlider.setVisible(false);
  smartStepLabel.setVisible(false);

  // Smart Step slider callback
  smartStepSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (smartStepSlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change Smart Step Shift");
    int value = static_cast<int>(smartStepSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("smartStepShift", value, &undoManager);
    }
  };

  // Pitch Bend slider callbacks
  pbShiftSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;
    if (pbShiftSlider.getTextValueSuffix().contains("---"))
      return;
    undoManager.beginNewTransaction("Change PB Shift");
    int value = static_cast<int>(pbShiftSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid()) {
        tree.setProperty("pbShift", value, &undoManager);
        // Note: data2 will be recalculated by InputProcessor when it rebuilds
        // the map from tree, using the global range from SettingsManager
      }
    }
  };

  // Setup Alias Selector
  refreshAliasSelector();
  aliasSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;

    // Don't update if showing mixed value (no selection or multiple different
    // values)
    if (aliasSelector.getSelectedId() == -1)
      return;

    juce::String aliasName = aliasSelector.getText();

    // Phase 46.5: deviceHash is the single source of truth.
    // Compute the alias hash from the selected name.
    uintptr_t newHash = 0;
    if (aliasName != "Global (All Devices)" && aliasName != "Any / Master" &&
        aliasName != "Unassigned" && !aliasName.isEmpty()) {
      newHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
    }

    undoManager.beginNewTransaction("Change Alias");
    for (auto &tree : selectedTrees) {
      if (!tree.isValid())
        continue;

      tree.setProperty(
          "deviceHash",
          juce::String::toHexString((juce::int64)newHash).toUpperCase(),
          &undoManager);

      // Optional: keep human-readable name in XML for debugging.
      // Do NOT read from this as the primary source (deviceHash is
      // authoritative).
      if (newHash == 0)
        tree.setProperty("inputAlias", "", &undoManager);
      else
        tree.setProperty("inputAlias", aliasName, &undoManager);
    }
  };

  // Setup Type Selector
  typeSelector.addItem("Note", 1);
  typeSelector.addItem("CC", 2);
  typeSelector.addItem("Command", 3);
  typeSelector.addItem("Macro", 4);
  typeSelector.addItem("Envelope", 5);
  typeSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;

    juce::String typeStr;
    int selectedId = typeSelector.getSelectedId();
    if (selectedId == 1)
      typeStr = "Note";
    else if (selectedId == 2)
      typeStr = "CC";
    else if (selectedId == 3)
      typeStr = "Command";
    else if (selectedId == 4)
      typeStr = "Macro";
    else if (selectedId == 5)
      typeStr = "Envelope";
    else
      return; // Invalid selection

    undoManager.beginNewTransaction("Change Type");
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("type", typeStr, &undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Setup Command Selector (IDs 1-13 map to CommandID 0-12)
  commandSelector.addItem("Sustain (Momentary)", 1);
  commandSelector.addItem("Sustain (Toggle)", 2);
  commandSelector.addItem("Sustain (Inverse)", 3);
  commandSelector.addItem("Latch (Toggle)", 4);
  commandSelector.addItem("Panic (All Off)", 5);
  commandSelector.addItem("Panic (Latched Only)", 6);
  commandSelector.addItem("Global Pitch +1", 7);
  commandSelector.addItem("Global Pitch -1", 8);
  commandSelector.addItem("Global Mode +1", 9);
  commandSelector.addItem("Global Mode -1", 10);
  // Phase 41: Layer commands
  commandSelector.addItem("Layer Momentary (Hold)", 11);
  commandSelector.addItem("Layer Toggle", 12);
  commandSelector.addItem("Layer Solo", 13);
  commandSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;

    int selectedId = commandSelector.getSelectedId();
    if (selectedId < 1 || selectedId > 13)
      return;

    // Map ComboBox ID (1-13) to CommandID enum value (0-12)
    int cmdValue = selectedId - 1;

    undoManager.beginNewTransaction("Change Command");
    for (auto &tree : selectedTrees) {
      if (tree.isValid()) {
        tree.setProperty("data1", cmdValue, &undoManager);
        // Phase 41: For layer commands, initialize data2 to 0 if not set
        if (cmdValue >= 10 && cmdValue <= 12) {
          if (!tree.hasProperty("data2")) {
            tree.setProperty("data2", 0, &undoManager);
          }
        }
      }
    }

    updateVisibility();
    updateControlsFromSelection();
  };

  // Setup Channel Slider
  channelSlider.setRange(1, 16, 1);
  channelSlider.setTextValueSuffix("");
  channelSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;

    // Don't update if showing mixed value (suffix contains "---")
    if (channelSlider.getTextValueSuffix().contains("---"))
      return;

    undoManager.beginNewTransaction("Change Channel");
    int value = static_cast<int>(channelSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("channel", value, &undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Setup Data1 Slider
  data1Slider.setRange(0, 127, 1);
  data1Slider.setTextValueSuffix("");
  data1Slider.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 80, 20);
  data1Slider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;

    // Don't update if showing mixed value (suffix contains "---")
    if (data1Slider.getTextValueSuffix().contains("---"))
      return;

    // Phase 41: Check if this is a layer command (data1Slider is showing layer
    // ID)
    bool isLayerCommand = false;
    if (allTreesHaveSameValue("type") &&
        getCommonValue("type").toString() == "Command") {
      if (allTreesHaveSameValue("data1")) {
        int cmdId = static_cast<int>(getCommonValue("data1"));
        isLayerCommand = (cmdId >= 10 && cmdId <= 12);
      }
    }

    undoManager.beginNewTransaction(isLayerCommand ? "Change Layer ID"
                                                   : "Change Data1");
    int value = static_cast<int>(data1Slider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid()) {
        if (isLayerCommand) {
          // Phase 41: For layer commands, save layer ID to data2
          tree.setProperty("data2", value, &undoManager);
        } else {
          tree.setProperty("data1", value, &undoManager);
        }
      }
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Setup Data2 Slider
  data2Slider.setRange(0, 127, 1);
  data2Slider.setTextValueSuffix("");
  data2Slider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;

    // Don't update if showing mixed value (suffix contains "---")
    if (data2Slider.getTextValueSuffix().contains("---"))
      return;

    undoManager.beginNewTransaction("Change Data2");
    int value = static_cast<int>(data2Slider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("data2", value, &undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Setup Velocity Random Slider (only for Note type)
  randVelSlider.setRange(0, 64, 1);
  randVelSlider.setTextValueSuffix("");
  randVelSlider.onValueChange = [this] {
    if (selectedTrees.empty())
      return;

    // Don't update if showing mixed value
    if (randVelSlider.getTextValueSuffix().contains("---"))
      return;

    undoManager.beginNewTransaction("Change Velocity Random");
    int value = static_cast<int>(randVelSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("velRandom", value, &undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Register with DeviceManager for changes
  deviceManager.addChangeListener(this);

  // Initially disable all controls
  enableControls(false);
}

MappingInspector::~MappingInspector() {
  deviceManager.removeChangeListener(this);

  // Remove listeners from selection
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.removeListener(this);
  }
}

void MappingInspector::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &deviceManager) {
    // Device alias configuration changed, refresh the selector
    refreshAliasSelector();
    updateControlsFromSelection();
  }
}

void MappingInspector::refreshAliasSelector() {
  aliasSelector.clear(juce::dontSendNotification);

  // Add "Global (All Devices)" option (hash 0)
  aliasSelector.addItem("Global (All Devices)", 1);

  // Add all aliases
  auto aliases = deviceManager.getAllAliasNames();
  for (int i = 0; i < aliases.size(); ++i) {
    aliasSelector.addItem(aliases[i], i + 2); // Start IDs from 2
  }
}

void MappingInspector::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));

  if (selectedTrees.empty()) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No selection", getLocalBounds(), juce::Justification::centred,
               true);
  }
}

void MappingInspector::resized() {
  auto area = getLocalBounds().reduced(10);
  int labelWidth = 80;
  int controlHeight = 25;
  int spacing = 10;
  int topPadding = 10;
  int leftMargin = area.getX() + labelWidth;
  int width = area.getWidth() - labelWidth;

  int y = topPadding;

  typeSelector.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  if (channelSlider.isVisible()) {
    channelSlider.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }

  aliasSelector.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  if (commandSelector.isVisible()) {
    commandSelector.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }
  if (targetLayerSelector.isVisible()) {
    targetLayerSelector.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }

  if (data1Slider.isVisible()) {
    data1Slider.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }

  if (data2Slider.isVisible()) {
    data2Slider.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }

  if (randVelSlider.isVisible()) {
    randVelSlider.setBounds(leftMargin, y, width, controlHeight);
    y += controlHeight + spacing;
  }

  // ADSR controls (for Envelope type) - layout in a 2x2 grid
  if (attackSlider.isVisible()) {
    int adsrY = y;
    int adsrWidth = (width - spacing) / 2;

    attackSlider.setBounds(leftMargin, adsrY, adsrWidth, controlHeight);
    decaySlider.setBounds(leftMargin + adsrWidth + spacing, adsrY, adsrWidth,
                          controlHeight);
    adsrY += controlHeight + spacing;

    sustainSlider.setBounds(leftMargin, adsrY, adsrWidth, controlHeight);
    releaseSlider.setBounds(leftMargin + adsrWidth + spacing, adsrY, adsrWidth,
                            controlHeight);
    adsrY += controlHeight + spacing;

    envTargetSelector.setBounds(leftMargin, adsrY, width, controlHeight);
    adsrY += controlHeight + spacing;

    // Pitch Bend controls (if visible)
    if (pbShiftSlider.isVisible()) {
      pbShiftSlider.setBounds(leftMargin, adsrY, width, controlHeight);
      adsrY += controlHeight + spacing;
      pbGlobalRangeLabel.setBounds(leftMargin, adsrY, width, controlHeight);
      adsrY += controlHeight + spacing;
    }

    // Smart Scale Bend controls (if visible)
    if (smartStepSlider.isVisible()) {
      smartStepSlider.setBounds(leftMargin, adsrY, width, controlHeight);
      adsrY += controlHeight + spacing;
    }

    y = adsrY;
  }

  y += 10; // Bottom padding

  // Set component size based on calculated height
  setSize(getWidth(), y);
}

int MappingInspector::getRequiredHeight() const {
  int controlHeight = 25;
  int spacing = 10;
  int topPadding = 10;
  int bottomPadding = 10;

  // 5 controls: Type, Channel, Alias, Data1, Data2
  int numControls = 5;

  return topPadding + (numControls * (controlHeight + spacing)) + bottomPadding;
}

void MappingInspector::setSelection(
    const std::vector<juce::ValueTree> &selection) {
  isUpdatingFromTree = true;
  // Remove listeners from old selection
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.removeListener(this);
  }

  // Cache new selection
  selectedTrees = selection;

  // Add listeners to new selection
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.addListener(this);
  }

  // Update UI
  updateControlsFromSelection();
  isUpdatingFromTree = false;
}

void MappingInspector::updateControlsFromSelection() {
  if (selectedTrees.empty()) {
    enableControls(false);
    return;
  }

  enableControls(true);

  // Update Type Selector and configure Data1 slider based on type
  juce::String typeStr;
  if (allTreesHaveSameValue("type")) {
    typeStr = getCommonValue("type").toString();
    if (typeStr == "Note")
      typeSelector.setSelectedId(1, juce::dontSendNotification);
    else if (typeStr == "CC")
      typeSelector.setSelectedId(2, juce::dontSendNotification);
    else if (typeStr == "Command")
      typeSelector.setSelectedId(3, juce::dontSendNotification);
    else if (typeStr == "Macro")
      typeSelector.setSelectedId(4, juce::dontSendNotification);
    else if (typeStr == "Envelope")
      typeSelector.setSelectedId(5, juce::dontSendNotification);
    else
      typeSelector.setSelectedId(-1, juce::dontSendNotification);
  } else {
    typeSelector.setSelectedId(-1, juce::dontSendNotification);
    typeStr = ""; // Mixed types
  }

  // Show/hide controls based on type
  bool isCommand = (typeStr == "Command");
  bool isNoteOrCC =
      (typeStr == "Note" || typeStr == "CC" || typeStr == "Macro");
  bool isNote = (typeStr == "Note");
  bool isEnvelope = (typeStr == "Envelope");

  channelSlider.setVisible(isNoteOrCC || isEnvelope);
  channelLabel.setVisible(isNoteOrCC || isEnvelope);
  // data1Slider visibility for Envelope will be set later based on target
  if (!isEnvelope) {
    data1Slider.setVisible(isNoteOrCC);
    data1Label.setVisible(isNoteOrCC);
  }
  data2Slider.setVisible(isNoteOrCC || isEnvelope);
  data2Label.setVisible(isNoteOrCC || isEnvelope);
  randVelSlider.setVisible(isNote);
  randVelLabel.setVisible(isNote);

  commandSelector.setVisible(isCommand);
  commandLabel.setVisible(isCommand);
  updateVisibility();

  // ADSR controls visibility
  attackSlider.setVisible(isEnvelope);
  attackLabel.setVisible(isEnvelope);
  decaySlider.setVisible(isEnvelope);
  decayLabel.setVisible(isEnvelope);
  sustainSlider.setVisible(isEnvelope);
  sustainLabel.setVisible(isEnvelope);
  releaseSlider.setVisible(isEnvelope);
  releaseLabel.setVisible(isEnvelope);
  envTargetSelector.setVisible(isEnvelope);
  envTargetLabel.setVisible(isEnvelope);

  // Pitch Bend controls visibility (only for Envelope with PitchBend target)
  juce::String target = allTreesHaveSameValue("adsrTarget")
                            ? getCommonValue("adsrTarget").toString()
                            : "";
  bool isPitchBend = (isEnvelope && target == "PitchBend");
  bool isSmartScaleBend = (isEnvelope && target == "SmartScaleBend");
  pbShiftSlider.setVisible(isPitchBend);
  pbShiftLabel.setVisible(isPitchBend);
  pbGlobalRangeLabel.setVisible(isPitchBend);

  // Smart Scale Bend controls visibility
  smartStepSlider.setVisible(isSmartScaleBend);
  smartStepLabel.setVisible(isSmartScaleBend);

  // Hide data1Slider and data2Slider for Pitch Bend and SmartScaleBend (use
  // musical controls instead)
  if (isPitchBend || isSmartScaleBend) {
    data1Slider.setVisible(false);
    data1Label.setVisible(false);
    data2Slider.setVisible(false);
    data2Label.setVisible(false);
  }

  // Configure Data1 slider based on type (only for Note/CC/Macro)
  if (typeStr == "Note") {
    data1Label.setText("Note:", juce::dontSendNotification);
    data1Slider.setRange(0, 127, 1);
    data1Slider.setEnabled(true);
    // Install smart lambda for note names
    data1Slider.textFromValueFunction = [](double val) {
      return MidiNoteUtilities::getMidiNoteName(static_cast<int>(val));
    };
    data1Slider.valueFromTextFunction = [](const juce::String &text) {
      return static_cast<double>(MidiNoteUtilities::getMidiNoteFromText(text));
    };
    data1Slider.updateText();
  } else if (typeStr == "CC") {
    data1Label.setText("CC Number:", juce::dontSendNotification);
    data1Slider.setRange(0, 127, 1);
    data1Slider.setEnabled(true);
    // Remove smart lambda, revert to default numeric display
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
  } else if (typeStr == "Macro") {
    data1Label.setText("Macro ID:", juce::dontSendNotification);
    data1Slider.setRange(0, 127, 1);
    data1Slider.setEnabled(true);
    // Use default numeric display for macros
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
  } else if (typeStr == "Envelope") {
    // For Envelope: data1 = CC number (if target is CC), data2 = peak value
    data1Label.setText("CC Number:", juce::dontSendNotification);
    data1Slider.setRange(0, 127, 1);
    data1Slider.setEnabled(true);
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
    data2Label.setText("Peak Value:", juce::dontSendNotification);
    data2Slider.setRange(0, 16383, 1); // Allow up to Pitch Bend range
    data2Slider.updateText();
  } else {
    // Reset data2Slider range for non-Envelope types
    if (data2Slider.getMaximum() > 127) {
      data2Slider.setRange(0, 127, 1);
      data2Slider.updateText();
    }
  }

  if (typeStr == "Command") {
    // Command type - update commandSelector from data1
    if (allTreesHaveSameValue("data1")) {
      int data1 = static_cast<int>(getCommonValue("data1"));
      // Map CommandID enum value (0-12) to ComboBox ID (1-13)
      if (data1 >= 0 && data1 <= 12) {
        commandSelector.setSelectedId(data1 + 1, juce::dontSendNotification);
        // Layer commands now use targetLayerSelector; keep data1Slider in its
        // default range and label for non-layer commands.
        if (data1 < 10 || data1 > 12) {
          data1Label.setText("Data1:", juce::dontSendNotification);
        }
      } else {
        commandSelector.setSelectedId(-1, juce::dontSendNotification);
      }
    } else {
      commandSelector.setSelectedId(-1, juce::dontSendNotification);
    }
  } else {
    // Mixed or unknown type - disable smart functions
    data1Label.setText("Data1:", juce::dontSendNotification);
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
  }

  // Update Channel Slider
  if (allTreesHaveSameValue("channel")) {
    int channel = static_cast<int>(getCommonValue("channel"));
    channelSlider.setValue(channel, juce::dontSendNotification);
    channelSlider.setTextValueSuffix("");
  } else {
    // Mixed value - set to middle and show "---"
    channelSlider.setValue(8, juce::dontSendNotification);
    channelSlider.setTextValueSuffix(" (---)");
  }

  // Update Alias Selector
  {
    juce::String aliasName = "Global (All Devices)"; // default

    // Phase 46.5: Source of truth is deviceHash (NOT inputAlias).
    if (allTreesHaveSameValue("deviceHash")) {
      juce::String hashStr = getCommonValue("deviceHash").toString();
      uintptr_t hash = 0;
      if (!hashStr.isEmpty())
        hash = static_cast<uintptr_t>(hashStr.getHexValue64());

      if (hash != 0) {
        aliasName = deviceManager.getAliasName(hash);
        if (aliasName == "Unknown")
          aliasName = "Global (All Devices)"; // fallback
      }
    } else {
      // Mixed selection: clear
      aliasSelector.setSelectedId(-1, juce::dontSendNotification);
      aliasName.clear();
    }

    // Set selection if we found a single alias.
    // Phase 46.3: match by displayed text (robust even if IDs/order change).
    if (!aliasName.isEmpty() && aliasName != "Unknown") {
      bool found = false;
      for (int i = 0; i < aliasSelector.getNumItems(); ++i) {
        if (aliasSelector.getItemText(i) == aliasName) {
          aliasSelector.setSelectedItemIndex(i, juce::dontSendNotification);
          found = true;
          break;
        }
      }
      if (!found) {
        aliasSelector.setSelectedItemIndex(0, juce::dontSendNotification);
      }
    } else {
      aliasSelector.setSelectedId(-1, juce::dontSendNotification);
    }
  }

  // Update Data1 Slider (or Layer ID slider for layer commands)
  if (typeStr == "Command" && allTreesHaveSameValue("data1")) {
    int cmdId = static_cast<int>(getCommonValue("data1"));
    if (cmdId >= 10 && cmdId <= 12) {
      // Phase 44: Layer command - sync Target Layer selector from data2
      if (allTreesHaveSameValue("data2")) {
        int layerId = static_cast<int>(getCommonValue("data2"));
        layerId = juce::jlimit(0, 8, layerId);
        targetLayerSelector.setSelectedId(layerId + 1,
                                          juce::dontSendNotification);
      } else {
        targetLayerSelector.setSelectedId(
            1, juce::dontSendNotification); // Base when mixed
      }
    } else {
      // Regular command - show data1
      int data1 = cmdId;
      data1Slider.setValue(data1, juce::dontSendNotification);
      data1Slider.setTextValueSuffix("");
    }
  } else if (allTreesHaveSameValue("data1")) {
    int data1 = static_cast<int>(getCommonValue("data1"));
    data1Slider.setValue(data1, juce::dontSendNotification);
    data1Slider.setTextValueSuffix("");
  } else {
    // Mixed value - set to middle and show "---"
    data1Slider.setValue(64, juce::dontSendNotification);
    data1Slider.setTextValueSuffix(" (---)");
  }

  // Update Data2 Slider
  if (allTreesHaveSameValue("data2")) {
    int data2 = static_cast<int>(getCommonValue("data2"));
    data2Slider.setValue(data2, juce::dontSendNotification);
    data2Slider.setTextValueSuffix("");
  } else {
    // Mixed value - set to middle and show "---"
    data2Slider.setValue(64, juce::dontSendNotification);
    data2Slider.setTextValueSuffix(" (---)");
  }

  // Update Velocity Random Slider (only for Note type)
  if (typeStr == "Note") {
    if (allTreesHaveSameValue("velRandom")) {
      int velRandom = static_cast<int>(getCommonValue("velRandom"));
      randVelSlider.setValue(velRandom, juce::dontSendNotification);
      randVelSlider.setTextValueSuffix("");
    } else {
      // Mixed value - set to 0 and show "---"
      randVelSlider.setValue(0, juce::dontSendNotification);
      randVelSlider.setTextValueSuffix(" (---)");
    }
  }

  // Update ADSR controls (only for Envelope type)
  if (isEnvelope) {
    // Update Attack
    if (allTreesHaveSameValue("adsrAttack")) {
      int attack = static_cast<int>(getCommonValue("adsrAttack"));
      attackSlider.setValue(attack, juce::dontSendNotification);
      attackSlider.setTextValueSuffix(" ms");
    } else {
      attackSlider.setValue(50, juce::dontSendNotification);
      attackSlider.setTextValueSuffix(" ms (---)");
    }

    // Update Decay
    if (allTreesHaveSameValue("adsrDecay")) {
      int decay = static_cast<int>(getCommonValue("adsrDecay"));
      decaySlider.setValue(decay, juce::dontSendNotification);
      decaySlider.setTextValueSuffix(" ms");
    } else {
      decaySlider.setValue(0, juce::dontSendNotification);
      decaySlider.setTextValueSuffix(" ms (---)");
    }

    // Update Sustain
    if (allTreesHaveSameValue("adsrSustain")) {
      int sustain = static_cast<int>(getCommonValue("adsrSustain"));
      sustainSlider.setValue(sustain, juce::dontSendNotification);
      sustainSlider.setTextValueSuffix("");
    } else {
      sustainSlider.setValue(127, juce::dontSendNotification);
      sustainSlider.setTextValueSuffix(" (---)");
    }

    // Update Release
    if (allTreesHaveSameValue("adsrRelease")) {
      int release = static_cast<int>(getCommonValue("adsrRelease"));
      releaseSlider.setValue(release, juce::dontSendNotification);
      releaseSlider.setTextValueSuffix(" ms");
    } else {
      releaseSlider.setValue(50, juce::dontSendNotification);
      releaseSlider.setTextValueSuffix(" ms (---)");
    }

    // Update Target Selector
    if (allTreesHaveSameValue("adsrTarget")) {
      juce::String target = getCommonValue("adsrTarget").toString();
      if (target == "CC" || target.isEmpty())
        envTargetSelector.setSelectedId(1, juce::dontSendNotification);
      else if (target == "PitchBend")
        envTargetSelector.setSelectedId(2, juce::dontSendNotification);
      else if (target == "SmartScaleBend")
        envTargetSelector.setSelectedId(3, juce::dontSendNotification);
      else
        envTargetSelector.setSelectedId(-1, juce::dontSendNotification);
    } else {
      envTargetSelector.setSelectedId(-1, juce::dontSendNotification);
    }

    // Update Pitch Bend sliders
    if (target == "PitchBend") {
      // Update PB Shift
      if (allTreesHaveSameValue("pbShift")) {
        int pbShift = static_cast<int>(getCommonValue("pbShift"));
        pbShiftSlider.setValue(pbShift, juce::dontSendNotification);
        pbShiftSlider.setTextValueSuffix(" semitones");
      } else {
        pbShiftSlider.setValue(0, juce::dontSendNotification);
        pbShiftSlider.setTextValueSuffix(" semitones (---)");
      }
    }

    // Update Smart Scale Bend slider
    if (target == "SmartScaleBend") {
      // Update Smart Step Shift
      if (allTreesHaveSameValue("smartStepShift")) {
        int smartStep = static_cast<int>(getCommonValue("smartStepShift"));
        smartStepSlider.setValue(smartStep, juce::dontSendNotification);
        smartStepSlider.setTextValueSuffix(" steps");
      } else {
        smartStepSlider.setValue(0, juce::dontSendNotification);
        smartStepSlider.setTextValueSuffix(" steps (---)");
      }
    }

    // Update data1Slider visibility based on target
    bool showData1 = (target != "PitchBend");
    data1Slider.setVisible(showData1);
    data1Label.setVisible(showData1);
  }

  // Trigger layout update after visibility changes
  resized();
}

void MappingInspector::updateVisibility() {
  bool isLayerCmd = false;
  if (!selectedTrees.empty() && allTreesHaveSameValue("type") &&
      getCommonValue("type").toString() == "Command" &&
      allTreesHaveSameValue("data1")) {
    int cmdId = static_cast<int>(getCommonValue("data1"));
    isLayerCmd =
        (cmdId >= static_cast<int>(OmniKey::CommandID::LayerMomentary) &&
         cmdId <= static_cast<int>(OmniKey::CommandID::LayerSolo));
  }
  targetLayerSelector.setVisible(isLayerCmd);
  targetLayerLabel.setVisible(isLayerCmd);
}

void MappingInspector::enableControls(bool enabled) {
  typeSelector.setEnabled(enabled);
  channelSlider.setEnabled(enabled);
  aliasSelector.setEnabled(enabled);
  data1Slider.setEnabled(enabled);
  data2Slider.setEnabled(enabled);
  randVelSlider.setEnabled(enabled);
  commandSelector.setEnabled(enabled);
  targetLayerSelector.setEnabled(enabled);
}

bool MappingInspector::allTreesHaveSameValue(const juce::Identifier &property) {
  if (selectedTrees.empty())
    return false;

  juce::var firstValue = selectedTrees[0].getProperty(property);
  for (size_t i = 1; i < selectedTrees.size(); ++i) {
    if (selectedTrees[i].getProperty(property) != firstValue)
      return false;
  }
  return true;
}

juce::var MappingInspector::getCommonValue(const juce::Identifier &property) {
  if (!selectedTrees.empty())
    return selectedTrees[0].getProperty(property);
  return juce::var();
}

void MappingInspector::updatePitchBendPeakValue(juce::ValueTree &tree) {
  // Note: This method is no longer used since InputProcessor now handles
  // the calculation using global range. However, we keep it for potential
  // future use or if we need to update data2 from the UI side.
  // The actual calculation happens in GridCompiler when the grid is rebuilt
  // using SettingsManager::getPitchBendRange().
}

void MappingInspector::valueTreePropertyChanged(
    juce::ValueTree &tree, const juce::Identifier &property) {
  // Phase 45.8: avoid feedback loops if we are already syncing from the tree
  if (isUpdatingFromTree)
    return;

  // Only react to properties that the inspector actually displays
  if (!(property == juce::Identifier("type") ||
        property == juce::Identifier("channel") ||
        property == juce::Identifier("data1") ||
        property == juce::Identifier("data2") ||
        property == juce::Identifier("inputAlias") ||
        property == juce::Identifier("deviceHash") ||
        property == juce::Identifier("adsrAttack") ||
        property == juce::Identifier("adsrDecay") ||
        property == juce::Identifier("adsrSustain") ||
        property == juce::Identifier("adsrRelease") ||
        property == juce::Identifier("adsrTarget") ||
        property == juce::Identifier("pbShift") ||
        property == juce::Identifier("smartStepShift")))
    return;

  // Check if this tree is part of the current selection
  bool isRelevant = false;
  for (const auto &t : selectedTrees) {
    if (t == tree) {
      isRelevant = true;
      break;
    }
  }
  if (!isRelevant)
    return;

  // Refresh UI on the message thread, reusing current selection
  juce::MessageManager::callAsync([this]() {
    if (selectedTrees.empty())
      return;
    isUpdatingFromTree = true;
    setSelection(selectedTrees);
    isUpdatingFromTree = false;
  });
}
