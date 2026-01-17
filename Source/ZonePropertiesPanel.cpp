#include "ZonePropertiesPanel.h"
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" || aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

// Helper to convert alias hash to name (for display)
static juce::String aliasHashToName(uintptr_t hash, DeviceManager *deviceMgr) {
  if (hash == 0)
    return "Any / Master";
  
  // We can't easily reverse the hash, so we'll need to check all aliases
  // For now, just return "Unknown" - in practice, we should store the alias name
  // For Phase 11, we'll use a simple approach: store the alias name in the zone
  return "Unknown";
}

ZonePropertiesPanel::ZonePropertiesPanel(DeviceManager *deviceMgr, RawInputManager *rawInputMgr)
    : deviceManager(deviceMgr), rawInputManager(rawInputMgr) {
  
  // Labels
  addAndMakeVisible(aliasLabel);
  aliasLabel.setText("Device Alias:", juce::dontSendNotification);
  aliasLabel.attachToComponent(&aliasSelector, true);

  addAndMakeVisible(aliasSelector);
  refreshAliasSelector();
  aliasSelector.onChange = [this] {
    if (currentZone && deviceManager) {
      int selected = aliasSelector.getSelectedItemIndex();
      if (selected >= 0) {
        juce::String aliasName = aliasSelector.getItemText(selected);
        currentZone->targetAliasHash = aliasNameToHash(aliasName);
      }
    }
  };

  addAndMakeVisible(nameLabel);
  nameLabel.setText("Zone Name:", juce::dontSendNotification);
  nameLabel.attachToComponent(&nameEditor, true);

  addAndMakeVisible(nameEditor);
  nameEditor.onTextChange = [this] {
    if (currentZone) {
      currentZone->name = nameEditor.getText();
    }
  };

  addAndMakeVisible(scaleLabel);
  scaleLabel.setText("Scale:", juce::dontSendNotification);
  scaleLabel.attachToComponent(&scaleSelector, true);

  addAndMakeVisible(scaleSelector);
  scaleSelector.addItem("Chromatic", 1);
  scaleSelector.addItem("Major", 2);
  scaleSelector.addItem("Minor", 3);
  scaleSelector.addItem("Pentatonic Major", 4);
  scaleSelector.addItem("Pentatonic Minor", 5);
  scaleSelector.addItem("Blues", 6);
  scaleSelector.onChange = [this] {
    if (currentZone) {
      int selected = scaleSelector.getSelectedItemIndex();
      switch (selected) {
      case 0: currentZone->scale = ScaleUtilities::ScaleType::Chromatic; break;
      case 1: currentZone->scale = ScaleUtilities::ScaleType::Major; break;
      case 2: currentZone->scale = ScaleUtilities::ScaleType::Minor; break;
      case 3: currentZone->scale = ScaleUtilities::ScaleType::PentatonicMajor; break;
      case 4: currentZone->scale = ScaleUtilities::ScaleType::PentatonicMinor; break;
      case 5: currentZone->scale = ScaleUtilities::ScaleType::Blues; break;
      default: currentZone->scale = ScaleUtilities::ScaleType::Major; break;
      }
    }
  };

  addAndMakeVisible(rootLabel);
  rootLabel.setText("Root Note:", juce::dontSendNotification);
  rootLabel.attachToComponent(&rootSlider, true);

  addAndMakeVisible(rootSlider);
  rootSlider.setRange(0, 127, 1);
  rootSlider.setTextValueSuffix(" (" + MidiNoteUtilities::getMidiNoteName(60) + ")");
  rootSlider.setValue(60);
  rootSlider.textFromValueFunction = [](double value) {
    return MidiNoteUtilities::getMidiNoteName(static_cast<int>(value));
  };
  rootSlider.valueFromTextFunction = [](const juce::String &text) {
    return static_cast<double>(MidiNoteUtilities::getMidiNoteFromText(text));
  };
  rootSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->rootNote = static_cast<int>(rootSlider.getValue());
    }
  };

  addAndMakeVisible(chromaticOffsetLabel);
  chromaticOffsetLabel.setText("Chromatic Offset:", juce::dontSendNotification);
  chromaticOffsetLabel.attachToComponent(&chromaticOffsetSlider, true);

  addAndMakeVisible(chromaticOffsetSlider);
  chromaticOffsetSlider.setRange(-12, 12, 1);
  chromaticOffsetSlider.setValue(0);
  chromaticOffsetSlider.textFromValueFunction = [](double value) {
    int v = static_cast<int>(value);
    if (v == 0) return juce::String("0");
    if (v > 0) return juce::String("+") + juce::String(v) + "st";
    return juce::String(v) + "st";
  };
  chromaticOffsetSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->chromaticOffset = static_cast<int>(chromaticOffsetSlider.getValue());
    }
  };

  addAndMakeVisible(degreeOffsetLabel);
  degreeOffsetLabel.setText("Degree Offset:", juce::dontSendNotification);
  degreeOffsetLabel.attachToComponent(&degreeOffsetSlider, true);

  addAndMakeVisible(degreeOffsetSlider);
  degreeOffsetSlider.setRange(-7, 7, 1);
  degreeOffsetSlider.setValue(0);
  degreeOffsetSlider.textFromValueFunction = [](double value) {
    int v = static_cast<int>(value);
    if (v == 0) return juce::String("0");
    if (v > 0) return juce::String("+") + juce::String(v);
    return juce::String(v);
  };
  degreeOffsetSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->degreeOffset = static_cast<int>(degreeOffsetSlider.getValue());
    }
  };

  addAndMakeVisible(transposeLockButton);
  transposeLockButton.setButtonText("Lock Transpose");
  transposeLockButton.onClick = [this] {
    if (currentZone) {
      currentZone->isTransposeLocked = transposeLockButton.getToggleState();
    }
  };

  addAndMakeVisible(captureKeysButton);
  captureKeysButton.setButtonText("Assign Keys");
  captureKeysButton.setClickingTogglesState(true);

  addAndMakeVisible(strategyLabel);
  strategyLabel.setText("Layout Strategy:", juce::dontSendNotification);
  strategyLabel.attachToComponent(&strategySelector, true);

  addAndMakeVisible(strategySelector);
  strategySelector.addItem("Linear", 1);
  strategySelector.addItem("Grid", 2);
  strategySelector.onChange = [this] {
    if (currentZone) {
      int selected = strategySelector.getSelectedItemIndex();
      if (selected == 0) {
        currentZone->layoutStrategy = Zone::LayoutStrategy::Linear;
      } else if (selected == 1) {
        currentZone->layoutStrategy = Zone::LayoutStrategy::Grid;
      }
      // Enable/disable grid interval slider based on strategy
      gridIntervalSlider.setEnabled(currentZone->layoutStrategy == Zone::LayoutStrategy::Grid);
    }
  };

  addAndMakeVisible(gridIntervalLabel);
  gridIntervalLabel.setText("Grid Interval:", juce::dontSendNotification);
  gridIntervalLabel.attachToComponent(&gridIntervalSlider, true);

  addAndMakeVisible(gridIntervalSlider);
  gridIntervalSlider.setRange(-12, 12, 1);
  gridIntervalSlider.setValue(5);
  gridIntervalSlider.textFromValueFunction = [](double value) {
    int v = static_cast<int>(value);
    if (v == 0) return juce::String("0");
    if (v > 0) return juce::String("+") + juce::String(v) + "st";
    return juce::String(v) + "st";
  };
  gridIntervalSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->gridInterval = static_cast<int>(gridIntervalSlider.getValue());
    }
  };
  gridIntervalSlider.setEnabled(false); // Initially disabled (defaults to Linear)

  addAndMakeVisible(chipList);
  chipList.onKeyRemoved = [this](int keyCode) {
    if (currentZone) {
      currentZone->removeKey(keyCode);
      chipList.setKeys(currentZone->inputKeyCodes);
      updateKeysAssignedLabel();
    }
  };

  if (rawInputManager) {
    rawInputManager->addListener(this);
  }

  if (deviceManager) {
    deviceManager->addChangeListener(this);
  }
}

ZonePropertiesPanel::~ZonePropertiesPanel() {
  if (rawInputManager) {
    rawInputManager->removeListener(this);
  }
  if (deviceManager) {
    deviceManager->removeChangeListener(this);
  }
}

void ZonePropertiesPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void ZonePropertiesPanel::resized() {
  auto area = getLocalBounds().reduced(8);
  int labelWidth = 120;
  int rowHeight = 28;
  int spacing = 4;

  int y = 0;

  // Alias
  aliasSelector.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Name
  nameEditor.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Scale
  scaleSelector.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Root Note
  rootSlider.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Chromatic Offset
  chromaticOffsetSlider.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Degree Offset
  degreeOffsetSlider.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Lock Transpose
  transposeLockButton.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Capture Keys
  captureKeysButton.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Strategy
  strategySelector.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Grid Interval
  gridIntervalSlider.setBounds(area.removeFromTop(rowHeight).withTrimmedLeft(labelWidth));
  area.removeFromTop(spacing);
  y += rowHeight + spacing;

  // Key Chip List
  auto chipListArea = area.removeFromTop(120);
  chipList.setBounds(chipListArea.withTrimmedLeft(labelWidth).reduced(4));
}

void ZonePropertiesPanel::setZone(std::shared_ptr<Zone> zone) {
  currentZone = zone;
  updateControlsFromZone();
}

void ZonePropertiesPanel::updateControlsFromZone() {
  if (!currentZone) {
    // Disable all controls
    aliasSelector.setEnabled(false);
    nameEditor.setEnabled(false);
    scaleSelector.setEnabled(false);
    rootSlider.setEnabled(false);
    chromaticOffsetSlider.setEnabled(false);
    degreeOffsetSlider.setEnabled(false);
    transposeLockButton.setEnabled(false);
    captureKeysButton.setEnabled(false);
    strategySelector.setEnabled(false);
    gridIntervalSlider.setEnabled(false);
    chipList.setEnabled(false);
    chipList.setKeys({});
    return;
  }

  // Enable all controls
  aliasSelector.setEnabled(true);
  nameEditor.setEnabled(true);
  scaleSelector.setEnabled(true);
  rootSlider.setEnabled(true);
  chromaticOffsetSlider.setEnabled(true);
  degreeOffsetSlider.setEnabled(true);
  transposeLockButton.setEnabled(true);
  captureKeysButton.setEnabled(true);
  strategySelector.setEnabled(true);
  gridIntervalSlider.setEnabled(currentZone->layoutStrategy == Zone::LayoutStrategy::Grid);

  // Update values
  nameEditor.setText(currentZone->name, juce::dontSendNotification);

  // Set alias selector (find matching alias hash)
  // For Phase 11, we'll match by hash - in a full implementation, we'd store the alias name
  int aliasIndex = 0; // Default to "Any / Master"
  if (currentZone->targetAliasHash != 0 && deviceManager) {
    // Try to find matching alias
    auto aliases = deviceManager->getAllAliasNames();
    for (int i = 0; i < aliases.size(); ++i) {
      if (aliasNameToHash(aliases[i]) == currentZone->targetAliasHash) {
        aliasIndex = i + 1; // +1 because index 0 is "Any / Master"
        break;
      }
    }
  }
  aliasSelector.setSelectedItemIndex(aliasIndex, juce::dontSendNotification);

  // Set scale
  int scaleIndex = 1; // Default to Major
  switch (currentZone->scale) {
  case ScaleUtilities::ScaleType::Chromatic: scaleIndex = 0; break;
  case ScaleUtilities::ScaleType::Major: scaleIndex = 1; break;
  case ScaleUtilities::ScaleType::Minor: scaleIndex = 2; break;
  case ScaleUtilities::ScaleType::PentatonicMajor: scaleIndex = 3; break;
  case ScaleUtilities::ScaleType::PentatonicMinor: scaleIndex = 4; break;
  case ScaleUtilities::ScaleType::Blues: scaleIndex = 5; break;
  }
  scaleSelector.setSelectedItemIndex(scaleIndex, juce::dontSendNotification);

  rootSlider.setValue(currentZone->rootNote, juce::dontSendNotification);
  chromaticOffsetSlider.setValue(currentZone->chromaticOffset, juce::dontSendNotification);
  degreeOffsetSlider.setValue(currentZone->degreeOffset, juce::dontSendNotification);
  transposeLockButton.setToggleState(currentZone->isTransposeLocked, juce::dontSendNotification);

  // Set strategy
  int strategyIndex = (currentZone->layoutStrategy == Zone::LayoutStrategy::Linear) ? 0 : 1;
  strategySelector.setSelectedItemIndex(strategyIndex, juce::dontSendNotification);
  
  // Set grid interval
  gridIntervalSlider.setValue(currentZone->gridInterval, juce::dontSendNotification);
  gridIntervalSlider.setEnabled(currentZone->layoutStrategy == Zone::LayoutStrategy::Grid);

  // Update chip list
  chipList.setKeys(currentZone->inputKeyCodes);
  
  updateKeysAssignedLabel();
}

void ZonePropertiesPanel::updateKeysAssignedLabel() {
  // Label removed - chip list now shows keys visually
}

void ZonePropertiesPanel::refreshAliasSelector() {
  aliasSelector.clear();
  aliasSelector.addItem("Any / Master", 1);
  
  if (deviceManager) {
    auto aliases = deviceManager->getAllAliasNames();
    for (int i = 0; i < aliases.size(); ++i) {
      aliasSelector.addItem(aliases[i], i + 2);
    }
  }
  
  aliasSelector.setSelectedItemIndex(0, juce::dontSendNotification);
}

void ZonePropertiesPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  // Only process if capture mode is active
  if (!captureKeysButton.getToggleState() || !currentZone)
    return;

  // Only process key down events
  if (!isDown)
    return;

  // Add key code if unique
  auto &keys = currentZone->inputKeyCodes;
  if (std::find(keys.begin(), keys.end(), keyCode) == keys.end()) {
    keys.push_back(keyCode);
    // Update chip list on message thread
    juce::MessageManager::callAsync([this] {
      chipList.setKeys(currentZone->inputKeyCodes);
      updateKeysAssignedLabel();
    });
  }
}

void ZonePropertiesPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events during key capture
}

void ZonePropertiesPanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == deviceManager) {
    juce::MessageManager::callAsync([this] {
      refreshAliasSelector();
      if (currentZone) {
        updateControlsFromZone();
      }
    });
  }
}
