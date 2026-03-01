#include "MappingInspector.h"
#include "MappingInspectorLogic.h"
#include "DeviceManager.h"
#include "KeyNameUtilities.h"
#include "MappingTypes.h"
#include "MidiNoteUtilities.h"
#include "PresetManager.h"
#include "SettingsManager.h"
#include "MappingDefinition.h"
#include "TouchpadMixerManager.h"

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
      int textWidth = label->getFont().getStringWidth(label->getText()) + 10;
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
                                   DeviceManager &deviceMgr,
                                   SettingsManager &settingsMgr,
                                   PresetManager *presetMgr,
                                   TouchpadMixerManager *touchpadMixerMgr)
    : undoManager(undoMgr), deviceManager(deviceMgr),
      settingsManager(settingsMgr), presetManager(presetMgr),
      touchpadMixerManager(touchpadMixerMgr) {
  deviceManager.addChangeListener(this);
  settingsManager.addChangeListener(this);
  if (touchpadMixerManager)
    touchpadMixerManager->addChangeListener(this);
}

MappingInspector::~MappingInspector() {
  deviceManager.removeChangeListener(this);
  settingsManager.removeChangeListener(this);
  if (touchpadMixerManager)
    touchpadMixerManager->removeChangeListener(this);
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
    g.drawText("No selection", getLocalBounds(), juce::Justification::centred,
               true);
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

  createAliasRow();
  createKeyboardGroupRow();
  createKeyRow();

  // Determine whether to show Base-layer-only \"Apply on all layers\" toggle.
  bool showForceAllLayers = false;
  if (presetManager && selectedTrees[0].isValid() &&
      selectedTrees[0].hasType("Mapping")) {
    int layerId = -1;
    juce::ValueTree node = selectedTrees[0];
    while (node.isValid()) {
      if (node.hasType("Layer")) {
        layerId = static_cast<int>(node.getProperty("id", -1));
        break;
      }
      node = node.getParent();
    }
    if (layerId < 0 && selectedTrees[0].hasProperty("layerID"))
      layerId =
          static_cast<int>(selectedTrees[0].getProperty("layerID", -1));
    showForceAllLayers = (layerId == 0);
  }

  int pbRange = settingsManager.getPitchBendRange();
  InspectorSchema schema =
      MappingDefinition::getSchema(selectedTrees[0], pbRange);

  if (showForceAllLayers) {
    schema.push_back(MappingDefinition::createSeparator(
        "Global behavior", juce::Justification::centredLeft));
    InspectorControl force;
    force.propertyId = "forceAllLayers";
    force.label = "Apply on all layers";
    force.controlType = InspectorControl::Type::Toggle;
    force.widthWeight = 1.0f;
    schema.push_back(force);
  }
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
    g.fillRect(textRight + pad, lineY, bounds.getRight() - (textRight + pad),
               lineHeight);
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
    // If this slider is conditionally enabled by another boolean property and
    // that condition is currently false, hide the control entirely to avoid
    // visual clutter (e.g. Slide rest value/glide when return-to-rest is off).
    if (def.enabledConditionProperty.isNotEmpty()) {
      if (selectedTrees.empty())
        return;
      const bool isEnabledCond = selectedTrees[0].getProperty(
          juce::Identifier(def.enabledConditionProperty.toStdString()), false);
      if (!isEnabledCond)
        return;
    }

    auto sl = std::make_unique<juce::Slider>();
    sl->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      sl->setTextValueSuffix(" " + def.suffix);
    sl->setEnabled(def.isEnabled);

    if (def.valueFormat == InspectorControl::Format::NoteName) {
      sl->textFromValueFunction = [](double val) {
        return MidiNoteUtilities::getMidiNoteName(static_cast<int>(val));
      };
      sl->valueFromTextFunction = [](const juce::String &text) {
        return static_cast<double>(
            MidiNoteUtilities::getMidiNoteFromText(text));
      };
    }

    // PitchBend peak: valueScaleRange > 0 -> display semitones, store raw
    // 0-16383
    const int scaleRange = def.valueScaleRange;
    if (scaleRange > 0) {
      if (sameVal && !currentVal.isVoid()) {
        int raw = static_cast<int>(currentVal);
        double semitones = (raw - 8192) * scaleRange / 8192.0;
        sl->setValue(juce::jlimit(def.min, def.max, semitones),
                     juce::dontSendNotification);
      } else {
        sl->setValue(0.0, juce::dontSendNotification);
        if (!sameVal)
          sl->setTextValueSuffix(" (---)");
      }
    } else if (sameVal && !currentVal.isVoid()) {
      sl->setValue(static_cast<double>(currentVal), juce::dontSendNotification);
    } else if (sameVal && currentVal.isVoid() && !def.defaultValue.isVoid()) {
      sl->setValue(static_cast<double>(def.defaultValue), juce::dontSendNotification);
    } else if (!sameVal) {
      sl->setValue((def.min + def.max) * 0.5, juce::dontSendNotification);
      sl->setTextValueSuffix(" (---)");
    }

    juce::Slider *slPtr = sl.get();
    sl->onValueChange = [this, propId, def, slPtr, scaleRange]() {
      if (selectedTrees.empty())
        return;
      if (slPtr->getTextValueSuffix().contains("---"))
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      double v = slPtr->getValue();
      juce::var valueToSet;
      if (scaleRange > 0) {
        int raw = static_cast<int>(8192 + (v / scaleRange) * 8192);
        raw = juce::jlimit(0, 16383, raw);
        valueToSet = juce::var(raw);
      } else {
        valueToSet = (def.step >= 1.0)
                         ? juce::var(static_cast<int>(std::round(v)))
                         : juce::var(v);
      }
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
    // Populate touchpadLayoutGroupId dynamically from TouchpadMixerManager
    if (def.propertyId == "touchpadLayoutGroupId" && touchpadMixerManager) {
      // JUCE ComboBox uses id=0 for "no selection", so encode groupId+1.
      cb->addItem("- No Group -", 1);
      auto groups = touchpadMixerManager->getLayoutGroups();
      for (const auto &group : groups) {
        cb->addItem(group.second, group.first + 1);
      }
    } else if ((def.propertyId == "keyboardLayoutGroupId" ||
                def.propertyId == "keyboardGroupId") && presetManager) {
      cb->addItem("None", 1);
      auto names = presetManager->getKeyboardGroupNames();
      for (const auto &p : names) {
        cb->addItem(p.second, p.first + 1);
      }
    } else {
      for (const auto &p : def.options)
        cb->addItem(p.second, p.first);
    }

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
    } else if (def.propertyId == "releaseBehavior") {
      juce::String rbStr = currentVal.toString();
      if (rbStr.isEmpty())
        rbStr = "Send Note Off";
      for (const auto &p : def.options) {
        if (p.second == rbStr) {
          cb->setSelectedId(p.first, juce::dontSendNotification);
          break;
        }
      }
    } else if (def.propertyId == "pitchPadMode") {
      juce::String modeStr = currentVal.toString();
      if (modeStr.isEmpty())
        modeStr = "Absolute";
      for (const auto &p : def.options) {
        if (p.second.equalsIgnoreCase(modeStr)) {
          cb->setSelectedId(p.first, juce::dontSendNotification);
          break;
        }
      }
    } else if (def.propertyId == "sustainStyle") {
      // Virtual: data1 0,1,2 -> combo ids 1,2,3
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = (data1 >= 0 && data1 <= 2) ? (data1 + 1) : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "panicMode") {
      // Virtual: data1==5 or data2==1 -> Panic latched only (2), data2==2 ->
      // Panic chords (3), else Panic all (1)
      int data1 = static_cast<int>(getCommonValue("data1"));
      int data2 = static_cast<int>(getCommonValue("data2"));
      int id = (data2 == 2) ? 3 : (data1 == 5 || data2 == 1) ? 2 : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "transposeMode") {
      juce::String modeStr = currentVal.toString();
      if (modeStr.isEmpty())
        modeStr = "Global";
      int id = modeStr.equalsIgnoreCase("Local") ? 2 : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "transposeModify") {
      int modify = static_cast<int>(currentVal);
      int id = (modify >= 0 && modify <= 4) ? (modify + 1) : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "layerStyle") {
      // Virtual: data1 10,11 -> combo ids 1,2 (Hold to switch, Toggle layer)
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = (data1 == 10) ? 1 : (data1 == 11) ? 2 : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "commandCategory") {
      // Virtual: map underlying CommandID in data1 to high-level categories.
      int data1 = static_cast<int>(getCommonValue("data1"));
      const int sustainMin = 0, sustainMax = 2;
      if (data1 >= sustainMin && data1 <= sustainMax) {
        cb->setSelectedId(100, juce::dontSendNotification); // Sustain
      } else if (data1 ==
                 static_cast<int>(MIDIQy::CommandID::LatchToggle)) {
        cb->setSelectedId(101, juce::dontSendNotification); // Latch
      } else if (data1 == static_cast<int>(MIDIQy::CommandID::Panic) ||
                 data1 == static_cast<int>(MIDIQy::CommandID::PanicLatch)) {
        cb->setSelectedId(102, juce::dontSendNotification); // Panic
      } else if (data1 == static_cast<int>(MIDIQy::CommandID::Transpose) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalPitchDown)) {
        cb->setSelectedId(103, juce::dontSendNotification); // Transpose
      } else if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalModeUp) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalModeDown)) {
        cb->setSelectedId(104, juce::dontSendNotification); // Global mode
      } else if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalRootUp) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalRootDown) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalRootSet)) {
        cb->setSelectedId(105, juce::dontSendNotification); // Global Root
      } else if (data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalScaleNext) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalScalePrev) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::GlobalScaleSet)) {
        cb->setSelectedId(106, juce::dontSendNotification); // Global Scale
      } else if (data1 ==
                     static_cast<int>(MIDIQy::CommandID::LayerMomentary) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::LayerToggle)) {
        cb->setSelectedId(110, juce::dontSendNotification); // Layer
      } else if (data1 ==
                     static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloMomentary) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloToggle) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloSet) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloClear)) {
        cb->setSelectedId(111, juce::dontSendNotification); // Keyboard group solo
      } else if (data1 ==
                     static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloSet) ||
                 data1 ==
                     static_cast<int>(MIDIQy::CommandID::TouchpadLayoutGroupSoloClear)) {
        cb->setSelectedId(112, juce::dontSendNotification); // Touchpad group solo
      }
    } else if (def.propertyId == "globalModeDirection") {
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = (data1 == static_cast<int>(MIDIQy::CommandID::GlobalModeDown))
                   ? 2
                   : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "globalRootMode") {
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = 1;
      if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalRootDown))
        id = 2;
      else if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalRootSet))
        id = 3;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "globalScaleMode") {
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = 1;
      if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalScalePrev))
        id = 2;
      else if (data1 == static_cast<int>(MIDIQy::CommandID::GlobalScaleSet))
        id = 3;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "touchpadSoloScope") {
      int v = static_cast<int>(getCommonValue("touchpadSoloScope"));
      int id = (v == 1) ? 2 : (v == 2) ? 3 : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "touchpadSoloType") {
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = 1;
      if (data1 ==
          static_cast<int>(
              MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle))
        id = 2;
      else if (data1 ==
               static_cast<int>(
                   MIDIQy::CommandID::TouchpadLayoutGroupSoloSet))
        id = 3;
      else if (data1 ==
               static_cast<int>(
                   MIDIQy::CommandID::TouchpadLayoutGroupSoloClear))
        id = 4;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "touchpadLayoutGroupId") {
      int groupId = static_cast<int>(getCommonValue("touchpadLayoutGroupId"));
      // Stored groupId (0 = none) -> combo id (1 = "- No Group -")
      cb->setSelectedId(groupId + 1, juce::dontSendNotification);
    } else if (def.propertyId == "keyboardSoloType") {
      int data1 = static_cast<int>(getCommonValue("data1"));
      int id = 1;
      if (data1 == static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloToggle))
        id = 2;
      else if (data1 == static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloSet))
        id = 3;
      else if (data1 == static_cast<int>(MIDIQy::CommandID::KeyboardLayoutGroupSoloClear))
        id = 4;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "keyboardLayoutGroupId") {
      int groupId = static_cast<int>(getCommonValue("keyboardLayoutGroupId"));
      cb->setSelectedId(groupId + 1, juce::dontSendNotification);
    } else if (def.propertyId == "keyboardGroupId") {
      int groupId = static_cast<int>(getCommonValue("keyboardGroupId"));
      cb->setSelectedId(groupId + 1, juce::dontSendNotification);
    } else if (def.propertyId == "keyboardSoloScope") {
      int v = static_cast<int>(getCommonValue("keyboardSoloScope"));
      int id = (v == 1) ? 2 : (v == 2) ? 3 : 1;
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (def.propertyId == "data1" && !def.options.count(5)) {
      // Panic Latch (5) -> 4; GlobalPitchDown (7) -> 6
      int data1 = static_cast<int>(currentVal);
      int display = data1;
      if (data1 == 5)
        display = 4;
      else if (data1 == 7)
        display = 6;
      cb->setSelectedId(display, juce::dontSendNotification);
    } else if (sameVal && !currentVal.isVoid()) {
      int id = static_cast<int>(currentVal);
      cb->setSelectedId(id, juce::dontSendNotification);
    }

    juce::ComboBox *cbPtr = cb.get();
    cb->onChange = [this, def, cbPtr]() {
      if (selectedTrees.empty())
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      int id = cbPtr->getSelectedId();
      for (auto &tree : selectedTrees) {
        if (tree.isValid())
          MappingInspectorLogic::applyComboSelectionToMapping(tree, def, id,
                                                              &undoManager);
      }
      if (def.propertyId == "type" || def.propertyId == "data1" ||
          def.propertyId == "commandCategory" ||
          def.propertyId == "sustainStyle" ||
          def.propertyId == "panicMode" ||
          def.propertyId == "layerStyle" ||
          def.propertyId == "transposeMode" ||
          def.propertyId == "transposeModify" ||
          def.propertyId == "globalModeDirection" ||
          def.propertyId == "globalRootMode" ||
          def.propertyId == "globalScaleMode" ||
          def.propertyId == "touchpadSoloType" ||
          def.propertyId == "touchpadLayoutGroupId" ||
          def.propertyId == "touchpadSoloScope" ||
          def.propertyId == "keyboardSoloType" ||
          def.propertyId == "keyboardLayoutGroupId" ||
          def.propertyId == "keyboardSoloScope" ||
          def.propertyId == "keyboardGroupId") {
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
      }
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
    bool defaultForToggle = (def.propertyId == "releaseLatchedOnToggleOff");
    // Reset pitch on release: default on when target is PitchBend or
    // SmartScaleBend
    if (def.propertyId == "sendReleaseValue" && selectedTrees.size() == 1 &&
        selectedTrees[0].isValid()) {
      juce::String targetStr =
          selectedTrees[0].getProperty("adsrTarget", "CC").toString();
      bool isPitchOrSmart = targetStr.equalsIgnoreCase("PitchBend") ||
                            targetStr.equalsIgnoreCase("SmartScaleBend");
      if (isPitchOrSmart && currentVal.isVoid()) {
        defaultForToggle = true;
        selectedTrees[0].setProperty("sendReleaseValue", true, &undoManager);
      }
    }
    if (sameVal && !currentVal.isVoid())
      tb->setToggleState(static_cast<bool>(currentVal),
                         juce::dontSendNotification);
    else
      tb->setToggleState(defaultForToggle, juce::dontSendNotification);

    tb->onClick = [this, propId, def, tb = tb.get()]() {
      if (selectedTrees.empty())
        return;
      undoManager.beginNewTransaction("Change " + def.label);
      bool v = tb->getToggleState();
      for (auto &tree : selectedTrees) {
        if (tree.isValid())
          tree.setProperty(propId, v, &undoManager);
      }
      // When this toggle controls other controls' enabledConditionProperty
      // (e.g. Slide "Return to rest on finger release"), rebuild the UI so
      // dependent sliders can appear/disappear immediately.
      if (def.propertyId == "slideReturnOnRelease") {
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
      }
    };

    auto lbl = std::make_unique<juce::Label>("", def.label + ":");
    lbl->setJustificationType(juce::Justification::centredLeft);
    auto container =
        std::make_unique<LabeledControl>(std::move(lbl), std::move(tb));
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
  rowComp->label->setText("Device:", juce::dontSendNotification);

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
      newHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
    }
    undoManager.beginNewTransaction("Change Device");
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

void MappingInspector::createKeyboardGroupRow() {
  if (!presetManager || selectedTrees.empty() || !selectedTrees[0].isValid() ||
      !selectedTrees[0].hasType("Mapping"))
    return;
  juce::ValueTree mapping = selectedTrees[0];
  juce::String typeStr = mapping.getProperty("type", "Note").toString().trim();
  if (typeStr.equalsIgnoreCase("Command")) {
    int data1 = (int)mapping.getProperty("data1", 0);
    bool isGroupSolo =
        (data1 == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloMomentary ||
         data1 == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloToggle ||
         data1 == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloSet ||
         data1 == (int)MIDIQy::CommandID::KeyboardLayoutGroupSoloClear) ||
        (data1 == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloMomentary ||
         data1 == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloToggle ||
         data1 == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloSet ||
         data1 == (int)MIDIQy::CommandID::TouchpadLayoutGroupSoloClear);
    if (isGroupSolo)
      return;
  }

  UiRow row;
  row.isSeparatorRow = false;
  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Keyboard group:", juce::dontSendNotification);

  auto groupCombo = std::make_unique<juce::ComboBox>();
  groupCombo->addItem("None", 1);
  auto names = presetManager->getKeyboardGroupNames();
  for (const auto &p : names)
    groupCombo->addItem(p.second, p.first + 1);

  const juce::Identifier propId("keyboardGroupId");
  int groupId = 0;
  if (allTreesHaveSameValue(propId))
    groupId = static_cast<int>(getCommonValue(propId));
  groupCombo->setSelectedId(groupId + 1, juce::dontSendNotification);
  if (!allTreesHaveSameValue(propId))
    groupCombo->setSelectedId(-1, juce::dontSendNotification);

  juce::ComboBox *groupComboPtr = groupCombo.get();
  groupCombo->onChange = [this, groupComboPtr]() {
    if (selectedTrees.empty())
      return;
    int sel = groupComboPtr->getSelectedId();
    if (sel < 0)
      return;
    int newId = sel - 1;
    undoManager.beginNewTransaction("Change Keyboard group");
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("keyboardGroupId", newId, &undoManager);
    }
  };

  rowComp->editor = std::move(groupCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

// Map keyCode to ComboBox item id (JUCE requires non-zero ids). 0..255
// -> 1..256; specials -> 257..260.
static int keyCodeToItemId(int keyCode) {
  if (keyCode == InputTypes::ScrollUp)
    return 257;
  if (keyCode == InputTypes::ScrollDown)
    return 258;
  if (keyCode == InputTypes::PointerX)
    return 259;
  if (keyCode == InputTypes::PointerY)
    return 260;
  if (keyCode >= 0 && keyCode <= 255)
    return keyCode + 1;
  return 1; // fallback: Key 0
}

static int itemIdToKeyCode(int itemId) {
  if (itemId >= 257 && itemId <= 260) {
    const int codes[] = {InputTypes::ScrollUp, InputTypes::ScrollDown,
                         InputTypes::PointerX, InputTypes::PointerY};
    return codes[itemId - 257];
  }
  if (itemId >= 1 && itemId <= 256)
    return itemId - 1;
  return 0;
}

void MappingInspector::createKeyRow() {
  UiRow row;
  row.isSeparatorRow = false;

  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Key:", juce::dontSendNotification);

  auto keyCombo = std::make_unique<juce::ComboBox>();
  for (int keyCode = 0; keyCode <= 255; ++keyCode)
    keyCombo->addItem(KeyNameUtilities::getKeyName(keyCode), keyCode + 1);
  keyCombo->addItem("Scroll Up", 257);
  keyCombo->addItem("Scroll Down", 258);
  keyCombo->addItem("Trackpad X", 259);
  keyCombo->addItem("Trackpad Y", 260);

  const juce::Identifier inputKeyId("inputKey");
  int currentKey = 0;
  if (allTreesHaveSameValue(inputKeyId))
    currentKey = static_cast<int>(getCommonValue(inputKeyId));
  keyCombo->setSelectedId(keyCodeToItemId(currentKey),
                          juce::dontSendNotification);
  if (!allTreesHaveSameValue(inputKeyId))
    keyCombo->setSelectedId(-1, juce::dontSendNotification);

  juce::ComboBox *keyComboPtr = keyCombo.get();
  keyCombo->onChange = [this, keyComboPtr]() {
    if (selectedTrees.empty() || keyComboPtr->getSelectedId() == -1)
      return;
    int newKeyCode = itemIdToKeyCode(keyComboPtr->getSelectedId());
    undoManager.beginNewTransaction("Change Key");
    for (auto &tree : selectedTrees) {
      if (tree.isValid())
        tree.setProperty("inputKey", newKeyCode, &undoManager);
    }
  };

  rowComp->editor = std::move(keyCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void MappingInspector::resized() {
  const int rowHeight = 25;
  const int separatorRowHeight = 15; // Phase 55.9: smaller for separator rows
  const int spacing = 4;             // Phase 55.10: tighter layout
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
  const int separatorTopMargin =
      12;                // Phase 55.11: extra space before separator
  const int spacing = 4; // Phase 55.10: matches resized()
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
  if (selectedTrees.empty())
    return juce::var();
  juce::var v = selectedTrees[0].getProperty(property);
  if (v.isVoid())
    return MappingDefinition::getDefaultValue(property.toString());
  return v;
}

void MappingInspector::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &deviceManager || source == &settingsManager ||
      source == touchpadMixerManager)
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
  // When target changes to PitchBend or SmartScaleBend, default "Reset pitch on
  // release" on
  if (property == juce::Identifier("adsrTarget")) {
    juce::String targetStr = tree.getProperty("adsrTarget", "CC").toString();
    if (targetStr.equalsIgnoreCase("PitchBend") ||
        targetStr.equalsIgnoreCase("SmartScaleBend"))
      tree.setProperty("sendReleaseValue", true, &undoManager);
  }
  bool needsRebuild = false;
  if (property == juce::Identifier("inputKey") ||
      property == juce::Identifier("inputAlias") ||
      property == juce::Identifier("type") ||
      property == juce::Identifier("adsrTarget") ||
      property == juce::Identifier("sendReleaseValue") ||
      property == juce::Identifier("useCustomEnvelope") ||
      property == juce::Identifier("releaseBehavior") ||
      property == juce::Identifier("forceAllLayers")) {
    needsRebuild = true;
  } else if (property == juce::Identifier("data1")) {
    if (allTreesHaveSameValue("type") &&
        getCommonValue("type").toString() == "Command") {
      needsRebuild = true;
    }
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

  if (presetManager &&
      (property == juce::Identifier("inputKey") ||
       property == juce::Identifier("inputAlias") ||
       property == juce::Identifier("releaseBehavior") ||
       property == juce::Identifier("followTranspose") ||
       property == juce::Identifier("deviceHash") ||
       property == juce::Identifier("pitchPadMode") ||
       property == juce::Identifier("pitchPadRestingPercent") ||
       property == juce::Identifier("type") ||
       property == juce::Identifier("channel") ||
       property == juce::Identifier("data1") ||
       property == juce::Identifier("data2") ||
       property == juce::Identifier("velRandom") ||
       property == juce::Identifier("touchpadThreshold") ||
       property == juce::Identifier("touchpadTriggerAbove") ||
       property == juce::Identifier("touchpadValueWhenOn") ||
       property == juce::Identifier("touchpadValueWhenOff") ||
       property == juce::Identifier("touchpadInputMin") ||
       property == juce::Identifier("touchpadInputMax") ||
       property == juce::Identifier("touchpadOutputMin") ||
       property == juce::Identifier("touchpadOutputMax") ||
       property == juce::Identifier("forceAllLayers") ||
       property == juce::Identifier("enabled"))) {
    presetManager->sendChangeMessage();
  }
}
