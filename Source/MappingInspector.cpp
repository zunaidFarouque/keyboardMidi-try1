#include "MappingInspector.h"
#include "DeviceManager.h"
#include "MidiNoteUtilities.h"

namespace {
// Phase 55.6: Invisible container for Label + Editor as one layout unit
struct LabelEditorRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
  static constexpr int labelWidth = 80;
  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelWidth));
    if (editor)
      editor->setBounds(r);
  }
};

// Phase 55.8: Label on left, editor on right; label width from text
class LabeledControl : public juce::Component {
public:
  LabeledControl(std::unique_ptr<juce::Label> l,
                 std::unique_ptr<juce::Component> c)
      : label(std::move(l)), editor(std::move(c)) {
    if (label)
      addAndMakeVisible(*label);
    addAndMakeVisible(*editor);
  }
  void resized() override {
    auto area = getLocalBounds();
    if (label) {
      int textWidth =
          label->getFont().getStringWidth(label->getText()) + 10;
      label->setBounds(area.removeFromLeft(textWidth));
    }
    editor->setBounds(area);
  }
  // Phase 55.10: For auto-width layout - fit to content
  int getIdealWidth() const {
    int w = 0;
    if (label)
      w += label->getFont().getStringWidth(label->getText()) + 10;
    w += 30; // ToggleButton / editor minimum
    return w;
  }

private:
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
};
} // namespace

MappingInspector::MappingInspector(juce::UndoManager &undoMgr,
                                   DeviceManager &deviceMgr)
    : undoManager(undoMgr), deviceManager(deviceMgr) {
  deviceManager.addChangeListener(this);
}

MappingInspector::~MappingInspector() {
  deviceManager.removeChangeListener(this);
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.removeListener(this);
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

void MappingInspector::setSelection(
    const std::vector<juce::ValueTree> &selection) {
  isUpdatingFromTree = true;
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.removeListener(this);
  }
  selectedTrees = selection;
  for (auto &tree : selectedTrees) {
    if (tree.isValid())
      tree.addListener(this);
  }
  rebuildUI();
  isUpdatingFromTree = false;
}

void MappingInspector::rebuildUI() {
  for (auto &row : uiRows) {
    for (auto &item : row.items) {
      if (item.component)
        removeChildComponent(item.component.get());
    }
  }
  uiRows.clear();

  if (selectedTrees.empty())
    return;

  InspectorSchema schema =
      MappingDefinition::getSchema(selectedTrees[0]);
  for (const auto &def : schema) {
    // Phase 55.9: Handle explicit Separator items
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
    if (!def.sameLine || uiRows.empty()) {
      UiRow row;
      row.isSeparatorRow = false;
      uiRows.push_back(std::move(row));
    }
    createControl(def, uiRows.back());
  }
  createAliasRow();

  resized();
}

MappingInspector::SeparatorComponent::SeparatorComponent(
    const juce::String &label, juce::Justification justification)
    : labelText(label), textAlign(justification) {}

void MappingInspector::SeparatorComponent::paint(juce::Graphics &g) {
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

  if (textLeft - pad > bounds.getX()) {
    g.setColour(juce::Colours::grey);
    g.fillRect(bounds.getX(), lineY, textLeft - pad - bounds.getX(),
               lineHeight);
  }
  if (textRight + pad < bounds.getRight()) {
    g.setColour(juce::Colours::grey);
    g.fillRect(textRight + pad, lineY,
               bounds.getRight() - (textRight + pad), lineHeight);
  }
}

void MappingInspector::createControl(const InspectorControl &def,
                                     UiRow &currentRow) {
  const juce::Identifier propId(def.propertyId.toStdString());
  const bool sameVal = allTreesHaveSameValue(propId);
  juce::var currentVal = getCommonValue(propId);

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
    sl->setEnabled(def.isEnabled);

    // Phase 55.7: Handle enabled condition property
    if (def.enabledConditionProperty.isNotEmpty()) {
      bool isEnabled = selectedTrees[0].getProperty(
          juce::Identifier(def.enabledConditionProperty.toStdString()), false);
      sl->setEnabled(isEnabled);
    }

    if (def.valueFormat == InspectorControl::Format::NoteName) {
      sl->textFromValueFunction = [](double val) {
        return MidiNoteUtilities::getMidiNoteName(static_cast<int>(val));
      };
      sl->valueFromTextFunction = [](const juce::String &text) {
        return static_cast<double>(
            MidiNoteUtilities::getMidiNoteFromText(text));
      };
    }

    if (sameVal && !currentVal.isVoid())
      sl->setValue(static_cast<double>(currentVal), juce::dontSendNotification);
    else if (!sameVal) {
      sl->setValue((def.min + def.max) * 0.5, juce::dontSendNotification);
      sl->setTextValueSuffix(" (---)");
    }

    juce::Slider *slPtr = sl.get();
    sl->onValueChange = [this, propId, def, slPtr]() {
      if (selectedTrees.empty())
        return;
      if (slPtr->getTextValueSuffix().contains("---"))
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      double v = slPtr->getValue();
      juce::var valueToSet =
          (def.step >= 1.0) ? juce::var(static_cast<int>(std::round(v)))
                            : juce::var(v);
      for (auto &tree : selectedTrees) {
        if (tree.isValid())
          tree.setProperty(propId, valueToSet, &undoManager);
      }
    };

    // Phase 55.7: Handle empty labels (no container needed)
    if (def.label.isEmpty()) {
      addItem(std::move(sl), def.widthWeight, def.autoWidth);
    } else {
      auto rowComp = std::make_unique<LabelEditorRow>();
      rowComp->label = std::make_unique<juce::Label>();
      rowComp->label->setText(def.label + ":", juce::dontSendNotification);
      rowComp->editor = std::move(sl);
      rowComp->addAndMakeVisible(*rowComp->label);
      rowComp->addAndMakeVisible(*rowComp->editor);
      addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    }
    break;
  }
  case InspectorControl::Type::ComboBox: {
    auto cb = std::make_unique<juce::ComboBox>();
    for (const auto &p : def.options)
      cb->addItem(p.second, p.first);

    if (def.propertyId == "type") {
      juce::String typeStr = currentVal.toString();
      for (const auto &p : def.options) {
        if (p.second == typeStr) {
          cb->setSelectedId(p.first, juce::dontSendNotification);
          break;
        }
      }
    } else if (def.propertyId == "adsrTarget") {
      juce::String targetStr = currentVal.toString();
      if (targetStr.isEmpty())
        targetStr = "CC";
      for (const auto &p : def.options) {
        if (p.second == targetStr) {
          cb->setSelectedId(p.first, juce::dontSendNotification);
          break;
        }
      }
    } else if (sameVal && !currentVal.isVoid()) {
      int id = static_cast<int>(currentVal);
      cb->setSelectedId(id, juce::dontSendNotification);
    }

    juce::ComboBox *cbPtr = cb.get();
    cb->onChange = [this, propId, def, cbPtr]() {
      if (selectedTrees.empty())
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      juce::var valueToSet;
      if (def.propertyId == "type" || def.propertyId == "adsrTarget") {
        int id = cbPtr->getSelectedId();
        auto it = def.options.find(id);
        valueToSet = (it != def.options.end()) ? juce::var(it->second)
                                                : juce::var();
      } else {
        valueToSet = cbPtr->getSelectedId();
      }
      for (auto &tree : selectedTrees) {
        if (tree.isValid())
          tree.setProperty(propId, valueToSet, &undoManager);
      }
      if (def.propertyId == "type" || def.propertyId == "data1")
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
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
    // Phase 55.8: No button text; label is separate on the left
    if (sameVal && !currentVal.isVoid())
      tb->setToggleState(static_cast<bool>(currentVal),
                         juce::dontSendNotification);
    else
      tb->setToggleState(false, juce::dontSendNotification);

    tb->onClick = [this, propId, def, tb = tb.get()]() {
      if (selectedTrees.empty())
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      bool v = tb->getToggleState();
      for (auto &tree : selectedTrees) {
        if (tree.isValid())
          tree.setProperty(propId, v, &undoManager);
      }
    };

    auto lbl = std::make_unique<juce::Label>("", def.label + ":");
    lbl->setJustificationType(juce::Justification::centredLeft);
    auto container = std::make_unique<LabeledControl>(std::move(lbl),
                                                      std::move(tb));
    addItem(std::move(container), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::LabelOnly: {
    auto label = std::make_unique<juce::Label>();
    label->setText(def.label, juce::dontSendNotification);
    addItem(std::move(label), def.widthWeight, def.autoWidth);
    break;
  }
  case InspectorControl::Type::Separator:
    // Phase 55.9: Separators handled in rebuildUI, not here
    break;
  default:
    break;
  }
}

void MappingInspector::createAliasRow() {
  UiRow row;
  row.isSeparatorRow = false;

  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Alias:", juce::dontSendNotification);

  auto aliasCombo = std::make_unique<juce::ComboBox>();
  aliasCombo->addItem("Global (All Devices)", 1);
  juce::StringArray aliases = deviceManager.getAllAliasNames();
  for (int i = 0; i < aliases.size(); ++i)
    aliasCombo->addItem(aliases[i], i + 2);

  juce::String aliasName = "Global (All Devices)";
  if (allTreesHaveSameValue("deviceHash")) {
    juce::String hashStr = getCommonValue("deviceHash").toString();
    if (!hashStr.isEmpty()) {
      uintptr_t hash = static_cast<uintptr_t>(hashStr.getHexValue64());
      if (hash != 0) {
        aliasName = deviceManager.getAliasName(hash);
        if (aliasName == "Unknown")
          aliasName = "Global (All Devices)";
      }
    }
  } else {
    aliasCombo->setSelectedId(-1, juce::dontSendNotification);
  }

  if (!aliasName.isEmpty() && aliasName != "Unknown") {
    for (int i = 0; i < aliasCombo->getNumItems(); ++i) {
      if (aliasCombo->getItemText(i) == aliasName) {
        aliasCombo->setSelectedItemIndex(i, juce::dontSendNotification);
        break;
      }
    }
  }

  juce::ComboBox *aliasComboPtr = aliasCombo.get();
  aliasCombo->onChange = [this, aliasComboPtr]() {
    if (selectedTrees.empty() || aliasComboPtr->getSelectedId() == -1)
      return;
    juce::String aliasName = aliasComboPtr->getText();
    uintptr_t newHash = 0;
    if (aliasName != "Global (All Devices)" && aliasName != "Any / Master" &&
        aliasName != "Unassigned" && !aliasName.isEmpty()) {
      newHash =
          static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
    }
    undoManager.beginNewTransaction("Change Alias");
    for (auto &tree : selectedTrees) {
      if (!tree.isValid())
        continue;
      tree.setProperty(
          "deviceHash",
          juce::String::toHexString((juce::int64)newHash).toUpperCase(),
          &undoManager);
      if (newHash == 0)
        tree.setProperty("inputAlias", "", &undoManager);
      else
        tree.setProperty("inputAlias", aliasName, &undoManager);
    }
  };

  rowComp->editor = std::move(aliasCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void MappingInspector::resized() {
  const int rowHeight = 25;
  const int separatorRowHeight = 15; // Phase 55.9: smaller for separator rows
  const int spacing = 4;  // Phase 55.10: tighter layout
  const int topPadding = 4;
  auto bounds = getLocalBounds().reduced(4);
  int y = bounds.getY() + topPadding;

  for (auto &row : uiRows) {
    if (row.items.empty())
      continue;

    // Phase 55.11: Extra top margin before separator rows for readability
    if (row.isSeparatorRow)
      y += 12;

    const int h = row.isSeparatorRow ? separatorRowHeight : rowHeight;
    const int totalAvailable = bounds.getWidth();
    int usedWidth = 0;
    float totalWeight = 0.0f;

    // Phase 55.10: Measure auto-width items
    for (auto &item : row.items) {
      if (item.isAutoWidth) {
        if (auto *lc = dynamic_cast<LabeledControl *>(item.component.get())) {
          usedWidth += lc->getIdealWidth();
        } else {
          usedWidth += 100;
        }
      } else {
        totalWeight += item.weight;
      }
    }

    int remainingWidth = std::max(0, totalAvailable - usedWidth);
    int x = bounds.getX();

    for (auto &item : row.items) {
      int w = 0;
      if (item.isAutoWidth) {
        if (auto *lc = dynamic_cast<LabeledControl *>(item.component.get())) {
          w = lc->getIdealWidth();
        } else {
          w = 100;
        }
      } else {
        if (totalWeight > 0.0f) {
          w = static_cast<int>((item.weight / totalWeight) * remainingWidth);
        } else {
          w = remainingWidth;
        }
      }

      if (item.component)
        item.component->setBounds(x, y, w, h);
      x += w;
    }

    y += h + spacing;
  }
  setSize(getWidth(), y + 4);
}

int MappingInspector::getRequiredHeight() const {
  const int controlHeight = 25;
  const int separatorHeight = 15; // Phase 55.9
  const int separatorTopMargin = 12; // Phase 55.11: extra space before separator
  const int spacing = 4;  // Phase 55.10: matches resized()
  const int topPadding = 4;
  const int bottomPadding = 4;
  int total = topPadding + bottomPadding;
  for (const auto &row : uiRows) {
    if (row.isSeparatorRow)
      total += separatorTopMargin;
    total += (row.isSeparatorRow ? separatorHeight : controlHeight) + spacing;
  }
  return total;
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

void MappingInspector::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &deviceManager)
    rebuildUI();
}

void MappingInspector::valueTreePropertyChanged(
    juce::ValueTree &tree, const juce::Identifier &property) {
  if (isUpdatingFromTree)
    return;
  bool isRelevant = false;
  for (const auto &t : selectedTrees) {
    if (t == tree) {
      isRelevant = true;
      break;
    }
  }
  if (!isRelevant)
    return;
  // Phase 55.6: Rebuild only when schema structure changes (avoids rebuild during slider drag)
  bool needsRebuild = false;
  if (property == juce::Identifier("type") ||
      property == juce::Identifier("adsrTarget") ||
      property == juce::Identifier("sendReleaseValue") ||
      property == juce::Identifier("useCustomEnvelope")) {
    needsRebuild = true;
  } else if (property == juce::Identifier("data1")) {
    if (allTreesHaveSameValue("type") &&
        getCommonValue("type").toString() == "Command")
      needsRebuild = true;
  }
  if (needsRebuild) {
    juce::MessageManager::callAsync([this]() {
      if (selectedTrees.empty())
        return;
      rebuildUI();
    });
  } else {
    repaint();
  }
}
