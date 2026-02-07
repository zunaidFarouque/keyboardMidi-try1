#include "SettingsPanel.h"
#include "MappingTypes.h"
#include <windows.h>

namespace {
const std::array<ActionType, 3> kMappingTypeOrder = {
    ActionType::Note, ActionType::Expression, ActionType::Command};
const char *kMappingTypeNames[] = {"Note", "Expression", "Command"};
} // namespace

void SettingsPanel::initialize() { settingsManager.addChangeListener(this); }

SettingsPanel::SettingsPanel(SettingsManager &settingsMgr, MidiEngine &midiEng,
                             RawInputManager &rawInputMgr)
    : settingsManager(settingsMgr), midiEngine(midiEng),
      rawInputManager(rawInputMgr),
      mappingColorsGroup("Mapping Colors", "Mapping Colors") {
  // Phase 42: addChangeListener moved to initialize()

  // Setup PB Range Slider
  pbRangeLabel.setText("Global Pitch Bend Range (+/- semitones):",
                       juce::dontSendNotification);
  pbRangeLabel.attachToComponent(&pbRangeSlider, true);
  addAndMakeVisible(pbRangeLabel);
  addAndMakeVisible(pbRangeSlider);

  pbRangeSlider.setRange(1, 96, 1);
  pbRangeSlider.setTextValueSuffix(" semitones");
  pbRangeSlider.setValue(settingsManager.getPitchBendRange(),
                         juce::dontSendNotification);

  pbRangeSlider.onValueChange = [this] {
    int value = static_cast<int>(pbRangeSlider.getValue());
    settingsManager.setPitchBendRange(value);
  };

  // Visualizer settings: X/Y opacity (0–100%)
  addAndMakeVisible(visualizerGroup);
  visXOpacityLabel.setText("Touchpad X bands opacity:",
                           juce::dontSendNotification);
  visYOpacityLabel.setText("Touchpad Y bands opacity:",
                           juce::dontSendNotification);
  addAndMakeVisible(visXOpacityLabel);
  addAndMakeVisible(visXOpacitySlider);
  addAndMakeVisible(visYOpacityLabel);
  addAndMakeVisible(visYOpacitySlider);

  visXOpacitySlider.setRange(0.0, 100.0, 1.0);
  visYOpacitySlider.setRange(0.0, 100.0, 1.0);
  visXOpacitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
  visYOpacitySlider.setSliderStyle(juce::Slider::LinearHorizontal);
  visXOpacitySlider.setTextValueSuffix(" %");
  visYOpacitySlider.setTextValueSuffix(" %");
  visXOpacitySlider.setValue(settingsManager.getVisualizerXOpacity() * 100.0,
                             juce::dontSendNotification);
  visYOpacitySlider.setValue(settingsManager.getVisualizerYOpacity() * 100.0,
                             juce::dontSendNotification);

  visXOpacitySlider.onValueChange = [this] {
    float v = static_cast<float>(visXOpacitySlider.getValue() / 100.0);
    settingsManager.setVisualizerXOpacity(v);
  };
  visYOpacitySlider.onValueChange = [this] {
    float v = static_cast<float>(visYOpacitySlider.getValue() / 100.0);
    settingsManager.setVisualizerYOpacity(v);
  };

  showTouchpadInMiniWindowToggle.setButtonText(
      "Show touchpad visualizer in mini window");
  showTouchpadInMiniWindowToggle.setToggleState(
      settingsManager.getShowTouchpadVisualizerInMiniWindow(),
      juce::dontSendNotification);
  showTouchpadInMiniWindowToggle.onClick = [this] {
    settingsManager.setShowTouchpadVisualizerInMiniWindow(
        showTouchpadInMiniWindowToggle.getToggleState());
  };
  addAndMakeVisible(showTouchpadInMiniWindowToggle);

  resetMiniWindowPositionButton.setButtonText("Reset mini window position");
  resetMiniWindowPositionButton.onClick = [this] {
    settingsManager.resetMiniWindowPosition();
    if (onResetMiniWindowPosition)
      onResetMiniWindowPosition();
  };
  addAndMakeVisible(resetMiniWindowPositionButton);

  // Setup Send RPN Button (Phase 25.2)
  addAndMakeVisible(sendRpnButton);
  sendRpnButton.setButtonText("Sync Range to Synth");
  sendRpnButton.onClick = [this] {
    int range = settingsManager.getPitchBendRange();
    DBG("Sending RPN Range " + juce::String(range) + " to all channels...");
    // Send RPN to all 16 MIDI channels
    for (int ch = 1; ch <= 16; ++ch) {
      midiEngine.sendPitchBendRangeRPN(ch, range);
    }
    // Show feedback (optional - could use a toast or label)
    sendRpnButton.setButtonText("Sent!");
    juce::MessageManager::callAsync(
        [this] { sendRpnButton.setButtonText("Sync Range to Synth"); });
  };
  sendRpnButton.setTooltip(
      "Some VST plugins and synths rely on RPN (Registered Parameter Number) "
      "messages for pitch bend range. This button sends the current pitch bend "
      "range setting as RPN data to all 16 MIDI channels, so your synth will "
      "use the same range.");

  // Mapping Colors (Phase 37)
  addAndMakeVisible(mappingColorsGroup);
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    addAndMakeVisible(typeColorButtons[i]);
    typeColorButtons[i].setButtonText(kMappingTypeNames[i]);
    ActionType type = kMappingTypeOrder[i];
    juce::TextButton *btn = &typeColorButtons[i];
    typeColorButtons[i].onClick = [this, type, btn] {
      launchColourSelectorForType(type, btn);
    };
  }
  refreshTypeColorButtons();

  // Setup Toggle Key Label and Button
  toggleKeyLabel.setText("Global MIDI Toggle Key:", juce::dontSendNotification);
  toggleKeyLabel.attachToComponent(&toggleKeyButton, true);
  addAndMakeVisible(toggleKeyLabel);
  addAndMakeVisible(toggleKeyButton);
  updateToggleKeyButtonText();
  toggleKeyButton.onClick = [this] {
    if (!isLearningToggleKey) {
      // Enter learn mode
      isLearningToggleKey = true;
      toggleKeyButton.setButtonText("Press any key...");
      rawInputManager.addListener(this);
    } else {
      // Cancel learn mode
      isLearningToggleKey = false;
      rawInputManager.removeListener(this);
      updateToggleKeyButtonText();
    }
  };

  // Setup Reset Toggle Key Button
  addAndMakeVisible(resetToggleKeyButton);
  resetToggleKeyButton.setButtonText("Reset");
  resetToggleKeyButton.onClick = [this] {
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon, "Reset Toggle Key",
        "Reset the MIDI toggle key to F12?", "Yes", "Cancel", this,
        juce::ModalCallbackFunction::create([this](int result) {
          if (result == 1) { // OK button
            settingsManager.setToggleKey(VK_F12);
            updateToggleKeyButtonText();
          }
        }));
  };

  // Setup Performance Mode Key Label and Button
  performanceModeKeyLabel.setText("Performance Mode Shortcut:",
                                  juce::dontSendNotification);
  performanceModeKeyLabel.attachToComponent(&performanceModeKeyButton, true);
  addAndMakeVisible(performanceModeKeyLabel);
  addAndMakeVisible(performanceModeKeyButton);
  updatePerformanceModeKeyButtonText();
  performanceModeKeyButton.onClick = [this] {
    if (!isLearningPerformanceModeKey) {
      // Enter learn mode
      isLearningPerformanceModeKey = true;
      performanceModeKeyButton.setButtonText("Press any key...");
      rawInputManager.addListener(this);
    } else {
      // Cancel learn mode
      isLearningPerformanceModeKey = false;
      rawInputManager.removeListener(this);
      updatePerformanceModeKeyButtonText();
    }
  };

  // Setup Reset Performance Mode Key Button
  addAndMakeVisible(resetPerformanceModeKeyButton);
  resetPerformanceModeKeyButton.setButtonText("Reset");
  resetPerformanceModeKeyButton.onClick = [this] {
    juce::AlertWindow::showOkCancelBox(
        juce::AlertWindow::QuestionIcon, "Reset Performance Mode Key",
        "Reset the Performance Mode shortcut to F11?", "Yes", "Cancel", this,
        juce::ModalCallbackFunction::create([this](int result) {
          if (result == 1) {                             // OK button
            settingsManager.setPerformanceModeKey(0x7A); // VK_F11
            updatePerformanceModeKeyButtonText();
          }
        }));
  };

  // Setup Studio Mode Toggle
  studioModeToggle.setButtonText("Studio Mode (Multi-Device Support)");
  studioModeToggle.setToggleState(settingsManager.isStudioMode(),
                                  juce::dontSendNotification);
  studioModeToggle.onClick = [this] {
    settingsManager.setStudioMode(studioModeToggle.getToggleState());
  };
  addAndMakeVisible(studioModeToggle);

  // Cap window refresh at 30 FPS
  capRefresh30FpsToggle.setButtonText("Cap window refresh at 30 FPS");
  capRefresh30FpsToggle.setToggleState(
      settingsManager.isCapWindowRefresh30Fps(), juce::dontSendNotification);
  capRefresh30FpsToggle.onClick = [this] {
    settingsManager.setCapWindowRefresh30Fps(
        capRefresh30FpsToggle.getToggleState());
  };
  addAndMakeVisible(capRefresh30FpsToggle);

  // Delay MIDI message (for binding in other software)
  delayMidiLabel.setText("Delay MIDI message (seconds):",
                         juce::dontSendNotification);
  delayMidiLabel.attachToComponent(&delayMidiCheckbox, true);
  addAndMakeVisible(delayMidiLabel);
  addAndMakeVisible(delayMidiCheckbox);
  addAndMakeVisible(delayMidiSlider);

  delayMidiCheckbox.setButtonText("Enable");
  delayMidiCheckbox.setToggleState(settingsManager.isDelayMidiEnabled(),
                                   juce::dontSendNotification);
  delayMidiCheckbox.onClick = [this] {
    bool enabled = delayMidiCheckbox.getToggleState();
    settingsManager.setDelayMidiEnabled(enabled);
    delayMidiSlider.setEnabled(enabled);
  };

  delayMidiSlider.setRange(1, 10, 1);
  delayMidiSlider.setTextValueSuffix(" s");
  delayMidiSlider.setValue(settingsManager.getDelayMidiSeconds(),
                           juce::dontSendNotification);
  delayMidiSlider.setEnabled(settingsManager.isDelayMidiEnabled());
  delayMidiLabel.setTooltip(
      "Useful when mapping MIDI in other software (e.g. DAW or MIDI learn): "
      "press a key here, then within the delay time select the target control "
      "in the other app; the MIDI message is sent after the delay so the "
      "listener can capture it for mapping.");
  delayMidiCheckbox.setTooltip(delayMidiLabel.getTooltip());
  delayMidiSlider.setTooltip(delayMidiLabel.getTooltip());
  delayMidiSlider.onValueChange = [this] {
    int value = static_cast<int>(delayMidiSlider.getValue());
    settingsManager.setDelayMidiSeconds(value);
  };
}

SettingsPanel::~SettingsPanel() {
  settingsManager.removeChangeListener(this);
  if (isLearningToggleKey || isLearningPerformanceModeKey) {
    rawInputManager.removeListener(this);
  }
}

void SettingsPanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &settingsManager) {
    refreshTypeColorButtons();
    studioModeToggle.setToggleState(settingsManager.isStudioMode(),
                                    juce::dontSendNotification);
    capRefresh30FpsToggle.setToggleState(
        settingsManager.isCapWindowRefresh30Fps(), juce::dontSendNotification);
    delayMidiCheckbox.setToggleState(settingsManager.isDelayMidiEnabled(),
                                     juce::dontSendNotification);
    delayMidiSlider.setValue(settingsManager.getDelayMidiSeconds(),
                             juce::dontSendNotification);
    delayMidiSlider.setEnabled(settingsManager.isDelayMidiEnabled());
    visXOpacitySlider.setValue(settingsManager.getVisualizerXOpacity() * 100.0,
                               juce::dontSendNotification);
    visYOpacitySlider.setValue(settingsManager.getVisualizerYOpacity() * 100.0,
                               juce::dontSendNotification);
  }
}

void SettingsPanel::refreshTypeColorButtons() {
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    juce::Colour c = settingsManager.getTypeColor(kMappingTypeOrder[i]);
    typeColorButtons[i].setColour(juce::TextButton::buttonColourId, c);
    typeColorButtons[i].repaint();
  }
}

void SettingsPanel::launchColourSelectorForType(ActionType type,
                                                juce::TextButton *button) {
  int flags = juce::ColourSelector::showColourspace |
              juce::ColourSelector::showSliders |
              juce::ColourSelector::showColourAtTop;
  auto *selector = new juce::ColourSelector(flags);
  selector->setName("Mapping Type Color");
  selector->setCurrentColour(settingsManager.getTypeColor(type));
  selector->setSize(400, 300);

  class Listener : public juce::ChangeListener {
  public:
    Listener(SettingsManager *mgr, juce::ColourSelector *s, ActionType t,
             juce::TextButton *b)
        : settingsMgr(mgr), selector(s), actionType(t), btn(b) {}
    void changeListenerCallback(juce::ChangeBroadcaster *src) override {
      if (src != selector || !settingsMgr || !btn)
        return;
      juce::Colour c = selector->getCurrentColour();
      settingsMgr->setTypeColor(actionType, c);
      btn->setColour(juce::TextButton::buttonColourId, c);
      btn->repaint();
    }

  private:
    SettingsManager *settingsMgr;
    juce::ColourSelector *selector;
    ActionType actionType;
    juce::TextButton *btn;
  };
  auto *listener = new Listener(&settingsManager, selector, type, button);
  selector->addChangeListener(listener);

  juce::CallOutBox::launchAsynchronously(
      std::unique_ptr<juce::Component>(selector), button->getScreenBounds(),
      this);
}

void SettingsPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                      bool isDown) {
  if (!isDown)
    return;

  if (isLearningToggleKey) {
    // Learn the toggle key
    settingsManager.setToggleKey(keyCode);
    isLearningToggleKey = false;
    rawInputManager.removeListener(this);
    updateToggleKeyButtonText();
    return;
  }

  if (isLearningPerformanceModeKey) {
    // Learn the performance mode key
    settingsManager.setPerformanceModeKey(keyCode);
    isLearningPerformanceModeKey = false;
    rawInputManager.removeListener(this);
    updatePerformanceModeKeyButtonText();
    return;
  }
}

void SettingsPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                    float value) {
  // Ignore axis events during key learning
}

void SettingsPanel::updateToggleKeyButtonText() {
  int toggleKey = settingsManager.getToggleKey();
  juce::String keyName = RawInputManager::getKeyName(toggleKey);
  toggleKeyButton.setButtonText("Toggle Key: " + keyName);
}

void SettingsPanel::updatePerformanceModeKeyButtonText() {
  int perfKey = settingsManager.getPerformanceModeKey();
  juce::String keyName = RawInputManager::getKeyName(perfKey);
  performanceModeKeyButton.setButtonText("Shortcut: " + keyName);
}

void SettingsPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void SettingsPanel::parentSizeChanged() {
  // When inside a Viewport, ensure we have a size so we paint (viewport doesn't
  // resize its viewed component)
  if (auto *vp = dynamic_cast<juce::Viewport *>(getParentComponent())) {
    int w = vp->getWidth();
    if (w > 0)
      setSize(w, 520);
  }
}

void SettingsPanel::resized() {
  // When inside a Viewport, use viewport width and set height to content height
  // so scrollbar appears
  int panelW = getWidth();
  if (auto *vp = dynamic_cast<juce::Viewport *>(getParentComponent())) {
    panelW = vp->getWidth();
    int controlHeight = 25;
    int spacing = 10;
    int visGroupHeight = controlHeight * 4 + spacing * 5 + 28;
    int groupHeight = controlHeight + spacing + 28;
    int contentH = 10 + (controlHeight + spacing) * 7 + visGroupHeight +
                   spacing + groupHeight + 24;
    setSize(panelW, contentH);
  }

  auto area = getLocalBounds().reduced(10);
  int labelWidth = 200;
  int controlHeight = 25;
  int spacing = 10;
  int topPadding = 10;
  int leftMargin = area.getX() + labelWidth;
  int width = area.getWidth() - labelWidth;

  int y = topPadding;
  pbRangeSlider.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  sendRpnButton.setBounds(leftMargin, y, 200, controlHeight);
  y += controlHeight + spacing;

  // Toggle Key row: button and reset button side by side
  int toggleKeyButtonWidth = 200;
  int resetButtonWidth = 80;
  toggleKeyButton.setBounds(leftMargin, y, toggleKeyButtonWidth, controlHeight);
  resetToggleKeyButton.setBounds(leftMargin + toggleKeyButtonWidth + spacing, y,
                                 resetButtonWidth, controlHeight);
  y += controlHeight + spacing;

  // Performance Mode Key row: button and reset button side by side
  performanceModeKeyButton.setBounds(leftMargin, y, toggleKeyButtonWidth,
                                     controlHeight);
  resetPerformanceModeKeyButton.setBounds(leftMargin + toggleKeyButtonWidth +
                                              spacing,
                                          y, resetButtonWidth, controlHeight);
  y += controlHeight + spacing;

  // Studio Mode Toggle
  studioModeToggle.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  // Cap window refresh at 30 FPS
  capRefresh30FpsToggle.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  // Delay MIDI message: checkbox and slider row
  int checkboxWidth = 80;
  int sliderWidth = width - checkboxWidth - spacing;
  delayMidiCheckbox.setBounds(leftMargin, y, checkboxWidth, controlHeight);
  delayMidiSlider.setBounds(leftMargin + checkboxWidth + spacing, y,
                            sliderWidth, controlHeight);
  y += controlHeight + spacing;

  // Visualizer group (X/Y opacity sliders + mini window toggle + reset button)
  int visGroupHeight = controlHeight * 4 + spacing * 5 + 28;
  visualizerGroup.setBounds(area.getX(), y, area.getWidth(), visGroupHeight);
  int visInnerX = area.getX() + 10;
  int visInnerY = y + 24;
  int visInnerW = area.getWidth() - 20;

  int visLabelW = 200;
  int visSliderW = visInnerW - visLabelW;
  visXOpacityLabel.setBounds(visInnerX, visInnerY, visLabelW, controlHeight);
  visXOpacitySlider.setBounds(visInnerX + visLabelW, visInnerY, visSliderW,
                              controlHeight);
  visInnerY += controlHeight + spacing;
  visYOpacityLabel.setBounds(visInnerX, visInnerY, visLabelW, controlHeight);
  visYOpacitySlider.setBounds(visInnerX + visLabelW, visInnerY, visSliderW,
                              controlHeight);
  visInnerY += controlHeight + spacing;
  showTouchpadInMiniWindowToggle.setBounds(visInnerX, visInnerY,
                                           visInnerW, controlHeight);
  visInnerY += controlHeight + spacing;
  resetMiniWindowPositionButton.setBounds(visInnerX, visInnerY, visInnerW,
                                          controlHeight);
  y += visGroupHeight + spacing;

  // Mapping Colors group (panel coordinates)
  int groupHeight = controlHeight + spacing + 28;
  mappingColorsGroup.setBounds(area.getX(), y, area.getWidth(), groupHeight);
  int innerX = area.getX() + 10;
  int innerY = y + 24;
  int innerW = area.getWidth() - 20;
  int btnW = (innerW - 4 * spacing) / 5;
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    typeColorButtons[i].setBounds(innerX + (int)i * (btnW + spacing), innerY,
                                  btnW, controlHeight);
  }
}
