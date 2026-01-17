#include "MappingInspector.h"
#include "MidiNoteUtilities.h"
#include "DeviceManager.h"

MappingInspector::MappingInspector(juce::UndoManager *undoMgr, DeviceManager *deviceMgr)
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

  aliasLabel.setText("Alias:", juce::dontSendNotification);
  aliasLabel.attachToComponent(&aliasSelector, true);
  addAndMakeVisible(aliasLabel);
  addAndMakeVisible(aliasSelector);

  // Setup Alias Selector
  refreshAliasSelector();
  aliasSelector.onChange = [this] {
    if (selectedTrees.empty())
      return;
    
    // Don't update if showing mixed value (no selection or multiple different values)
    if (aliasSelector.getSelectedId() == -1)
      return;

    juce::String aliasName = aliasSelector.getText();
    
    // Special case: "Any / Master" means empty or hash 0
    if (aliasName == "Any / Master")
      aliasName = "";

    undoManager->beginNewTransaction("Change Alias");
    for (auto &tree : selectedTrees) {
      if (tree.isValid()) {
        if (aliasName.isEmpty()) {
          // Set both inputAlias (empty) and deviceHash (0) for legacy compatibility
          tree.setProperty("inputAlias", "", undoManager);
          tree.setProperty("deviceHash", "0", undoManager);
        } else {
          tree.setProperty("inputAlias", aliasName, undoManager);
          // Also update deviceHash to 0 for legacy compatibility (or remove it)
          // Actually, let's keep deviceHash for backward compatibility
        }
      }
    }
  };

  // Setup Type Selector
  typeSelector.addItem("Note", 1);
  typeSelector.addItem("CC", 2);
  typeSelector.addItem("Macro", 3);
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
      typeStr = "Macro";
    else
      return; // Invalid selection

    undoManager->beginNewTransaction("Change Type");
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("type", typeStr, undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
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

    undoManager->beginNewTransaction("Change Channel");
    int value = static_cast<int>(channelSlider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("channel", value, undoManager);
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

    undoManager->beginNewTransaction("Change Data1");
    int value = static_cast<int>(data1Slider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("data1", value, undoManager);
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

    undoManager->beginNewTransaction("Change Data2");
    int value = static_cast<int>(data2Slider.getValue());
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("data2", value, undoManager);
    }
    // Transaction ends automatically when next beginNewTransaction() is called
  };

  // Register with DeviceManager for changes
  if (deviceManager != nullptr) {
    deviceManager->addChangeListener(this);
  }

  // Initially disable all controls
  enableControls(false);
}

MappingInspector::~MappingInspector() {
  if (deviceManager != nullptr) {
    deviceManager->removeChangeListener(this);
  }
}

void MappingInspector::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == deviceManager) {
    // Device alias configuration changed, refresh the selector
    refreshAliasSelector();
    updateControlsFromSelection();
  }
}

void MappingInspector::refreshAliasSelector() {
  aliasSelector.clear(juce::dontSendNotification);
  
  if (deviceManager == nullptr)
    return;

  // Add "Any / Master" option (hash 0)
  aliasSelector.addItem("Any / Master", 1);
  
  // Add all aliases
  auto aliases = deviceManager->getAllAliasNames();
  for (int i = 0; i < aliases.size(); ++i) {
    aliasSelector.addItem(aliases[i], i + 2); // Start IDs from 2
  }
}

void MappingInspector::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));

  if (selectedTrees.empty()) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No selection", getLocalBounds(),
               juce::Justification::centred, true);
  }
}

void MappingInspector::resized() {
  auto area = getLocalBounds().reduced(10);
  int labelWidth = 80;
  int controlHeight = 25;
  int spacing = 10;

  typeSelector.setBounds(area.removeFromTop(controlHeight));
  area.removeFromTop(spacing);

  channelSlider.setBounds(area.removeFromTop(controlHeight));
  area.removeFromTop(spacing);

  aliasSelector.setBounds(area.removeFromTop(controlHeight));
  area.removeFromTop(spacing);

  data1Slider.setBounds(area.removeFromTop(controlHeight));
  area.removeFromTop(spacing);

  data2Slider.setBounds(area.removeFromTop(controlHeight));
}

void MappingInspector::setSelection(const std::vector<juce::ValueTree> &selection) {
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
    else if (typeStr == "Macro")
      typeSelector.setSelectedId(3, juce::dontSendNotification);
    else
      typeSelector.setSelectedId(-1, juce::dontSendNotification);
  } else {
    typeSelector.setSelectedId(-1, juce::dontSendNotification);
    typeStr = ""; // Mixed types
  }

  // Configure Data1 slider based on type
  if (typeStr == "Note") {
    data1Label.setText("Note:", juce::dontSendNotification);
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
    data1Slider.setEnabled(true);
    // Remove smart lambda, revert to default numeric display
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
  } else if (typeStr == "Macro") {
    data1Label.setText("Macro ID:", juce::dontSendNotification);
    data1Slider.setEnabled(true);
    // Use default numeric display for macros
    data1Slider.textFromValueFunction = nullptr;
    data1Slider.valueFromTextFunction = nullptr;
    data1Slider.updateText();
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
  if (deviceManager != nullptr) {
    juce::String aliasName;
    
    // Try to get alias from inputAlias property first
    if (allTreesHaveSameValue("inputAlias")) {
      aliasName = getCommonValue("inputAlias").toString();
      if (aliasName.isEmpty()) {
        aliasName = "Any / Master";
      }
    } else {
      // Try legacy deviceHash property
      if (allTreesHaveSameValue("deviceHash")) {
        uintptr_t deviceHash = 0;
        juce::var hashVar = getCommonValue("deviceHash");
        if (hashVar.isString()) {
          deviceHash = static_cast<uintptr_t>(hashVar.toString().getHexValue64());
        } else if (hashVar.isInt()) {
          deviceHash = static_cast<uintptr_t>(static_cast<juce::int64>(hashVar));
        }
        
        if (deviceHash == 0) {
          aliasName = "Any / Master";
        } else {
          aliasName = deviceManager->getAliasName(deviceHash);
        }
      } else {
        // Mixed values - clear selection
        aliasSelector.setSelectedId(-1, juce::dontSendNotification);
      }
    }
    
    // Set selection if we found a single alias
    if (!aliasName.isEmpty() && aliasName != "Unknown") {
      // Find the ID for this alias name
      if (aliasName == "Any / Master") {
        aliasSelector.setSelectedId(1, juce::dontSendNotification);
      } else {
        auto aliases = deviceManager->getAllAliasNames();
        int index = aliases.indexOf(aliasName);
        if (index >= 0) {
          aliasSelector.setSelectedId(index + 2, juce::dontSendNotification); // IDs start at 2
        } else {
          aliasSelector.setSelectedId(-1, juce::dontSendNotification);
        }
      }
    } else if (aliasName == "Unknown" || aliasSelector.getSelectedId() == -1) {
      aliasSelector.setSelectedId(-1, juce::dontSendNotification);
    }
  }

  // Update Data1 Slider
  if (allTreesHaveSameValue("data1")) {
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
}

void MappingInspector::enableControls(bool enabled) {
  typeSelector.setEnabled(enabled);
  channelSlider.setEnabled(enabled);
  aliasSelector.setEnabled(enabled);
  data1Slider.setEnabled(enabled);
  data2Slider.setEnabled(enabled);
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

void MappingInspector::valueTreePropertyChanged(juce::ValueTree &tree,
                                                 const juce::Identifier &property) {
  // Update UI if the changed property is one we're displaying
  if (property == juce::Identifier("type") || property == juce::Identifier("channel") ||
      property == juce::Identifier("data1") || property == juce::Identifier("data2") ||
      property == juce::Identifier("inputAlias") || property == juce::Identifier("deviceHash")) {
    updateControlsFromSelection();
  }
}
