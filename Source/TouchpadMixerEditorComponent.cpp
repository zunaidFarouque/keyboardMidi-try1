#include "TouchpadMixerEditorComponent.h"
#include "TouchpadMixerDefinition.h"

namespace {
struct LabelEditorRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
  static constexpr int labelWidth = 100;
  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelWidth));
    if (editor)
      editor->setBounds(r);
  }
};

class LabeledControl : public juce::Component {
public:
  LabeledControl(std::unique_ptr<juce::Label> l,
                 std::unique_ptr<juce::Component> c)
      : label(std::move(l)), editor(std::move(c)) {
    if (label)
      addAndMakeVisible(*label);
    if (editor)
      addAndMakeVisible(*editor);
  }
  void resized() override {
    auto area = getLocalBounds();
    if (label) {
      int textWidth = label->getFont().getStringWidth(label->getText()) + 10;
      label->setBounds(area.removeFromLeft(textWidth));
    }
    if (editor)
      editor->setBounds(area);
  }
  int getIdealWidth() const {
    int w = 0;
    if (label)
      w += label->getFont().getStringWidth(label->getText()) + 10;
    w += 30;
    return w;
  }

private:
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
};
} // namespace

juce::PopupMenu::Options
TouchpadMixerEditorComponent::ComboPopupLAF::getOptionsForComboBoxPopupMenu(
    juce::ComboBox &box, juce::Label &label) {
  auto opts = LookAndFeel_V4::getOptionsForComboBoxPopupMenu(box, label);
  if (juce::Component *top = box.getTopLevelComponent())
    return opts.withParentComponent(top);
  return opts;
}

TouchpadMixerEditorComponent::SeparatorComponent::SeparatorComponent(
    const juce::String &label, juce::Justification justification)
    : labelText(label), textAlign(justification) {}

void TouchpadMixerEditorComponent::SeparatorComponent::paint(
    juce::Graphics &g) {
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
  int textLeft = bounds.getCentreX() - textBlockWidth / 2;
  g.setColour(juce::Colours::lightgrey);
  g.setFont(font);
  g.drawText(labelText, textLeft, bounds.getY(), textBlockWidth,
             bounds.getHeight(), textAlign, true);
  if (textLeft - pad > bounds.getX())
    g.fillRect(bounds.getX(), lineY, textLeft - pad - bounds.getX(),
               lineHeight);
  if (textLeft + textBlockWidth + pad < bounds.getRight())
    g.fillRect(textLeft + textBlockWidth + pad, lineY,
               bounds.getRight() - (textLeft + textBlockWidth + pad),
               lineHeight);
}

TouchpadMixerEditorComponent::TouchpadMixerEditorComponent(
    TouchpadMixerManager *mgr)
    : manager(mgr) {
  setLookAndFeel(&comboPopupLAF);
}

TouchpadMixerEditorComponent::~TouchpadMixerEditorComponent() {
  setLookAndFeel(nullptr);
  for (auto &row : uiRows) {
    for (auto &item : row.items) {
      if (item.component)
        removeChildComponent(item.component.get());
    }
  }
  uiRows.clear();
}

void TouchpadMixerEditorComponent::setLayout(int index,
                                            const TouchpadMixerConfig *config) {
  selectedIndex = index;
  if (config)
    currentConfig = *config;
  else
    currentConfig = TouchpadMixerConfig();
  rebuildUI();
  setEnabled(config != nullptr);
}

juce::var TouchpadMixerEditorComponent::getConfigValue(
    const juce::String &propertyId) const {
  if (propertyId == "type") {
    switch (currentConfig.type) {
    case TouchpadType::Mixer:
      return juce::var(1);
    case TouchpadType::DrumPad:
      return juce::var(2);
    case TouchpadType::ChordPad:
      return juce::var(3);
    default:
      return juce::var(1);
    }
  }
  if (propertyId == "name")
    return juce::var(juce::String(currentConfig.name));
  if (propertyId == "layerId")
    return juce::var(currentConfig.layerId + 1); // combo IDs 1..9
  if (propertyId == "layoutGroupId")
    return juce::var(currentConfig.layoutGroupId);
  if (propertyId == "numFaders")
    return juce::var(currentConfig.numFaders);
  if (propertyId == "ccStart")
    return juce::var(currentConfig.ccStart);
  if (propertyId == "midiChannel")
    return juce::var(currentConfig.midiChannel);
  if (propertyId == "inputMin")
    return juce::var(currentConfig.inputMin);
  if (propertyId == "inputMax")
    return juce::var(currentConfig.inputMax);
  if (propertyId == "outputMin")
    return juce::var(currentConfig.outputMin);
  if (propertyId == "outputMax")
    return juce::var(currentConfig.outputMax);
  if (propertyId == "quickPrecision")
    return juce::var(currentConfig.quickPrecision ==
                             TouchpadMixerQuickPrecision::Precision
                         ? 2
                         : 1);
  if (propertyId == "absRel")
    return juce::var(currentConfig.absRel == TouchpadMixerAbsRel::Relative ? 2
                                                                           : 1);
  if (propertyId == "lockFree")
    return juce::var(currentConfig.lockFree == TouchpadMixerLockFree::Free ? 2
                                                                           : 1);
  if (propertyId == "muteButtonsEnabled")
    return juce::var(currentConfig.muteButtonsEnabled);
  if (propertyId == "drumPadRows")
    return juce::var(currentConfig.drumPadRows);
  if (propertyId == "drumPadColumns")
    return juce::var(currentConfig.drumPadColumns);
  if (propertyId == "drumPadMidiNoteStart")
    return juce::var(currentConfig.drumPadMidiNoteStart);
  if (propertyId == "drumPadBaseVelocity")
    return juce::var(currentConfig.drumPadBaseVelocity);
  if (propertyId == "drumPadVelocityRandom")
    return juce::var(currentConfig.drumPadVelocityRandom);
  if (propertyId == "drumPadLayoutMode") {
    switch (currentConfig.drumPadLayoutMode) {
    case DrumPadLayoutMode::Classic:
      return juce::var(1);
    case DrumPadLayoutMode::HarmonicGrid:
      return juce::var(2);
    default:
      return juce::var(1);
    }
  }
  if (propertyId == "harmonicRowInterval")
    return juce::var(currentConfig.harmonicRowInterval);
  if (propertyId == "harmonicUseScaleFilter")
    return juce::var(currentConfig.harmonicUseScaleFilter);
  if (propertyId == "chordPadPreset")
    return juce::var(currentConfig.chordPadPreset);
  if (propertyId == "chordPadLatchMode")
    return juce::var(currentConfig.chordPadLatchMode);
  if (propertyId == "drumFxSplitSplitRow")
    return juce::var(currentConfig.drumFxSplitSplitRow);
  if (propertyId == "fxCcStart")
    return juce::var(currentConfig.fxCcStart);
  if (propertyId == "fxOutputMin")
    return juce::var(currentConfig.fxOutputMin);
  if (propertyId == "fxOutputMax")
    return juce::var(currentConfig.fxOutputMax);
  if (propertyId == "fxToggleMode")
    return juce::var(currentConfig.fxToggleMode);
  if (propertyId == "regionLeft")
    return juce::var(static_cast<double>(currentConfig.region.left));
  if (propertyId == "regionTop")
    return juce::var(static_cast<double>(currentConfig.region.top));
  if (propertyId == "regionRight")
    return juce::var(static_cast<double>(currentConfig.region.right));
  if (propertyId == "regionBottom")
    return juce::var(static_cast<double>(currentConfig.region.bottom));
  if (propertyId == "zIndex")
    return juce::var(currentConfig.zIndex);
  if (propertyId == "regionLock")
    return juce::var(currentConfig.regionLock);
  return juce::var();
}

void TouchpadMixerEditorComponent::applyConfigValue(
    const juce::String &propertyId, const juce::var &value) {
  if (selectedIndex < 0 || !manager)
    return;
  if (propertyId == "type") {
    int id = static_cast<int>(value);
    switch (id) {
    case 1:
      currentConfig.type = TouchpadType::Mixer;
      break;
    case 2:
      currentConfig.type = TouchpadType::DrumPad;
      break;
    case 3:
      currentConfig.type = TouchpadType::ChordPad;
      break;
    default:
      currentConfig.type = TouchpadType::Mixer;
      break;
    }
    manager->updateLayout(selectedIndex, currentConfig);
    rebuildUI();
    return;
  } else if (propertyId == "name")
    currentConfig.name = value.toString().toStdString();
  else if (propertyId == "layerId")
    currentConfig.layerId =
        juce::jlimit(0, 8, static_cast<int>(value) - 1); // combo ID 1..9
  else if (propertyId == "layoutGroupId")
    currentConfig.layoutGroupId = juce::jmax(0, static_cast<int>(value));
  else if (propertyId == "numFaders")
    currentConfig.numFaders = juce::jlimit(1, 32, static_cast<int>(value));
  else if (propertyId == "ccStart")
    currentConfig.ccStart = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "midiChannel")
    currentConfig.midiChannel = juce::jlimit(1, 16, static_cast<int>(value));
  else if (propertyId == "inputMin")
    currentConfig.inputMin = static_cast<float>(static_cast<double>(value));
  else if (propertyId == "inputMax")
    currentConfig.inputMax = static_cast<float>(static_cast<double>(value));
  else if (propertyId == "outputMin")
    currentConfig.outputMin = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "outputMax")
    currentConfig.outputMax = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "quickPrecision")
    currentConfig.quickPrecision = (static_cast<int>(value) == 2)
                                       ? TouchpadMixerQuickPrecision::Precision
                                       : TouchpadMixerQuickPrecision::Quick;
  else if (propertyId == "absRel")
    currentConfig.absRel = (static_cast<int>(value) == 2)
                               ? TouchpadMixerAbsRel::Relative
                               : TouchpadMixerAbsRel::Absolute;
  else if (propertyId == "lockFree")
    currentConfig.lockFree = (static_cast<int>(value) == 2)
                                 ? TouchpadMixerLockFree::Free
                                 : TouchpadMixerLockFree::Lock;
  else if (propertyId == "muteButtonsEnabled")
    currentConfig.muteButtonsEnabled = static_cast<bool>(value);
  else if (propertyId == "drumPadRows")
    currentConfig.drumPadRows = juce::jlimit(1, 8, static_cast<int>(value));
  else if (propertyId == "drumPadColumns")
    currentConfig.drumPadColumns = juce::jlimit(1, 16, static_cast<int>(value));
  else if (propertyId == "drumPadMidiNoteStart")
    currentConfig.drumPadMidiNoteStart =
        juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "drumPadBaseVelocity")
    currentConfig.drumPadBaseVelocity =
        juce::jlimit(1, 127, static_cast<int>(value));
  else if (propertyId == "drumPadVelocityRandom")
    currentConfig.drumPadVelocityRandom =
        juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "drumPadLayoutMode") {
    int id = static_cast<int>(value);
    currentConfig.drumPadLayoutMode =
        (id == 2) ? DrumPadLayoutMode::HarmonicGrid
                  : DrumPadLayoutMode::Classic;
  }
  else if (propertyId == "harmonicRowInterval")
    currentConfig.harmonicRowInterval =
        juce::jlimit(-12, 12, static_cast<int>(value));
  else if (propertyId == "harmonicUseScaleFilter")
    currentConfig.harmonicUseScaleFilter = static_cast<bool>(value);
  else if (propertyId == "chordPadPreset")
    currentConfig.chordPadPreset = juce::jmax(0, static_cast<int>(value));
  else if (propertyId == "chordPadLatchMode")
    currentConfig.chordPadLatchMode = static_cast<bool>(value);
  else if (propertyId == "drumFxSplitSplitRow")
    currentConfig.drumFxSplitSplitRow =
        juce::jlimit(0, 8, static_cast<int>(value));
  else if (propertyId == "fxCcStart")
    currentConfig.fxCcStart = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "fxOutputMin")
    currentConfig.fxOutputMin = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "fxOutputMax")
    currentConfig.fxOutputMax = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "fxToggleMode")
    currentConfig.fxToggleMode = static_cast<bool>(value);
  else if (propertyId == "regionLeft")
    currentConfig.region.left =
        static_cast<float>(juce::jlimit(0.0, 1.0, static_cast<double>(value)));
  else if (propertyId == "regionTop")
    currentConfig.region.top =
        static_cast<float>(juce::jlimit(0.0, 1.0, static_cast<double>(value)));
  else if (propertyId == "regionRight")
    currentConfig.region.right =
        static_cast<float>(juce::jlimit(0.0, 1.0, static_cast<double>(value)));
  else if (propertyId == "regionBottom")
    currentConfig.region.bottom =
        static_cast<float>(juce::jlimit(0.0, 1.0, static_cast<double>(value)));
  else if (propertyId == "zIndex")
    currentConfig.zIndex = juce::jlimit(-100, 100, static_cast<int>(value));
  else if (propertyId == "regionLock")
    currentConfig.regionLock = static_cast<bool>(value);
  else
    return;
  manager->updateLayout(selectedIndex, currentConfig);
}

void TouchpadMixerEditorComponent::createControl(
    const InspectorControl &def, std::vector<UiItem> &currentRow) {
  const juce::String propId = def.propertyId;
  juce::var currentVal = getConfigValue(propId);

  auto addItem = [this, &currentRow](std::unique_ptr<juce::Component> comp,
                                     float weight, bool isAuto = false) {
    if (!comp)
      return;
    addAndMakeVisible(*comp);
    currentRow.push_back({std::move(comp), weight, isAuto});
  };

  switch (def.controlType) {
  case InspectorControl::Type::TextEditor: {
    auto te = std::make_unique<juce::TextEditor>();
    te->setMultiLine(false);
    te->setText(currentVal.toString(), false);
    juce::TextEditor *tePtr = te.get();
    te->onFocusLost = [this, tePtr, propId]() {
      applyConfigValue(propId, juce::var(tePtr->getText().toStdString()));
    };
    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->editor = std::move(te);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Slider: {
    auto sl = std::make_unique<juce::Slider>();
    sl->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      sl->setTextValueSuffix(" " + def.suffix);
    sl->setEnabled(def.isEnabled);
    if (!currentVal.isVoid())
      sl->setValue(static_cast<double>(currentVal), juce::dontSendNotification);
    juce::Slider *slPtr = sl.get();
    sl->onValueChange = [this, propId, def, slPtr]() {
      double v = slPtr->getValue();
      juce::var valueToSet = (def.step >= 1.0)
                                 ? juce::var(static_cast<int>(std::round(v)))
                                 : juce::var(v);
      applyConfigValue(propId, valueToSet);
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
  case InspectorControl::Type::ComboBox: {
    auto cb = std::make_unique<juce::ComboBox>();
    if (propId == "layoutGroupId" && manager) {
      // JUCE ComboBox uses id=0 for \"no selection\", so encode groupId+1.
      cb->addItem("- No Group -", 1);
      for (const auto &g : manager->getGroups()) {
        if (g.id > 0)
          cb->addItem(g.name.empty() ? ("Group " + juce::String(g.id))
                                     : juce::String(g.name),
                      g.id + 1);
      }
    } else {
      for (const auto &p : def.options)
        cb->addItem(p.second, p.first);
    }
    if (propId == "type")
      ; // Both Mixer and Drum Pad are enabled
    int id = static_cast<int>(currentVal);
    if (propId == "layoutGroupId") {
      cb->setSelectedId(id + 1, juce::dontSendNotification);
    } else if (id > 0) {
      cb->setSelectedId(id, juce::dontSendNotification);
    }
    if (propId == "quickPrecision")
      cb->setTooltip(
          "Quick: one finger directly controls a fader. Precision: one finger "
          "shows overlay and position, second finger applies the value.");
    else if (propId == "absRel")
      cb->setTooltip(
          "Absolute: finger position on the touchpad sets the value. Relative: "
          "finger movement changes the value; you can start anywhere.");
    else if (propId == "lockFree")
      cb->setTooltip(
          "Lock: the first fader you touch stays selected until you release. "
          "Free: you can swipe to another fader while holding.");
    else if (propId == "layerId")
      cb->setTooltip("Layer this strip belongs to. Only active when this layer "
                     "is selected.");
    juce::ComboBox *cbPtr = cb.get();
    cb->onChange = [this, propId, cbPtr]() {
      int sel = cbPtr->getSelectedId();
      if (propId == "layoutGroupId")
        applyConfigValue(propId, juce::var(sel - 1)); // map back to groupId
      else
        applyConfigValue(propId, juce::var(sel));
    };
    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->editor = std::move(cb);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Toggle: {
    auto tb = std::make_unique<juce::ToggleButton>();
    tb->setToggleState(!currentVal.isVoid() && static_cast<bool>(currentVal),
                       juce::dontSendNotification);
    juce::ToggleButton *tbPtr = tb.get();
    tb->onClick = [this, propId, tbPtr]() {
      applyConfigValue(propId, juce::var(tbPtr->getToggleState()));
    };
    auto lbl = std::make_unique<juce::Label>("", def.label + ":");
    lbl->setJustificationType(juce::Justification::centredLeft);
    auto container =
        std::make_unique<LabeledControl>(std::move(lbl), std::move(tb));
    addItem(std::move(container), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Button: {
    auto btn = std::make_unique<juce::TextButton>(def.label);
    juce::TextButton *btnPtr = btn.get();
    btnPtr->onClick = [this, propId]() {
      if (propId == "relayoutRegion" && manager && selectedIndex >= 0) {
        // Create a temporary dialog for this invocation; the callback applies
        // the chosen region and rebuilds the UI.
        auto *dialog = new TouchpadRelayoutDialog(
            [this](float left, float top, float right, float bottom) {
              if (selectedIndex < 0 || !manager)
                return;
              currentConfig.region.left = left;
              currentConfig.region.top = top;
              currentConfig.region.right = right;
              currentConfig.region.bottom = bottom;
              manager->updateLayout(selectedIndex, currentConfig);
              rebuildUI();
            });

        juce::DialogWindow::LaunchOptions opts;
        opts.content.setOwned(dialog);
        opts.dialogTitle = "Re-layout region";
        opts.dialogBackgroundColour = juce::Colour(0xff222222);
        opts.escapeKeyTriggersCloseButton = true;
        opts.useNativeTitleBar = true;
        opts.resizable = false;
       #if JUCE_MODAL_LOOPS_PERMITTED
        opts.runModal();
       #else
        opts.launchAsync();
       #endif
      }
    };

    auto rowComp = std::make_unique<LabelEditorRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText("", juce::dontSendNotification);
    rowComp->editor = std::move(btn);
    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->editor);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Separator:
  case InspectorControl::Type::LabelOnly:
  case InspectorControl::Type::Color:
    break;
  }
}

void TouchpadMixerEditorComponent::rebuildUI() {
  for (auto &row : uiRows) {
    for (auto &item : row.items) {
      if (item.component)
        removeChildComponent(item.component.get());
    }
  }
  uiRows.clear();

  if (selectedIndex < 0) {
    resized();
    return;
  }

  InspectorSchema schema =
      TouchpadMixerDefinition::getSchema(currentConfig.type);
  for (const auto &def : schema) {
    if (def.controlType == InspectorControl::Type::Separator) {
      UiRow row;
      row.isSeparatorRow = true;
      auto sepComp =
          std::make_unique<SeparatorComponent>(def.label, def.separatorAlign);
      addAndMakeVisible(*sepComp);
      row.items.push_back({std::move(sepComp), 1.0f, false});
      uiRows.push_back(std::move(row));
      continue;
    }
    if (!def.sameLine || uiRows.empty() || uiRows.back().isSeparatorRow) {
      UiRow row;
      row.isSeparatorRow = false;
      uiRows.push_back(std::move(row));
    }
    createControl(def, uiRows.back().items);
  }
  resized();
}

void TouchpadMixerEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
  if (selectedIndex < 0) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("Select a strip from the list.", getLocalBounds(),
               juce::Justification::centred);
  }
}

int TouchpadMixerEditorComponent::getPreferredContentHeight() const {
  const int rowHeight = 25;
  const int separatorRowHeight = 15;
  const int spacing = 4;
  const int topPadding = 4;
  const int boundsReduction = 8;
  int y = boundsReduction + topPadding;
  for (const auto &row : uiRows) {
    if (row.items.empty())
      continue;
    if (row.isSeparatorRow)
      y += 12;
    const int h = row.isSeparatorRow ? separatorRowHeight : rowHeight;
    y += h + spacing;
  }
  return y + boundsReduction;
}

void TouchpadMixerEditorComponent::resized() {
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
        if (auto *lc = dynamic_cast<LabeledControl *>(item.component.get()))
          usedWidth += lc->getIdealWidth();
        else
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
        if (auto *lc = dynamic_cast<LabeledControl *>(item.component.get()))
          w = lc->getIdealWidth();
        else
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
}
