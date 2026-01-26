#include "ZonePropertiesPanel.h"
#include "ScaleEditorComponent.h"
#include "ScaleLibrary.h"
#include "ZoneManager.h"
#include <algorithm>

// Helper to convert alias name to hash (same as in InputProcessor)
static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

// Helper to convert alias hash to name (for display)
static juce::String aliasHashToName(uintptr_t hash, DeviceManager *deviceMgr) {
  if (hash == 0)
    return "Any / Master";

  // We can't easily reverse the hash, so we'll need to check all aliases
  // For now, just return "Unknown" - in practice, we should store the alias
  // name For Phase 11, we'll use a simple approach: store the alias name in the
  // zone
  return "Unknown";
}

ZonePropertiesPanel::ZonePropertiesPanel(ZoneManager *zoneMgr,
                                         DeviceManager *deviceMgr,
                                         RawInputManager *rawInputMgr,
                                         ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr), deviceManager(deviceMgr),
      rawInputManager(rawInputMgr), scaleLibrary(scaleLib) {

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
        // Rebuild lookup table when alias changes
        if (zoneManager) {
          zoneManager->rebuildLookupTable();
        }
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
  refreshScaleSelector();
  scaleSelector.onChange = [this] {
    if (currentZone && scaleLibrary) {
      int selected = scaleSelector.getSelectedItemIndex();
      if (selected >= 0) {
        juce::String scaleName = scaleSelector.getItemText(selected);
        currentZone->scaleName = scaleName;
        if (zoneManager)
          rebuildZoneCache();
      }
    }
  };

  addAndMakeVisible(globalScaleToggle);
  globalScaleToggle.setButtonText("Global");
  globalScaleToggle.onClick = [this] {
    if (currentZone && zoneManager) {
      currentZone->useGlobalScale = globalScaleToggle.getToggleState();
      updateVisibility();
      rebuildZoneCache();
    }
  };

  addAndMakeVisible(editScaleButton);
  editScaleButton.setButtonText("Edit...");
  editScaleButton.onClick = [this] {
    if (!scaleLibrary)
      return;

    // Create editor component
    auto *editor = new ScaleEditorComponent(scaleLibrary);

    // CRITICAL: Set size BEFORE adding to dialog
    editor->setSize(600, 400);

    // Setup dialog options
    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(editor);
    options.dialogTitle = "Scale Editor";
    options.dialogBackgroundColour = juce::Colour(0xff222222);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = false;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;
    options.componentToCentreAround = this;

    // Launch dialog - this returns a DialogWindow* that manages its own
    // lifetime
    options.launchAsync();
  };

  addAndMakeVisible(rootLabel);
  rootLabel.setText("Root Note:", juce::dontSendNotification);
  rootLabel.attachToComponent(&rootSlider, true);

  addAndMakeVisible(rootSlider);
  rootSlider.setRange(0, 127, 1);
  rootSlider.setTextValueSuffix(" (" + MidiNoteUtilities::getMidiNoteName(60) +
                                ")");
  rootSlider.setValue(60);
  rootSlider.textFromValueFunction = [](double value) {
    return MidiNoteUtilities::getMidiNoteName(static_cast<int>(value));
  };
  rootSlider.valueFromTextFunction = [](const juce::String &text) {
    return static_cast<double>(MidiNoteUtilities::getMidiNoteFromText(text));
  };
  rootSlider.onValueChange = [this] {
    if (currentZone && scaleLibrary && zoneManager) {
      currentZone->rootNote = static_cast<int>(rootSlider.getValue());
      rebuildZoneCache();
    }
  };

  addAndMakeVisible(globalRootToggle);
  globalRootToggle.setButtonText("Global");
  globalRootToggle.onClick = [this] {
    if (currentZone && zoneManager) {
      currentZone->useGlobalRoot = globalRootToggle.getToggleState();
      updateVisibility();
      rebuildZoneCache();
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
    if (v == 0)
      return juce::String("0");
    if (v > 0)
      return juce::String("+") + juce::String(v) + "st";
    return juce::String(v) + "st";
  };
  chromaticOffsetSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->chromaticOffset =
          static_cast<int>(chromaticOffsetSlider.getValue());
      // Note: chromaticOffset is applied in processKey, no need to rebuild
      // cache
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
    if (v == 0)
      return juce::String("0");
    if (v > 0)
      return juce::String("+") + juce::String(v);
    return juce::String(v);
  };
  degreeOffsetSlider.onValueChange = [this] {
    if (currentZone && scaleLibrary && zoneManager) {
      currentZone->degreeOffset =
          static_cast<int>(degreeOffsetSlider.getValue());
      rebuildZoneCache();
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
  captureKeysButton.onClick = [this] {
    if (captureKeysButton.getToggleState()) {
      // If Assign Keys is checked, uncheck Remove Keys
      removeKeysButton.setToggleState(false, juce::dontSendNotification);
    }
  };

  addAndMakeVisible(removeKeysButton);
  removeKeysButton.setButtonText("Remove Keys");
  removeKeysButton.setClickingTogglesState(true);
  removeKeysButton.onClick = [this] {
    if (removeKeysButton.getToggleState()) {
      // If Remove Keys is checked, uncheck Assign Keys
      captureKeysButton.setToggleState(false, juce::dontSendNotification);
    }
  };

  addAndMakeVisible(strategyLabel);
  strategyLabel.setText("Layout Strategy:", juce::dontSendNotification);
  strategyLabel.attachToComponent(&strategySelector, true);

  addAndMakeVisible(strategySelector);
  strategySelector.addItem("Linear", 1);
  strategySelector.addItem("Grid", 2);
  strategySelector.addItem("Piano", 3);
  strategySelector.onChange = [this] {
    if (currentZone) {
      int selected = strategySelector.getSelectedItemIndex();
      if (selected == 0) {
        currentZone->layoutStrategy = Zone::LayoutStrategy::Linear;
      } else if (selected == 1) {
        currentZone->layoutStrategy = Zone::LayoutStrategy::Grid;
      } else if (selected == 2) {
        currentZone->layoutStrategy = Zone::LayoutStrategy::Piano;
      }
      gridIntervalSlider.setEnabled(currentZone->layoutStrategy ==
                                    Zone::LayoutStrategy::Grid);
      pianoHelpLabel.setVisible(currentZone->layoutStrategy ==
                                Zone::LayoutStrategy::Piano);
      updateVisibility();
      if (zoneManager && scaleLibrary) {
        rebuildZoneCache();
        zoneManager->rebuildLookupTable();
        zoneManager->sendChangeMessage();
      }

      // Notify parent that resize might be needed (label visibility changed)
      if (onResizeRequested) {
        onResizeRequested();
      }
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
    if (v == 0)
      return juce::String("0");
    if (v > 0)
      return juce::String("+") + juce::String(v) + "st";
    return juce::String(v) + "st";
  };
  gridIntervalSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->gridInterval =
          static_cast<int>(gridIntervalSlider.getValue());
    }
  };
  gridIntervalSlider.setEnabled(
      false); // Initially disabled (defaults to Linear)

  addAndMakeVisible(pianoHelpLabel);
  pianoHelpLabel.setText("Requires 2 rows of keys", juce::dontSendNotification);
  pianoHelpLabel.setColour(juce::Label::textColourId, juce::Colours::grey);
  pianoHelpLabel.setVisible(false); // Initially hidden

  addAndMakeVisible(chordTypeLabel);
  chordTypeLabel.setText("Chord Type:", juce::dontSendNotification);
  chordTypeLabel.attachToComponent(&chordTypeSelector, true);

  addAndMakeVisible(chordTypeSelector);
  chordTypeSelector.addItem("Off", 1);
  chordTypeSelector.addItem("Triad", 2);
  chordTypeSelector.addItem("Seventh", 3);
  chordTypeSelector.addItem("Ninth", 4);
  chordTypeSelector.addItem("Power5", 5);
  chordTypeSelector.onChange = [this] {
    if (currentZone) {
      int selected = chordTypeSelector.getSelectedItemIndex();
      if (selected >= 0) {
        // Map combo box index to enum (0 = Off/None, 1 = Triad, etc.)
        if (selected == 0) {
          currentZone->chordType = ChordUtilities::ChordType::None;
        } else if (selected == 1) {
          currentZone->chordType = ChordUtilities::ChordType::Triad;
        } else if (selected == 2) {
          currentZone->chordType = ChordUtilities::ChordType::Seventh;
        } else if (selected == 3) {
          currentZone->chordType = ChordUtilities::ChordType::Ninth;
        } else if (selected == 4) {
          currentZone->chordType = ChordUtilities::ChordType::Power5;
        }
        if (zoneManager && scaleLibrary) {
          rebuildZoneCache();
          zoneManager->sendChangeMessage();
        }
      }
    }
  };

  addAndMakeVisible(voicingLabel);
  voicingLabel.setText("Voicing:", juce::dontSendNotification);
  voicingLabel.attachToComponent(&voicingSelector, true);

  addAndMakeVisible(voicingSelector);
  voicingSelector.addItem("Root Position (Bass)", 1);
  voicingSelector.addItem("Smooth / Inversions (Pads)", 2);
  voicingSelector.addItem("Guitar / Spread (Strum)", 3);
  voicingSelector.addItem("Smooth (Filled)", 4);
  voicingSelector.addItem("Guitar (Filled)", 5);
  voicingSelector.onChange = [this] {
    if (currentZone) {
      int selected = voicingSelector.getSelectedItemIndex();
      if (selected >= 0) {
        if (selected == 0) {
          currentZone->voicing = ChordUtilities::Voicing::RootPosition;
        } else if (selected == 1) {
          currentZone->voicing = ChordUtilities::Voicing::Smooth;
        } else if (selected == 2) {
          currentZone->voicing = ChordUtilities::Voicing::GuitarSpread;
        } else if (selected == 3) {
          currentZone->voicing = ChordUtilities::Voicing::SmoothFilled;
        } else if (selected == 4) {
          currentZone->voicing = ChordUtilities::Voicing::GuitarFilled;
        }
        if (zoneManager && scaleLibrary) {
          rebuildZoneCache();
          zoneManager->sendChangeMessage();
        }
      }
    }
  };

  addAndMakeVisible(playModeLabel);
  playModeLabel.setText("Play Mode:", juce::dontSendNotification);
  playModeLabel.attachToComponent(&playModeSelector, true);

  addAndMakeVisible(playModeSelector);
  playModeSelector.addItem("Direct", 1);
  playModeSelector.addItem("Strum Buffer", 2);
  playModeSelector.onChange = [this] {
    if (currentZone) {
      int selected = playModeSelector.getSelectedItemIndex();
      if (selected >= 0) {
        if (selected == 0) {
          currentZone->playMode = Zone::PlayMode::Direct;
        } else if (selected == 1) {
          currentZone->playMode = Zone::PlayMode::Strum;
        }
        if (zoneManager) {
          zoneManager->sendChangeMessage();
        }
      }
    }
  };

  addAndMakeVisible(strumSpeedLabel);
  strumSpeedLabel.setText("Strum Speed:", juce::dontSendNotification);
  strumSpeedLabel.attachToComponent(&strumSpeedSlider, true);

  addAndMakeVisible(strumSpeedSlider);
  strumSpeedSlider.setRange(0, 500, 1);
  strumSpeedSlider.setValue(0);
  strumSpeedSlider.setTextValueSuffix(" ms");
  strumSpeedSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->strumSpeedMs = static_cast<int>(strumSpeedSlider.getValue());
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(releaseBehaviorLabel);
  releaseBehaviorLabel.setText("Release Behavior:", juce::dontSendNotification);
  releaseBehaviorLabel.attachToComponent(&releaseBehaviorSelector, true);

  addAndMakeVisible(releaseBehaviorSelector);
  releaseBehaviorSelector.addItem("Normal", 1);
  releaseBehaviorSelector.addItem("Sustain", 2);
  releaseBehaviorSelector.onChange = [this] {
    if (currentZone) {
      int selected = releaseBehaviorSelector.getSelectedItemIndex();
      if (selected >= 0) {
        if (selected == 0) {
          currentZone->releaseBehavior = Zone::ReleaseBehavior::Normal;
        } else if (selected == 1) {
          currentZone->releaseBehavior = Zone::ReleaseBehavior::Sustain;
        }
        if (zoneManager) {
          zoneManager->sendChangeMessage();
        }
      }
    }
  };

  addAndMakeVisible(releaseDurationLabel);
  releaseDurationLabel.setText("Release Duration:", juce::dontSendNotification);
  releaseDurationLabel.attachToComponent(&releaseDurationSlider, true);

  addAndMakeVisible(releaseDurationSlider);
  releaseDurationSlider.setRange(0, 5000, 1);
  releaseDurationSlider.setValue(0);
  releaseDurationSlider.setTextValueSuffix(" ms");
  releaseDurationSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->releaseDurationMs =
          static_cast<int>(releaseDurationSlider.getValue());
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(allowSustainToggle);
  allowSustainToggle.setButtonText("Allow Sustain");
  allowSustainToggle.onClick = [this] {
    if (currentZone) {
      currentZone->allowSustain = allowSustainToggle.getToggleState();
      if (zoneManager)
        zoneManager->sendChangeMessage();
    }
  };

  addAndMakeVisible(baseVelLabel);
  baseVelLabel.setText("Base Velocity:", juce::dontSendNotification);
  baseVelLabel.attachToComponent(&baseVelSlider, true);

  addAndMakeVisible(baseVelSlider);
  baseVelSlider.setRange(1, 127, 1);
  baseVelSlider.setValue(100);
  baseVelSlider.textFromValueFunction = [](double value) {
    return juce::String(static_cast<int>(value));
  };
  baseVelSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->baseVelocity = static_cast<int>(baseVelSlider.getValue());
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(randVelLabel);
  randVelLabel.setText("Velocity Random:", juce::dontSendNotification);
  randVelLabel.attachToComponent(&randVelSlider, true);

  addAndMakeVisible(randVelSlider);
  randVelSlider.setRange(0, 64, 1);
  randVelSlider.setValue(0);
  randVelSlider.textFromValueFunction = [](double value) {
    return juce::String(static_cast<int>(value));
  };
  randVelSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->velocityRandom = static_cast<int>(randVelSlider.getValue());
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(strictGhostToggle);
  strictGhostToggle.setButtonText("Strict Ghost Harmony");
  strictGhostToggle.onClick = [this] {
    if (currentZone) {
      currentZone->strictGhostHarmony = strictGhostToggle.getToggleState();
      if (zoneManager && scaleLibrary) {
        rebuildZoneCache();
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(ghostVelLabel);
  ghostVelLabel.setText("Ghost Velocity %:", juce::dontSendNotification);
  ghostVelLabel.attachToComponent(&ghostVelSlider, true);

  addAndMakeVisible(ghostVelSlider);
  ghostVelSlider.setRange(0.0, 100.0, 1.0);
  ghostVelSlider.setValue(60.0);
  ghostVelSlider.textFromValueFunction = [](double value) {
    return juce::String(static_cast<int>(value));
  };
  ghostVelSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->ghostVelocityScale =
          static_cast<float>(ghostVelSlider.getValue()) / 100.0f;
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  // Bass Note Controls
  addAndMakeVisible(bassToggle);
  bassToggle.setButtonText("Add Bass");
  bassToggle.onClick = [this] {
    if (currentZone) {
      currentZone->addBassNote = bassToggle.getToggleState();
      bassOctaveSlider.setEnabled(bassToggle.getToggleState());
      if (zoneManager && scaleLibrary) {
        rebuildZoneCache();
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(bassOctaveLabel);
  bassOctaveLabel.setText("Bass Octave:", juce::dontSendNotification);
  bassOctaveLabel.attachToComponent(&bassOctaveSlider, true);

  addAndMakeVisible(bassOctaveSlider);
  bassOctaveSlider.setRange(-3, -1, 1);
  bassOctaveSlider.setValue(-1);
  bassOctaveSlider.textFromValueFunction = [](double value) {
    return juce::String(static_cast<int>(value));
  };
  bassOctaveSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->bassOctaveOffset =
          static_cast<int>(bassOctaveSlider.getValue());
      if (zoneManager && scaleLibrary) {
        rebuildZoneCache();
        zoneManager->sendChangeMessage();
      }
    }
  };
  bassOctaveSlider.setEnabled(false); // Disabled until bass toggle is on

  // Display Mode Controls
  addAndMakeVisible(displayModeLabel);
  displayModeLabel.setText("Display Mode:", juce::dontSendNotification);
  displayModeLabel.attachToComponent(&displayModeSelector, true);

  addAndMakeVisible(displayModeSelector);
  displayModeSelector.addItem("Note Name", 1);
  displayModeSelector.addItem("Roman Numeral", 2);
  displayModeSelector.onChange = [this] {
    if (currentZone) {
      currentZone->showRomanNumerals =
          (displayModeSelector.getSelectedId() == 2);
      if (zoneManager && scaleLibrary) {
        rebuildZoneCache();
        zoneManager->sendChangeMessage();
      }
    }
  };

  // Polyphony Mode Controls (Phase 26)
  addAndMakeVisible(polyphonyModeLabel);
  polyphonyModeLabel.setText("Polyphony Mode:", juce::dontSendNotification);
  polyphonyModeLabel.attachToComponent(&polyphonyModeSelector, true);

  addAndMakeVisible(polyphonyModeSelector);
  polyphonyModeSelector.addItem("Poly", 1);
  polyphonyModeSelector.addItem("Mono (Retrigger)", 2);
  polyphonyModeSelector.addItem("Legato (Glide)", 3);
  polyphonyModeSelector.onChange = [this] {
    if (currentZone) {
      int selected = polyphonyModeSelector.getSelectedId();
      if (selected == 1)
        currentZone->polyphonyMode = PolyphonyMode::Poly;
      else if (selected == 2)
        currentZone->polyphonyMode = PolyphonyMode::Mono;
      else if (selected == 3)
        currentZone->polyphonyMode = PolyphonyMode::Legato;
      updateVisibility();
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };

  addAndMakeVisible(glideTimeLabel);
  glideTimeLabel.setText("Glide Time:", juce::dontSendNotification);
  glideTimeLabel.attachToComponent(&glideTimeSlider, true);

  addAndMakeVisible(glideTimeSlider);
  glideTimeSlider.setRange(0, 500, 1);
  glideTimeSlider.setTextValueSuffix(" ms");
  glideTimeSlider.setValue(50);
  glideTimeSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->glideTimeMs = static_cast<int>(glideTimeSlider.getValue());
      if (zoneManager) {
        zoneManager->sendChangeMessage();
      }
    }
  };
  glideTimeSlider.setVisible(
      false); // Initially hidden (only visible for Legato)
  glideTimeLabel.setVisible(false);

  addAndMakeVisible(channelLabel);
  channelLabel.setText("MIDI Channel:", juce::dontSendNotification);
  channelLabel.attachToComponent(&channelSlider, true);

  addAndMakeVisible(channelSlider);
  channelSlider.setRange(1, 16, 1);
  channelSlider.setValue(1);
  channelSlider.textFromValueFunction = [](double value) {
    return juce::String(static_cast<int>(value));
  };
  channelSlider.onValueChange = [this] {
    if (currentZone) {
      currentZone->midiChannel = static_cast<int>(channelSlider.getValue());
    }
  };

  addAndMakeVisible(colorLabel);
  colorLabel.setText("Zone Color:", juce::dontSendNotification);
  colorLabel.attachToComponent(&colorButton, true);

  addAndMakeVisible(colorButton);
  colorButton.setButtonText("Color");
  colorButton.onClick = [this] {
    if (!currentZone)
      return;

    // Create ColourSelector with options
    int flags = juce::ColourSelector::showColourspace |
                juce::ColourSelector::showSliders |
                juce::ColourSelector::showColourAtTop;
    auto *colourSelector = new juce::ColourSelector(flags);
    colourSelector->setName("Zone Color");
    colourSelector->setCurrentColour(currentZone->zoneColor);
    colourSelector->setSize(400, 300);

    // Create a listener to update color when it changes
    class ColourChangeListener : public juce::ChangeListener {
    public:
      ColourChangeListener(ZonePropertiesPanel *panel,
                           juce::ColourSelector *selector,
                           std::shared_ptr<Zone> zone, ZoneManager *zm)
          : panel_(panel), selector_(selector), zone_(zone), zoneManager_(zm) {}

      void changeListenerCallback(juce::ChangeBroadcaster *source) override {
        if (source == selector_ && zone_) {
          zone_->zoneColor = selector_->getCurrentColour();
          panel_->colorButton.setColour(juce::TextButton::buttonColourId,
                                        zone_->zoneColor);
          panel_->colorButton.repaint();

          // Notify ZoneManager to refresh Visualizer
          if (zoneManager_) {
            zoneManager_->sendChangeMessage();
          }
        }
      }

    private:
      ZonePropertiesPanel *panel_;
      juce::ColourSelector *selector_;
      std::shared_ptr<Zone> zone_;
      ZoneManager *zoneManager_;
    };

    auto *listener = new ColourChangeListener(this, colourSelector, currentZone,
                                              zoneManager);
    colourSelector->addChangeListener(listener);

    // Show in CallOutBox attached to the button
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(colourSelector),
        colorButton.getScreenBounds(), this);
  };

  addAndMakeVisible(chipList);
  chipList.onKeyRemoved = [this](int keyCode) {
    if (currentZone && scaleLibrary && zoneManager) {
      currentZone->removeKey(keyCode);
      chipList.setKeys(currentZone->inputKeyCodes);
      updateKeysAssignedLabel();
      rebuildZoneCache();
      zoneManager->rebuildLookupTable();
      zoneManager->sendChangeMessage();
      // Notify parent that resize is needed
      if (onResizeRequested) {
        onResizeRequested();
      }
    }
  };

  if (rawInputManager) {
    rawInputManager->addListener(this);
  }

  if (deviceManager) {
    deviceManager->addChangeListener(this);
  }

  if (scaleLibrary) {
    scaleLibrary->addChangeListener(this);
  }
}

ZonePropertiesPanel::~ZonePropertiesPanel() {
  if (rawInputManager) {
    rawInputManager->removeListener(this);
  }
  if (deviceManager) {
    deviceManager->removeChangeListener(this);
  }

  if (scaleLibrary) {
    scaleLibrary->removeChangeListener(this);
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
  int leftMargin = area.getX() + labelWidth;
  int width = area.getWidth() - labelWidth;

  int y = 8; // Start at top padding

  // Alias
  aliasSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Name
  nameEditor.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Scale (selector + Global toggle + edit button)
  auto scaleArea = juce::Rectangle<int>(leftMargin, y, width, rowHeight);
  editScaleButton.setBounds(scaleArea.removeFromRight(60).reduced(2));
  globalScaleToggle.setBounds(scaleArea.removeFromRight(52).reduced(2));
  scaleSelector.setBounds(scaleArea);
  y += rowHeight + spacing;

  // Root Note (+ Global toggle)
  auto rootArea = juce::Rectangle<int>(leftMargin, y, width, rowHeight);
  globalRootToggle.setBounds(rootArea.removeFromRight(52).reduced(2));
  rootSlider.setBounds(rootArea);
  y += rowHeight + spacing;

  // Chromatic Offset
  chromaticOffsetSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Degree Offset
  degreeOffsetSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Lock Transpose
  transposeLockButton.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Allow Sustain
  allowSustainToggle.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Base Velocity
  baseVelLabel.setBounds(0, y, labelWidth, rowHeight);
  baseVelSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Velocity Random
  randVelLabel.setBounds(0, y, labelWidth, rowHeight);
  randVelSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Strict Ghost Toggle
  strictGhostToggle.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Ghost Velocity
  ghostVelLabel.setBounds(0, y, labelWidth, rowHeight);
  ghostVelSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Bass Toggle
  bassToggle.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Bass Octave
  bassOctaveLabel.setBounds(0, y, labelWidth, rowHeight);
  bassOctaveSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Display Mode
  displayModeLabel.setBounds(0, y, labelWidth, rowHeight);
  displayModeSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Polyphony Mode (Phase 26)
  polyphonyModeSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Glide Time (only visible for Legato)
  if (glideTimeSlider.isVisible()) {
    glideTimeSlider.setBounds(leftMargin, y, width, rowHeight);
    y += rowHeight + spacing;
  }

  // Adaptive Glide Toggle (only visible for Legato)
  if (adaptiveGlideToggle.isVisible()) {
    adaptiveGlideToggle.setBounds(leftMargin, y, width, rowHeight);
    y += rowHeight + spacing;
  }

  // Max Glide Time (only visible for Adaptive)
  if (maxGlideTimeSlider.isVisible()) {
    maxGlideTimeSlider.setBounds(leftMargin, y, width, rowHeight);
    y += rowHeight + spacing;
  }

  // Mono Warning Label (only visible for Mono/Legato) (Phase 26.2)
  if (monoWarningLabel.isVisible()) {
    monoWarningLabel.setBounds(leftMargin, y, width, rowHeight * 2); // Allow 2 rows for text
    y += (rowHeight * 2) + spacing;
  }

  // Chord Type
  chordTypeSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Voicing
  voicingSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Play Mode
  playModeSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Strum Speed
  strumSpeedSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Release Behavior
  releaseBehaviorSelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Release Duration
  releaseDurationSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Capture Keys and Remove Keys (side by side)
  auto keyButtonsArea = juce::Rectangle<int>(leftMargin, y, width, rowHeight);
  removeKeysButton.setBounds(
      keyButtonsArea.removeFromRight(width / 2).reduced(2, 0));
  captureKeysButton.setBounds(keyButtonsArea.reduced(2, 0));
  y += rowHeight + spacing;

  // Strategy
  strategySelector.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Grid Interval
  gridIntervalSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Piano Help Label (only visible when Piano strategy is selected)
  pianoHelpLabel.setBounds(leftMargin, y, width, rowHeight);
  if (currentZone &&
      currentZone->layoutStrategy == Zone::LayoutStrategy::Piano) {
    y += rowHeight + spacing;
  }

  // MIDI Channel
  channelSlider.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Zone Color
  colorButton.setBounds(leftMargin, y, width, rowHeight);
  y += rowHeight + spacing;

  // Key Chip List (dynamic height based on number of chips)
  int chipListHeight = juce::jmax(
      120,
      static_cast<int>(
          (currentZone ? currentZone->inputKeyCodes.size() : 0) * 28 + 16));
  chipList.setBounds(leftMargin + 4, y, width - 8, chipListHeight);
  y += chipListHeight + spacing;

  // Set component size based on calculated height
  setSize(getWidth(), y + 8); // Bottom padding
}

int ZonePropertiesPanel::getRequiredHeight() const {
  int rowHeight = 28;
  int spacing = 4;
  int topPadding = 8;
  int bottomPadding = 8;

  // Count number of rows
  int numRows =
      26; // Alias, Name, Scale, Root, Chromatic, Degree, Lock, ChordType,
          // Voicing, PlayMode, StrumSpeed, ReleaseBehavior, ReleaseDuration,
          // AllowSustain, BaseVel, RandVel, StrictGhost, GhostVel, BassToggle,
          // BassOctave, DisplayMode, Capture/Remove, Strategy, Grid, Channel,
          // Color
  // Add one more row if Piano help label is visible
  if (currentZone &&
      currentZone->layoutStrategy == Zone::LayoutStrategy::Piano) {
    numRows++;
  }

  // Calculate chip list height dynamically
  int chipListHeight = juce::jmax(
      120,
      static_cast<int>(
          (currentZone ? currentZone->inputKeyCodes.size() : 0) * 28 + 16));

  return topPadding + (numRows * (rowHeight + spacing)) + chipListHeight +
         bottomPadding;
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
    globalScaleToggle.setEnabled(false);
    rootSlider.setEnabled(false);
    globalRootToggle.setEnabled(false);
    chromaticOffsetSlider.setEnabled(false);
    degreeOffsetSlider.setEnabled(false);
    transposeLockButton.setEnabled(false);
    captureKeysButton.setEnabled(false);
    removeKeysButton.setEnabled(false);
    strategySelector.setEnabled(false);
    gridIntervalSlider.setEnabled(false);
    chordTypeSelector.setEnabled(false);
    voicingSelector.setEnabled(false);
    playModeSelector.setEnabled(false);
    strumSpeedSlider.setEnabled(false);
    releaseBehaviorSelector.setEnabled(false);
    releaseDurationSlider.setEnabled(false);
    allowSustainToggle.setEnabled(false);
    baseVelSlider.setEnabled(false);
    randVelSlider.setEnabled(false);
    strictGhostToggle.setEnabled(false);
    ghostVelSlider.setEnabled(false);
    bassToggle.setEnabled(false);
    bassOctaveSlider.setEnabled(false);
    displayModeSelector.setEnabled(false);
    polyphonyModeSelector.setEnabled(false);
    glideTimeSlider.setEnabled(false);
    adaptiveGlideToggle.setEnabled(false);
    maxGlideTimeSlider.setEnabled(false);
    monoWarningLabel.setEnabled(false);
    channelSlider.setEnabled(false);
    colorButton.setEnabled(false);
    chipList.setEnabled(false);
    chipList.setKeys({});
    return;
  }

  // Enable all controls
  aliasSelector.setEnabled(true);
  nameEditor.setEnabled(true);
  scaleSelector.setEnabled(true);
  globalScaleToggle.setEnabled(true);
  rootSlider.setEnabled(true);
  globalRootToggle.setEnabled(true);
  chromaticOffsetSlider.setEnabled(true);
  degreeOffsetSlider.setEnabled(true);
  transposeLockButton.setEnabled(true);
  captureKeysButton.setEnabled(true);
  removeKeysButton.setEnabled(true);
  strategySelector.setEnabled(true);
  gridIntervalSlider.setEnabled(currentZone->layoutStrategy ==
                                Zone::LayoutStrategy::Grid);
  chordTypeSelector.setEnabled(true);
  voicingSelector.setEnabled(true);
  playModeSelector.setEnabled(true);
  strumSpeedSlider.setEnabled(true);
  releaseBehaviorSelector.setEnabled(true);
  releaseDurationSlider.setEnabled(true);
  allowSustainToggle.setEnabled(true);
  baseVelSlider.setEnabled(true);
  randVelSlider.setEnabled(true);
  strictGhostToggle.setEnabled(true);
  ghostVelSlider.setEnabled(true);
  bassToggle.setEnabled(true);
  bassOctaveSlider.setEnabled(currentZone->addBassNote);
    displayModeSelector.setEnabled(true);
    polyphonyModeSelector.setEnabled(true);
    glideTimeSlider.setEnabled(true);
    adaptiveGlideToggle.setEnabled(true);
    maxGlideTimeSlider.setEnabled(true);
    monoWarningLabel.setEnabled(true);
    channelSlider.setEnabled(true);
  colorButton.setEnabled(true);

  // Update values
  nameEditor.setText(currentZone->name, juce::dontSendNotification);

  // Set alias selector (find matching alias hash)
  // For Phase 11, we'll match by hash - in a full implementation, we'd store
  // the alias name
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

  // Set scale selector to match current zone's scale name
  if (scaleLibrary) {
    auto scaleNames = scaleLibrary->getScaleNames();
    int scaleIndex = -1;
    for (int i = 0; i < scaleNames.size(); ++i) {
      if (scaleNames[i] == currentZone->scaleName) {
        scaleIndex = i;
        break;
      }
    }
    scaleSelector.setSelectedItemIndex(scaleIndex >= 0 ? scaleIndex : 0,
                                       juce::dontSendNotification);
  }

  rootSlider.setValue(currentZone->rootNote, juce::dontSendNotification);
  chromaticOffsetSlider.setValue(currentZone->chromaticOffset,
                                 juce::dontSendNotification);
  degreeOffsetSlider.setValue(currentZone->degreeOffset,
                              juce::dontSendNotification);
  transposeLockButton.setToggleState(currentZone->isTransposeLocked,
                                     juce::dontSendNotification);

  // Set strategy
  int strategyIndex = 0;
  if (currentZone->layoutStrategy == Zone::LayoutStrategy::Linear) {
    strategyIndex = 0;
  } else if (currentZone->layoutStrategy == Zone::LayoutStrategy::Grid) {
    strategyIndex = 1;
  } else if (currentZone->layoutStrategy == Zone::LayoutStrategy::Piano) {
    strategyIndex = 2;
  }
  strategySelector.setSelectedItemIndex(strategyIndex,
                                        juce::dontSendNotification);

  // Set grid interval
  gridIntervalSlider.setValue(currentZone->gridInterval,
                              juce::dontSendNotification);
  gridIntervalSlider.setEnabled(currentZone->layoutStrategy ==
                                Zone::LayoutStrategy::Grid);

  // Show/hide piano help label
  pianoHelpLabel.setVisible(currentZone->layoutStrategy ==
                            Zone::LayoutStrategy::Piano);

  // Set MIDI channel
  channelSlider.setValue(currentZone->midiChannel, juce::dontSendNotification);

  // Set zone color button
  colorButton.setColour(juce::TextButton::buttonColourId,
                        currentZone->zoneColor);
  colorButton.repaint();

  // Set chord type
  int chordTypeIndex = 0;
  switch (currentZone->chordType) {
  case ChordUtilities::ChordType::None:
    chordTypeIndex = 0;
    break;
  case ChordUtilities::ChordType::Triad:
    chordTypeIndex = 1;
    break;
  case ChordUtilities::ChordType::Seventh:
    chordTypeIndex = 2;
    break;
  case ChordUtilities::ChordType::Ninth:
    chordTypeIndex = 3;
    break;
  case ChordUtilities::ChordType::Power5:
    chordTypeIndex = 4;
    break;
  }
  chordTypeSelector.setSelectedItemIndex(chordTypeIndex,
                                         juce::dontSendNotification);

  // Set voicing
  int voicingIndex = 0;
  switch (currentZone->voicing) {
  case ChordUtilities::Voicing::RootPosition:
    voicingIndex = 0;
    break;
  case ChordUtilities::Voicing::Smooth:
    voicingIndex = 1;
    break;
  case ChordUtilities::Voicing::GuitarSpread:
    voicingIndex = 2;
    break;
  case ChordUtilities::Voicing::SmoothFilled:
    voicingIndex = 3;
    break;
  case ChordUtilities::Voicing::GuitarFilled:
    voicingIndex = 4;
    break;
  default:
    voicingIndex = 0;
    break;
  }
  voicingSelector.setSelectedItemIndex(voicingIndex,
                                       juce::dontSendNotification);

  // Set play mode
  int playModeIndex = (currentZone->playMode == Zone::PlayMode::Direct) ? 0 : 1;
  playModeSelector.setSelectedItemIndex(playModeIndex,
                                        juce::dontSendNotification);

  // Set strum speed
  strumSpeedSlider.setValue(currentZone->strumSpeedMs,
                            juce::dontSendNotification);

  // Set release behavior
  int releaseBehaviorIndex =
      (currentZone->releaseBehavior == Zone::ReleaseBehavior::Normal) ? 0 : 1;
  releaseBehaviorSelector.setSelectedItemIndex(releaseBehaviorIndex,
                                               juce::dontSendNotification);

  // Set release duration
  releaseDurationSlider.setValue(currentZone->releaseDurationMs,
                                 juce::dontSendNotification);

  allowSustainToggle.setToggleState(currentZone->allowSustain,
                                    juce::dontSendNotification);
  baseVelSlider.setValue(currentZone->baseVelocity, juce::dontSendNotification);
  randVelSlider.setValue(currentZone->velocityRandom,
                         juce::dontSendNotification);
  strictGhostToggle.setToggleState(currentZone->strictGhostHarmony,
                                   juce::dontSendNotification);
  ghostVelSlider.setValue(currentZone->ghostVelocityScale * 100.0,
                          juce::dontSendNotification);
  bassToggle.setToggleState(currentZone->addBassNote,
                            juce::dontSendNotification);
  bassOctaveSlider.setValue(currentZone->bassOctaveOffset,
                            juce::dontSendNotification);
  bassOctaveSlider.setEnabled(currentZone->addBassNote);
  displayModeSelector.setSelectedId(currentZone->showRomanNumerals ? 2 : 1,
                                    juce::dontSendNotification);

  // Set polyphony mode
  int polyModeId = 1; // Default to Poly
  if (currentZone->polyphonyMode == PolyphonyMode::Poly)
    polyModeId = 1;
  else if (currentZone->polyphonyMode == PolyphonyMode::Mono)
    polyModeId = 2;
  else if (currentZone->polyphonyMode == PolyphonyMode::Legato)
    polyModeId = 3;
  polyphonyModeSelector.setSelectedId(polyModeId, juce::dontSendNotification);

  // Set glide time
  glideTimeSlider.setValue(currentZone->glideTimeMs,
                           juce::dontSendNotification);

  chipList.setKeys(currentZone->inputKeyCodes);

  globalScaleToggle.setToggleState(currentZone->useGlobalScale,
                                   juce::dontSendNotification);
  globalRootToggle.setToggleState(currentZone->useGlobalRoot,
                                  juce::dontSendNotification);
  updateVisibility();

  if (scaleLibrary && zoneManager)
    rebuildZoneCache();

  updateKeysAssignedLabel();

  // Notify parent that resize might be needed
  if (onResizeRequested) {
    onResizeRequested();
  }
}

void ZonePropertiesPanel::updateKeysAssignedLabel() {
  // Label removed - chip list now shows keys visually
}

void ZonePropertiesPanel::updateVisibility() {
  if (!currentZone)
    return;
  bool piano = (currentZone->layoutStrategy == Zone::LayoutStrategy::Piano);
  bool useGlobalScale = globalScaleToggle.getToggleState();
  bool useGlobalRoot = globalRootToggle.getToggleState();
  scaleSelector.setEnabled((!piano) && (!useGlobalScale));
  editScaleButton.setEnabled((!piano) && (!useGlobalScale));
  rootSlider.setEnabled(!useGlobalRoot);
  
  // Show glide controls only for Legato mode
  bool isLegato = (currentZone->polyphonyMode == PolyphonyMode::Legato);
  glideTimeSlider.setVisible(isLegato);
  glideTimeLabel.setVisible(isLegato);
  adaptiveGlideToggle.setVisible(isLegato);
  
  // Show max glide time slider only for Adaptive mode
  bool isAdaptive = isLegato && currentZone->isAdaptiveGlide;
  maxGlideTimeSlider.setVisible(isAdaptive);
  maxGlideTimeLabel.setVisible(isAdaptive);
  
  // Update glide time label text based on adaptive mode
  if (isLegato) {
    if (currentZone->isAdaptiveGlide) {
      glideTimeLabel.setText("Glide Time / Min:", juce::dontSendNotification);
    } else {
      glideTimeLabel.setText("Glide Time:", juce::dontSendNotification);
    }
  }
  
  // Show warning label for Mono/Legato modes (Phase 26.2)
  bool isMonoOrLegato = (currentZone->polyphonyMode == PolyphonyMode::Mono || 
                         currentZone->polyphonyMode == PolyphonyMode::Legato);
  monoWarningLabel.setVisible(isMonoOrLegato);
}

void ZonePropertiesPanel::rebuildZoneCache() {
  if (!currentZone || !zoneManager || !scaleLibrary)
    return;
  std::vector<int> intervals;
  int root;
  if (globalScaleToggle.getToggleState())
    intervals = scaleLibrary->getIntervals(zoneManager->getGlobalScaleName());
  else {
    int idx = scaleSelector.getSelectedItemIndex();
    juce::String name =
        (idx >= 0) ? scaleSelector.getItemText(idx) : currentZone->scaleName;
    intervals = scaleLibrary->getIntervals(name);
  }
  if (globalRootToggle.getToggleState())
    root = zoneManager->getGlobalRootNote();
  else
    root = static_cast<int>(rootSlider.getValue());
  currentZone->rebuildCache(intervals, root);
  zoneManager->sendChangeMessage();
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

void ZonePropertiesPanel::refreshScaleSelector() {
  scaleSelector.clear();

  if (scaleLibrary) {
    auto scaleNames = scaleLibrary->getScaleNames();
    for (int i = 0; i < scaleNames.size(); ++i) {
      scaleSelector.addItem(scaleNames[i], i + 1);
    }
  } else {
    // Fallback if no ScaleLibrary
    scaleSelector.addItem("Major", 1);
  }
}

void ZonePropertiesPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                            bool isDown) {
  if (!currentZone)
    return;

  // Only process key down events
  if (!isDown)
    return;

  // Assign Keys mode
  if (captureKeysButton.getToggleState()) {
    // Add key code if unique
    auto &keys = currentZone->inputKeyCodes;
    if (std::find(keys.begin(), keys.end(), keyCode) == keys.end()) {
      keys.push_back(keyCode);
      // Update chip list on message thread
      juce::MessageManager::callAsync([this] {
        chipList.setKeys(currentZone->inputKeyCodes);
        updateKeysAssignedLabel();
        if (scaleLibrary && zoneManager) {
          rebuildZoneCache();
          zoneManager->rebuildLookupTable();
          zoneManager->sendChangeMessage();
        }
        // Notify parent that resize is needed
        if (onResizeRequested) {
          onResizeRequested();
        }
      });
    }
    return;
  }

  // Remove Keys mode
  if (removeKeysButton.getToggleState()) {
    // Remove key code if it exists
    auto &keys = currentZone->inputKeyCodes;
    auto it = std::find(keys.begin(), keys.end(), keyCode);
    if (it != keys.end()) {
      currentZone->removeKey(keyCode);
      // Update chip list on message thread
      juce::MessageManager::callAsync([this] {
        chipList.setKeys(currentZone->inputKeyCodes);
        updateKeysAssignedLabel();
        if (scaleLibrary && zoneManager) {
          rebuildZoneCache();
          zoneManager->rebuildLookupTable();
          zoneManager->sendChangeMessage();
        }
        // Notify parent that resize is needed
        if (onResizeRequested) {
          onResizeRequested();
        }
      });
    }
    return;
  }
}

void ZonePropertiesPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                          float value) {
  // Ignore axis events during key capture
}

void ZonePropertiesPanel::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == deviceManager) {
    juce::MessageManager::callAsync([this] {
      refreshAliasSelector();
      if (currentZone) {
        updateControlsFromZone();
      }
    });
  } else if (source == scaleLibrary) {
    juce::MessageManager::callAsync([this] {
      refreshScaleSelector();
      if (currentZone) {
        updateControlsFromZone();
      }
    });
  }
}
