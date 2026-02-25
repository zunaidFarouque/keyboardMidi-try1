#include "SettingsPanel.h"
#include "MappingTypes.h"
#include "CrashLogger.h"
#include <windows.h>

namespace {
struct LabelEditorRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
  static constexpr int labelWidth = 220;
  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelWidth));
    if (editor)
      editor->setBounds(r);
  }
};

const std::array<ActionType, 3> kMappingTypeOrder = {
    ActionType::Note, ActionType::Expression, ActionType::Command};
const char *kMappingTypeNames[] = {"Note", "Expression", "Command"};
} // namespace

SettingsPanel::SeparatorComponent::SeparatorComponent(
    const juce::String &label, juce::Justification justification)
    : labelText(label), textAlign(justification) {}

void SettingsPanel::SeparatorComponent::paint(juce::Graphics &g) {
  auto bounds = getLocalBounds();
  const float centreY = bounds.getCentreY();
  const int lineY = static_cast<int>(centreY - 0.5f);
  const int lineHeight = 1;
  const int pad = 5;

  g.setColour(juce::Colours::grey);

  if (labelText.isEmpty()) {
    g.fillRect(bounds.getX(), lineY, bounds.getWidth(), lineHeight);
    return;
  }

  juce::Font font(14.0f, juce::Font::bold);
  const int textBlockWidth = font.getStringWidth(labelText) + pad * 2;
  int textLeft;
  int textRight;

  if ((textAlign.getFlags() & juce::Justification::centredLeft) != 0) {
    textLeft = bounds.getX();
    textRight = textLeft + textBlockWidth;
  } else if ((textAlign.getFlags() & juce::Justification::centredRight) != 0) {
    textRight = bounds.getRight();
    textLeft = textRight - textBlockWidth;
  } else {
    textLeft = bounds.getCentreX() - textBlockWidth / 2;
    textRight = textLeft + textBlockWidth;
  }

  g.setColour(juce::Colours::lightgrey);
  g.setFont(font);
  g.drawText(labelText, textLeft, bounds.getY(), textBlockWidth,
             bounds.getHeight(), textAlign, true);

  g.setColour(juce::Colours::grey);
  if (textLeft - pad > bounds.getX())
    g.fillRect(bounds.getX(), lineY, textLeft - pad - bounds.getX(),
               lineHeight);
  if (textRight + pad < bounds.getRight())
    g.fillRect(textRight + pad, lineY, bounds.getRight() - (textRight + pad),
               lineHeight);
}

void SettingsPanel::initialize() { settingsManager.addChangeListener(this); }

SettingsPanel::SettingsPanel(SettingsManager &settingsMgr, MidiEngine &midiEng,
                             RawInputManager &rawInputMgr)
    : settingsManager(settingsMgr), midiEngine(midiEng),
      rawInputManager(rawInputMgr) {
  // Phase 42: addChangeListener moved to initialize()
  rebuildUI();
}

SettingsPanel::~SettingsPanel() {
  settingsManager.removeChangeListener(this);
  if (isLearningToggleKey || isLearningPerformanceModeKey) {
    rawInputManager.removeListener(this);
  }
}

void SettingsPanel::rebuildUI() {
  // Reset non-owning pointers
  pbRangeSlider = nullptr;
  visXOpacitySlider = nullptr;
  visYOpacitySlider = nullptr;
  showTouchpadInMiniWindowToggle = nullptr;
  hideCursorInPerformanceModeToggle = nullptr;
  studioModeToggle = nullptr;
  capRefresh30FpsToggle = nullptr;
  delayMidiCheckbox = nullptr;
  delayMidiSlider = nullptr;
  rememberUiStateToggle = nullptr;
  debugModeToggle = nullptr;
  toggleKeyButton = nullptr;
  resetToggleKeyButton = nullptr;
  performanceModeKeyButton = nullptr;
  resetPerformanceModeKeyButton = nullptr;
  for (auto &btn : typeColorButtons)
    btn = nullptr;

  // Remove previous children owned via uiRows
  for (auto &row : uiRows) {
    for (auto &item : row.items) {
      if (item.component)
        removeChildComponent(item.component.get());
    }
  }
  uiRows.clear();

  // Build from schema
  InspectorSchema schema = SettingsDefinition::getSchema();
  bool addedSyncRow = false;
  for (const auto &def : schema) {
    if (def.controlType == InspectorControl::Type::Separator) {
      UiRow row;
      row.isSeparatorRow = true;
      auto sep =
          std::make_unique<SeparatorComponent>(def.label, def.separatorAlign);
      addAndMakeVisible(*sep);
      row.items.push_back({std::move(sep), 1.0f, false});
      uiRows.push_back(std::move(row));
      continue;
    }

    if (!def.sameLine || uiRows.empty() || uiRows.back().isSeparatorRow) {
      UiRow row;
      row.isSeparatorRow = false;
      uiRows.push_back(std::move(row));
    }

    createControl(def, uiRows.back());

    // Immediately after the pitch bend range slider, add the Sync button row
    if (!addedSyncRow && def.propertyId == "pitchBendRange") {
      UiRow syncRow;
      syncRow.isSeparatorRow = false;
      auto btn = std::make_unique<juce::TextButton>("Sync Range to Synth");
      juce::TextButton *btnPtr = btn.get();
      btn->onClick = [this, btnPtr]() {
        int range = settingsManager.getPitchBendRange();
        DBG("Sending RPN Range " + juce::String(range) + " to all channels...");
        for (int ch = 1; ch <= 16; ++ch)
          midiEngine.sendPitchBendRangeRPN(ch, range);
        btnPtr->setButtonText("Sent!");
        juce::MessageManager::callAsync(
            [btnPtr]() { btnPtr->setButtonText("Sync Range to Synth"); });
      };
      btn->setTooltip(
          "Some VST plugins and synths rely on RPN (Registered Parameter Number) "
          "messages for pitch bend range. This button sends the current pitch "
          "bend range setting as RPN data to all 16 MIDI channels.");
      addAndMakeVisible(*btn);
      syncRow.items.push_back({std::move(btn), 1.0f, false});
      uiRows.push_back(std::move(syncRow));
      addedSyncRow = true;
    }
  }

  // --- Custom rows not described by schema ---

  // Reset UI Layout row (under UI & Layout section)
  {
    UiRow row;
    row.isSeparatorRow = false;
    auto btn = std::make_unique<juce::TextButton>("Reset UI Layout");
    juce::TextButton *btnPtr = btn.get();
    btn->setTooltip(
        "Reset window positions, tabs, and panels to defaults, then exit MIDIQy.");
    btn->onClick = [this, btnPtr]() {
      juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::WarningIcon, "Reset UI Layout",
          "This will reset window positions, visible panels, and tab layout "
          "to their defaults, then exit MIDIQy.\n\n"
          "After it closes, please start MIDIQy again.\n\n"
          "Continue with reset and exit?",
          "Reset", "Cancel", this,
          juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1 && onResetUiLayout) {
              onResetUiLayout();
            }
          }));
    };
    addAndMakeVisible(*btn);
    row.items.push_back({std::move(btn), 1.0f, false});
    uiRows.push_back(std::move(row));
  }

  // Global Shortcuts section header
  {
    UiRow sepRow;
    sepRow.isSeparatorRow = true;
    auto sep = std::make_unique<SeparatorComponent>(
        "Global Shortcuts", juce::Justification::centredLeft);
    addAndMakeVisible(*sep);
    sepRow.items.push_back({std::move(sep), 1.0f, false});
    uiRows.push_back(std::move(sepRow));
  }

  // Key-learning rows (Toggle key + Performance mode key)
  {
    // Toggle key row: [label+button] [reset]
    UiRow row;
    row.isSeparatorRow = false;

    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText("Global MIDI Toggle Key:",
                            juce::dontSendNotification);
    auto setBtn = std::make_unique<juce::TextButton>();
    toggleKeyButton = setBtn.get();
    updateToggleKeyButtonText();
    setBtn->onClick = [this]() {
      if (!isLearningToggleKey) {
        isLearningToggleKey = true;
        if (toggleKeyButton)
          toggleKeyButton->setButtonText("Press any key...");
        rawInputManager.addListener(this);
      } else {
        isLearningToggleKey = false;
        rawInputManager.removeListener(this);
        updateToggleKeyButtonText();
      }
    };
    rowComp->editor = std::move(setBtn);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);
    addAndMakeVisible(*rowComp);
    row.items.push_back({std::move(rowComp), 0.7f, false});

    auto resetBtn = std::make_unique<juce::TextButton>("Reset");
    resetToggleKeyButton = resetBtn.get();
    resetBtn->onClick = [this]() {
      juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::QuestionIcon, "Reset Toggle Key",
          "Reset the MIDI toggle key to F12?", "Yes", "Cancel", this,
          juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1) {
              settingsManager.setToggleKey(VK_F12);
              updateToggleKeyButtonText();
            }
          }));
    };
    addAndMakeVisible(*resetBtn);
    row.items.push_back({std::move(resetBtn), 0.3f, true});

    uiRows.push_back(std::move(row));
  }
  {
    // Performance mode key row: [label+button] [reset]
    UiRow row;
    row.isSeparatorRow = false;

    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText("Performance Mode Shortcut:",
                            juce::dontSendNotification);
    auto setBtn = std::make_unique<juce::TextButton>();
    performanceModeKeyButton = setBtn.get();
    updatePerformanceModeKeyButtonText();
    setBtn->onClick = [this]() {
      if (!isLearningPerformanceModeKey) {
        isLearningPerformanceModeKey = true;
        if (performanceModeKeyButton)
          performanceModeKeyButton->setButtonText("Press any key...");
        rawInputManager.addListener(this);
      } else {
        isLearningPerformanceModeKey = false;
        rawInputManager.removeListener(this);
        updatePerformanceModeKeyButtonText();
      }
    };
    rowComp->editor = std::move(setBtn);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);
    addAndMakeVisible(*rowComp);
    row.items.push_back({std::move(rowComp), 0.7f, false});

    auto resetBtn = std::make_unique<juce::TextButton>("Reset");
    resetPerformanceModeKeyButton = resetBtn.get();
    resetBtn->onClick = [this]() {
      juce::AlertWindow::showOkCancelBox(
          juce::AlertWindow::QuestionIcon, "Reset Performance Mode Key",
          "Reset the Performance Mode shortcut to F11?", "Yes", "Cancel", this,
          juce::ModalCallbackFunction::create([this](int result) {
            if (result == 1) {
              settingsManager.setPerformanceModeKey(0x7A); // VK_F11
              updatePerformanceModeKeyButtonText();
            }
          }));
    };
    addAndMakeVisible(*resetBtn);
    row.items.push_back({std::move(resetBtn), 0.3f, true});

    uiRows.push_back(std::move(row));
  }

  // Mapping colors row
  {
    UiRow row;
    row.isSeparatorRow = false;

    auto sep =
        std::make_unique<SeparatorComponent>("Mapping Colors",
                                             juce::Justification::centredLeft);
    addAndMakeVisible(*sep);
    UiRow sepRow;
    sepRow.isSeparatorRow = true;
    sepRow.items.push_back({std::move(sep), 1.0f, false});
    uiRows.push_back(std::move(sepRow));

    UiRow buttonRow;
    buttonRow.isSeparatorRow = false;
    for (size_t i = 0; i < typeColorButtons.size(); ++i) {
      auto btn = std::make_unique<juce::TextButton>(kMappingTypeNames[i]);
      juce::TextButton *btnPtr = btn.get();
      typeColorButtons[i] = btnPtr;
      ActionType type = kMappingTypeOrder[i];
      btn->onClick = [this, type, btnPtr]() {
        launchColourSelectorForType(type, btnPtr);
      };
      addAndMakeVisible(*btn);
      buttonRow.items.push_back({std::move(btn), 1.0f, false});
    }
    uiRows.push_back(std::move(buttonRow));
    refreshTypeColorButtons();
  }

  resized();
}

void SettingsPanel::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &settingsManager) {
    refreshTypeColorButtons();
    if (studioModeToggle)
      studioModeToggle->setToggleState(settingsManager.isStudioMode(),
                                       juce::dontSendNotification);
    if (capRefresh30FpsToggle)
      capRefresh30FpsToggle->setToggleState(
          settingsManager.isCapWindowRefresh30Fps(),
          juce::dontSendNotification);
    if (delayMidiCheckbox)
      delayMidiCheckbox->setToggleState(settingsManager.isDelayMidiEnabled(),
                                        juce::dontSendNotification);
    if (delayMidiSlider) {
      delayMidiSlider->setValue(settingsManager.getDelayMidiSeconds(),
                                juce::dontSendNotification);
      delayMidiSlider->setEnabled(settingsManager.isDelayMidiEnabled());
    }
    if (visXOpacitySlider)
      visXOpacitySlider->setValue(
          settingsManager.getVisualizerXOpacity() * 100.0,
          juce::dontSendNotification);
    if (visYOpacitySlider)
      visYOpacitySlider->setValue(
          settingsManager.getVisualizerYOpacity() * 100.0,
          juce::dontSendNotification);
    if (showTouchpadInMiniWindowToggle)
      showTouchpadInMiniWindowToggle->setToggleState(
          settingsManager.getShowTouchpadVisualizerInMiniWindow(),
          juce::dontSendNotification);
    if (hideCursorInPerformanceModeToggle)
      hideCursorInPerformanceModeToggle->setToggleState(
          settingsManager.getHideCursorInPerformanceMode(),
          juce::dontSendNotification);
    if (rememberUiStateToggle)
      rememberUiStateToggle->setToggleState(
          settingsManager.getRememberUiState(), juce::dontSendNotification);
    if (debugModeToggle)
      debugModeToggle->setToggleState(
          settingsManager.getDebugModeEnabled(), juce::dontSendNotification);
  }
}

void SettingsPanel::refreshTypeColorButtons() {
  for (size_t i = 0; i < typeColorButtons.size(); ++i) {
    if (auto *btn = typeColorButtons[i]) {
      juce::Colour c = settingsManager.getTypeColor(kMappingTypeOrder[i]);
      btn->setColour(juce::TextButton::buttonColourId, c);
      btn->repaint();
    }
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

juce::var SettingsPanel::getSettingsValue(const juce::String &propertyId) const {
  if (propertyId == "pitchBendRange")
    return juce::var(settingsManager.getPitchBendRange());
  if (propertyId == "visualizerXOpacityPercent")
    return juce::var(static_cast<double>(settingsManager.getVisualizerXOpacity() * 100.0f));
  if (propertyId == "visualizerYOpacityPercent")
    return juce::var(static_cast<double>(settingsManager.getVisualizerYOpacity() * 100.0f));
  if (propertyId == "showTouchpadVisualizerInMiniWindow")
    return juce::var(settingsManager.getShowTouchpadVisualizerInMiniWindow());
  if (propertyId == "hideCursorInPerformanceMode")
    return juce::var(settingsManager.getHideCursorInPerformanceMode());
  if (propertyId == "studioMode")
    return juce::var(settingsManager.isStudioMode());
  if (propertyId == "capWindowRefresh30Fps")
    return juce::var(settingsManager.isCapWindowRefresh30Fps());
  if (propertyId == "delayMidiEnabled")
    return juce::var(settingsManager.isDelayMidiEnabled());
  if (propertyId == "delayMidiSeconds")
    return juce::var(settingsManager.getDelayMidiSeconds());
  if (propertyId == "rememberUiState")
    return juce::var(settingsManager.getRememberUiState());
  if (propertyId == "debugModeEnabled")
    return juce::var(settingsManager.getDebugModeEnabled());
  return juce::var();
}

void SettingsPanel::applySettingsValue(const juce::String &propertyId,
                                       const juce::var &value) {
  if (propertyId == "pitchBendRange") {
    int v = static_cast<int>(value);
    settingsManager.setPitchBendRange(v);
  } else if (propertyId == "visualizerXOpacityPercent") {
    double v = static_cast<double>(value);
    settingsManager.setVisualizerXOpacity(
        static_cast<float>(juce::jlimit(0.0, 100.0, v) / 100.0));
  } else if (propertyId == "visualizerYOpacityPercent") {
    double v = static_cast<double>(value);
    settingsManager.setVisualizerYOpacity(
        static_cast<float>(juce::jlimit(0.0, 100.0, v) / 100.0));
  } else if (propertyId == "showTouchpadVisualizerInMiniWindow") {
    settingsManager.setShowTouchpadVisualizerInMiniWindow(
        static_cast<bool>(value));
  } else if (propertyId == "hideCursorInPerformanceMode") {
    settingsManager.setHideCursorInPerformanceMode(static_cast<bool>(value));
  } else if (propertyId == "studioMode") {
    settingsManager.setStudioMode(static_cast<bool>(value));
  } else if (propertyId == "capWindowRefresh30Fps") {
    settingsManager.setCapWindowRefresh30Fps(static_cast<bool>(value));
  } else if (propertyId == "delayMidiEnabled") {
    bool enabled = static_cast<bool>(value);
    settingsManager.setDelayMidiEnabled(enabled);
    if (delayMidiSlider)
      delayMidiSlider->setEnabled(enabled);
  } else if (propertyId == "delayMidiSeconds") {
    int v = static_cast<int>(value);
    settingsManager.setDelayMidiSeconds(v);
  } else if (propertyId == "rememberUiState") {
    settingsManager.setRememberUiState(static_cast<bool>(value));
  } else if (propertyId == "debugModeEnabled") {
    bool enabled = static_cast<bool>(value);
    settingsManager.setDebugModeEnabled(enabled);
    CrashLogger::addBreadcrumb("Settings: Debug mode " +
                               juce::String(enabled ? "ON" : "OFF"));
  }
}

void SettingsPanel::createControl(const InspectorControl &def, UiRow &currentRow) {
  const juce::String propId = def.propertyId;
  juce::var currentVal = getSettingsValue(propId);

  auto addItem = [this, &currentRow](std::unique_ptr<juce::Component> comp,
                                     float weight, bool isAuto = false) {
    if (!comp)
      return;
    addAndMakeVisible(*comp);
    currentRow.items.push_back({std::move(comp), weight, isAuto});
  };

  switch (def.controlType) {
  case InspectorControl::Type::Slider: {
    auto sl = std::make_unique<juce::Slider>();
    sl->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      sl->setTextValueSuffix(" " + def.suffix);

    if (!currentVal.isVoid())
      sl->setValue(static_cast<double>(currentVal), juce::dontSendNotification);

    juce::Slider *slPtr = sl.get();

    if (propId == "pitchBendRange")
      pbRangeSlider = slPtr;
    else if (propId == "visualizerXOpacityPercent")
      visXOpacitySlider = slPtr;
    else if (propId == "visualizerYOpacityPercent")
      visYOpacitySlider = slPtr;
    else if (propId == "delayMidiSeconds")
      delayMidiSlider = slPtr;

    sl->onValueChange = [this, propId, def, slPtr]() {
      double v = slPtr->getValue();
      juce::var valueToSet =
          (def.step >= 1.0) ? juce::var(static_cast<int>(std::round(v)))
                            : juce::var(v);
      applySettingsValue(propId, valueToSet);
    };

    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->editor = std::move(sl);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);

    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Toggle: {
    auto tb = std::make_unique<juce::ToggleButton>();
    bool state = (!currentVal.isVoid() && static_cast<bool>(currentVal));
    tb->setToggleState(state, juce::dontSendNotification);
    juce::ToggleButton *tbPtr = tb.get();

    if (propId == "showTouchpadVisualizerInMiniWindow")
      showTouchpadInMiniWindowToggle = tbPtr;
    else if (propId == "hideCursorInPerformanceMode")
      hideCursorInPerformanceModeToggle = tbPtr;
    else if (propId == "studioMode")
      studioModeToggle = tbPtr;
    else if (propId == "capWindowRefresh30Fps")
      capRefresh30FpsToggle = tbPtr;
    else if (propId == "delayMidiEnabled")
      delayMidiCheckbox = tbPtr;
    else if (propId == "rememberUiState")
      rememberUiStateToggle = tbPtr;
    else if (propId == "debugModeEnabled")
      debugModeToggle = tbPtr;

    tb->onClick = [this, propId, tbPtr]() {
      applySettingsValue(propId, juce::var(tbPtr->getToggleState()));
    };

    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->editor = std::move(tb);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);

    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  default:
    break;
  }
}

int SettingsPanel::getRequiredHeight() const {
  const int controlHeight = 25;
  const int separatorHeight = 15;
  const int separatorTopMargin = 12;
  const int spacing = 4;
  const int topPadding = 4;
  const int bottomPadding = 4;

  int total = topPadding + bottomPadding;
  for (const auto &row : uiRows) {
    if (row.items.empty())
      continue;
    if (row.isSeparatorRow)
      total += separatorTopMargin;
    total += (row.isSeparatorRow ? separatorHeight : controlHeight) + spacing;
  }
  return total;
}

void SettingsPanel::updateToggleKeyButtonText() {
  int toggleKey = settingsManager.getToggleKey();
  juce::String keyName = RawInputManager::getKeyName(toggleKey);
  if (toggleKeyButton)
    toggleKeyButton->setButtonText("Toggle Key: " + keyName);
}

void SettingsPanel::updatePerformanceModeKeyButtonText() {
  int perfKey = settingsManager.getPerformanceModeKey();
  juce::String keyName = RawInputManager::getKeyName(perfKey);
  if (performanceModeKeyButton)
    performanceModeKeyButton->setButtonText("Shortcut: " + keyName);
}

void SettingsPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void SettingsPanel::parentSizeChanged() {
  // When inside a Viewport, ensure we match its width so we paint correctly.
  if (auto *vp = dynamic_cast<juce::Viewport *>(getParentComponent())) {
    int w = vp->getWidth();
    if (w > 0)
      setSize(w, getRequiredHeight());
  }
}

void SettingsPanel::resized() {
  const int rowHeight = 25;
  const int separatorRowHeight = 15;
  const int spacing = 4;
  const int topPadding = 4;

  auto bounds = getLocalBounds().reduced(8);

  int y = bounds.getY() + topPadding;
  for (auto &row : uiRows) {
    if (row.items.empty())
      continue;

    if (row.isSeparatorRow)
      y += 12;

    const int h = row.isSeparatorRow ? separatorRowHeight : rowHeight;
    const int totalAvailable = bounds.getWidth();
    int usedWidth = 0;
    float totalWeight = 0.0f;

    for (auto &item : row.items) {
      if (item.isAutoWidth) {
        usedWidth += 100;
      } else {
        totalWeight += item.weight;
      }
    }

    int remainingWidth = std::max(0, totalAvailable - usedWidth);
    int x = bounds.getX();

    for (auto &item : row.items) {
      int w = 0;
      if (item.isAutoWidth) {
        w = 100;
      } else {
        if (totalWeight > 0.0f)
          w = static_cast<int>((item.weight / totalWeight) * remainingWidth);
        else
          w = remainingWidth;
      }
      if (item.component)
        item.component->setBounds(x, y, w, h);
      x += w;
    }

    y += h + spacing;
  }

  setSize(getWidth(), y + 4);
}
