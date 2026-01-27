#include "SettingsPanel.h"
#include "MappingTypes.h"
#include <windows.h>

namespace {
const std::array<ActionType, 5> kMappingTypeOrder = {
    ActionType::Note, ActionType::CC, ActionType::Command, ActionType::Macro, ActionType::Envelope};
const char* kMappingTypeNames[] = {"Note", "CC", "Command", "Macro", "Envelope"};
} // namespace

SettingsPanel::SettingsPanel(SettingsManager& settingsMgr, MidiEngine& midiEng, RawInputManager& rawInputMgr)
    : settingsManager(settingsMgr), midiEngine(midiEng), rawInputManager(rawInputMgr),
      mappingColorsGroup("Mapping Colors", "Mapping Colors") {
  settingsManager.addChangeListener(this);

  // Setup PB Range Slider
  pbRangeLabel.setText("Global Pitch Bend Range (+/- semitones):", juce::dontSendNotification);
  pbRangeLabel.attachToComponent(&pbRangeSlider, true);
  addAndMakeVisible(pbRangeLabel);
  addAndMakeVisible(pbRangeSlider);
  
  pbRangeSlider.setRange(1, 96, 1);
  pbRangeSlider.setTextValueSuffix(" semitones");
  pbRangeSlider.setValue(settingsManager.getPitchBendRange(), juce::dontSendNotification);
  
  pbRangeSlider.onValueChange = [this] {
    int value = static_cast<int>(pbRangeSlider.getValue());
    settingsManager.setPitchBendRange(value);
  };

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
    juce::MessageManager::callAsync([this] {
      sendRpnButton.setButtonText("Sync Range to Synth");
    });
  };
  
  // Mapping Colors (Phase 37)
  addAndMakeVisible(mappingColorsGroup);
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    addAndMakeVisible(typeColorButtons[i]);
    typeColorButtons[i].setButtonText(kMappingTypeNames[i]);
    ActionType type = kMappingTypeOrder[i];
    juce::TextButton* btn = &typeColorButtons[i];
    typeColorButtons[i].onClick = [this, type, btn] { launchColourSelectorForType(type, btn); };
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
        juce::AlertWindow::QuestionIcon,
        "Reset Toggle Key",
        "Reset the MIDI toggle key to F12?",
        "Yes",
        "Cancel",
        this,
        juce::ModalCallbackFunction::create([this](int result) {
          if (result == 1) { // OK button
            settingsManager.setToggleKey(VK_F12);
            updateToggleKeyButtonText();
          }
        }));
  };

  // Setup Studio Mode Toggle
  studioModeToggle.setButtonText("Studio Mode (Multi-Device Support)");
  studioModeToggle.setToggleState(settingsManager.isStudioMode(), juce::dontSendNotification);
  studioModeToggle.onClick = [this] {
    settingsManager.setStudioMode(studioModeToggle.getToggleState());
  };
  addAndMakeVisible(studioModeToggle);
}

SettingsPanel::~SettingsPanel() {
  settingsManager.removeChangeListener(this);
  if (isLearningToggleKey) {
    rawInputManager.removeListener(this);
  }
}

void SettingsPanel::changeListenerCallback(juce::ChangeBroadcaster* source) {
  if (source == &settingsManager) {
    refreshTypeColorButtons();
    // Update Studio Mode toggle state
    studioModeToggle.setToggleState(settingsManager.isStudioMode(), juce::dontSendNotification);
  }
}

void SettingsPanel::refreshTypeColorButtons() {
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    juce::Colour c = settingsManager.getTypeColor(kMappingTypeOrder[i]);
    typeColorButtons[i].setColour(juce::TextButton::buttonColourId, c);
    typeColorButtons[i].repaint();
  }
}

void SettingsPanel::launchColourSelectorForType(ActionType type, juce::TextButton* button) {
  int flags = juce::ColourSelector::showColourspace |
              juce::ColourSelector::showSliders |
              juce::ColourSelector::showColourAtTop;
  auto* selector = new juce::ColourSelector(flags);
  selector->setName("Mapping Type Color");
  selector->setCurrentColour(settingsManager.getTypeColor(type));
  selector->setSize(400, 300);

  class Listener : public juce::ChangeListener {
  public:
    Listener(SettingsManager* mgr, juce::ColourSelector* s, ActionType t, juce::TextButton* b)
        : settingsMgr(mgr), selector(s), actionType(t), btn(b) {}
    void changeListenerCallback(juce::ChangeBroadcaster* src) override {
      if (src != selector || !settingsMgr || !btn) return;
      juce::Colour c = selector->getCurrentColour();
      settingsMgr->setTypeColor(actionType, c);
      btn->setColour(juce::TextButton::buttonColourId, c);
      btn->repaint();
    }
  private:
    SettingsManager* settingsMgr;
    juce::ColourSelector* selector;
    ActionType actionType;
    juce::TextButton* btn;
  };
  auto* listener = new Listener(&settingsManager, selector, type, button);
  selector->addChangeListener(listener);

  juce::CallOutBox::launchAsynchronously(
      std::unique_ptr<juce::Component>(selector), button->getScreenBounds(), this);
}

void SettingsPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  if (!isLearningToggleKey || !isDown) {
    return;
  }
  
  // Learn the key
  settingsManager.setToggleKey(keyCode);
  isLearningToggleKey = false;
  rawInputManager.removeListener(this);
  updateToggleKeyButtonText();
}

void SettingsPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events during key learning
}

void SettingsPanel::updateToggleKeyButtonText() {
  int toggleKey = settingsManager.getToggleKey();
  juce::String keyName = RawInputManager::getKeyName(toggleKey);
  toggleKeyButton.setButtonText("Toggle Key: " + keyName);
}

void SettingsPanel::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void SettingsPanel::resized() {
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
  resetToggleKeyButton.setBounds(leftMargin + toggleKeyButtonWidth + spacing, y, resetButtonWidth, controlHeight);
  y += controlHeight + spacing;

  // Studio Mode Toggle
  studioModeToggle.setBounds(leftMargin, y, width, controlHeight);
  y += controlHeight + spacing;

  // Mapping Colors group (panel coordinates)
  int groupHeight = controlHeight + spacing + 28;
  mappingColorsGroup.setBounds(area.getX(), y, area.getWidth(), groupHeight);
  int innerX = area.getX() + 10;
  int innerY = y + 24;
  int innerW = area.getWidth() - 20;
  int btnW = (innerW - 4 * spacing) / 5;
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    typeColorButtons[i].setBounds(
        innerX + (int)i * (btnW + spacing), innerY, btnW, controlHeight);
  }
}
