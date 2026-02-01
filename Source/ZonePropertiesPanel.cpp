#include "ZonePropertiesPanel.h"
#include "ScaleEditorComponent.h"
#include "ScaleLibrary.h"
#include "ZoneManager.h"
#include <algorithm>

static uintptr_t aliasNameToHash(const juce::String &aliasName) {
  if (aliasName.isEmpty() || aliasName == "Any / Master" ||
      aliasName == "Global (All Devices)" || aliasName == "Global" ||
      aliasName == "Unassigned")
    return 0;
  return static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
}

namespace {
struct LabelEditorRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::Component> editor;
  static constexpr int labelWidth = 120;
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

// Composite: [label] [checkbox] [slider] – label gets extra width so long text
// isn’t clipped
struct StrumTimingVariationRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::ToggleButton> check;
  std::unique_ptr<juce::Slider> slider;
  static constexpr int labelW = 200;
  static constexpr int checkW = 24;

  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelW));
    if (check)
      check->setBounds(r.removeFromLeft(checkW));
    if (slider)
      slider->setBounds(r);
  }
};

// Composite: [label "Add Bass:"] [checkbox] [slider "Octave"] – slider disabled
// when checkbox off
struct AddBassWithOctaveRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::ToggleButton> check;
  std::unique_ptr<juce::Slider> slider;
  static constexpr int labelW = 120;
  static constexpr int checkW = 24;

  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelW));
    if (check)
      check->setBounds(r.removeFromLeft(checkW));
    if (slider)
      slider->setBounds(r);
  }
};

// Composite: [label "Delay release:"] [checkbox] [slider] – slider disabled
// when checkbox off (Normal release only; no timer when off)
struct DelayReleaseRow : juce::Component {
  std::unique_ptr<juce::Label> label;
  std::unique_ptr<juce::ToggleButton> check;
  std::unique_ptr<juce::Slider> slider;
  static constexpr int labelW = 140;
  static constexpr int checkW = 24;

  void resized() override {
    auto r = getLocalBounds();
    if (label)
      label->setBounds(r.removeFromLeft(labelW));
    if (check)
      check->setBounds(r.removeFromLeft(checkW));
    if (slider)
      slider->setBounds(r);
  }
};
} // namespace

ZonePropertiesPanel::SeparatorComponent::SeparatorComponent(
    const juce::String &label, juce::Justification justification)
    : labelText(label), textAlign(justification) {}

void ZonePropertiesPanel::SeparatorComponent::paint(juce::Graphics &g) {
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
  int textLeft, textRight;
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
  if (textLeft - pad > bounds.getX())
    g.fillRect(bounds.getX(), lineY, textLeft - pad - bounds.getX(),
               lineHeight);
  if (textRight + pad < bounds.getRight())
    g.fillRect(textRight + pad, lineY, bounds.getRight() - (textRight + pad),
               lineHeight);
}

ZonePropertiesPanel::ZonePropertiesPanel(ZoneManager *zoneMgr,
                                         DeviceManager *deviceMgr,
                                         RawInputManager *rawInputMgr,
                                         ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr), deviceManager(deviceMgr),
      rawInputManager(rawInputMgr), scaleLibrary(scaleLib) {
  if (deviceManager)
    deviceManager->addChangeListener(this);
  if (scaleLibrary)
    scaleLibrary->addChangeListener(this);
  if (rawInputManager)
    rawInputManager->addListener(this);
}

ZonePropertiesPanel::~ZonePropertiesPanel() {
  captureKeysButtonRef = nullptr;
  removeKeysButtonRef = nullptr;
  chipListRef = nullptr;
  if (rawInputManager)
    rawInputManager->removeListener(this);
  if (deviceManager)
    deviceManager->removeChangeListener(this);
  if (scaleLibrary)
    scaleLibrary->removeChangeListener(this);
}

void ZonePropertiesPanel::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
  if (!currentZone && uiRows.empty()) {
    g.setColour(juce::Colours::grey);
    g.setFont(14.0f);
    g.drawText("No zone selected", getLocalBounds(),
               juce::Justification::centred, true);
  }
}

void ZonePropertiesPanel::setZone(std::shared_ptr<Zone> zone) {
  currentZone = zone;
  rebuildUI();
}

void ZonePropertiesPanel::rebuildUI() {
  if (onBeforeRebuild)
    onBeforeRebuild();

  captureKeysButtonRef = nullptr;
  removeKeysButtonRef = nullptr;
  chipListRef = nullptr;
  chipListRowIndex = -1;

  for (auto &row : uiRows) {
    for (auto &item : row.items) {
      if (item.component)
        removeChildComponent(item.component.get());
    }
  }
  uiRows.clear();

  if (!currentZone) {
    lastSchemaSignature_.clear();
    resized();
    if (onAfterRebuild)
      onAfterRebuild();
    return;
  }

  ZoneSchema schema = ZoneDefinition::getSchema(currentZone.get());
  lastSchemaSignature_ = ZoneDefinition::getSchemaSignature(currentZone.get());
  for (const auto &def : schema) {
    if (def.controlType == ZoneControl::Type::Separator) {
      UiRow row;
      row.isSeparatorRow = true;
      row.isChipListRow = false;
      auto sep =
          std::make_unique<SeparatorComponent>(def.label, def.separatorAlign);
      addAndMakeVisible(*sep);
      row.items.push_back({std::move(sep), 1.0f, false});
      uiRows.push_back(std::move(row));
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomAlias) {
      createAliasRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomLayer) {
      createLayerRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomName) {
      createNameRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomScale) {
      createScaleRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomKeyAssign) {
      createKeyAssignRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomChipList) {
      createChipListRow();
      continue;
    }
    if (def.controlType == ZoneControl::Type::CustomColor) {
      createColorRow();
      continue;
    }
    if (!def.sameLine || uiRows.empty()) {
      UiRow row;
      row.isSeparatorRow = false;
      row.isChipListRow = false;
      uiRows.push_back(std::move(row));
    }
    createControl(def, uiRows.back());
  }

  resized();
  if (onResizeRequested)
    onResizeRequested();
  if (onAfterRebuild)
    onAfterRebuild();
}

void ZonePropertiesPanel::createControl(const ZoneControl &def,
                                        UiRow &currentRow) {
  Zone *zone = currentZone.get();
  const bool affectsCache = def.affectsCache;
  auto addItem = [this, &currentRow](std::unique_ptr<juce::Component> comp,
                                     float weight, bool isAuto = false) {
    if (!comp)
      return;
    addAndMakeVisible(*comp);
    currentRow.items.push_back({std::move(comp), weight, isAuto});
  };

  auto rebuildIfNeeded = [this, affectsCache, zone]() {
    if (affectsCache && zoneManager && scaleLibrary)
      rebuildZoneCache();
    if (zoneManager)
      zoneManager->sendChangeMessage();
    if (onResizeRequested)
      onResizeRequested();
    // If visibility of controls would change (e.g. chord type on/off), rebuild
    // UI
    if (currentZone && zoneManager && currentZone.get() == zone) {
      juce::String sig = ZoneDefinition::getSchemaSignature(currentZone.get());
      if (sig != lastSchemaSignature_) {
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
      }
    }
  };

  switch (def.controlType) {
  case ZoneControl::Type::Slider: {
    auto sl = std::make_unique<juce::Slider>();
    sl->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      sl->setTextValueSuffix(" " + def.suffix);

    if (def.propertyKey == "rootNote") {
      sl->textFromValueFunction = [](double v) {
        return MidiNoteUtilities::getMidiNoteName(static_cast<int>(v));
      };
      sl->valueFromTextFunction = [](const juce::String &t) {
        return static_cast<double>(MidiNoteUtilities::getMidiNoteFromText(t));
      };
    }
    if (def.propertyKey == "chromaticOffset") {
      sl->textFromValueFunction = [](double v) {
        int i = static_cast<int>(v);
        if (i == 0)
          return juce::String("0");
        return i > 0 ? juce::String("+") + juce::String(i) + "st"
                     : juce::String(i) + "st";
      };
    }
    if (def.propertyKey == "degreeOffset" ||
        def.propertyKey == "gridInterval" ||
        def.propertyKey == "globalRootOctaveOffset") {
      sl->textFromValueFunction = [](double v) {
        int i = static_cast<int>(v);
        if (i == 0)
          return juce::String("0");
        return i > 0 ? juce::String("+") + juce::String(i) : juce::String(i);
      };
    }

    if (def.propertyKey == "rootNote")
      sl->setValue(zone->rootNote, juce::dontSendNotification);
    else if (def.propertyKey == "chromaticOffset")
      sl->setValue(zone->chromaticOffset, juce::dontSendNotification);
    else if (def.propertyKey == "degreeOffset")
      sl->setValue(zone->degreeOffset, juce::dontSendNotification);
    else if (def.propertyKey == "globalRootOctaveOffset")
      sl->setValue(zone->globalRootOctaveOffset, juce::dontSendNotification);
    else if (def.propertyKey == "baseVelocity")
      sl->setValue(zone->baseVelocity, juce::dontSendNotification);
    else if (def.propertyKey == "velocityRandom")
      sl->setValue(zone->velocityRandom, juce::dontSendNotification);
    else if (def.propertyKey == "ghostVelocityScale")
      sl->setValue(zone->ghostVelocityScale * 100.0,
                   juce::dontSendNotification);
    else if (def.propertyKey == "glideTimeMs")
      sl->setValue(zone->glideTimeMs, juce::dontSendNotification);
    else if (def.propertyKey == "maxGlideTimeMs")
      sl->setValue(zone->maxGlideTimeMs, juce::dontSendNotification);
    else if (def.propertyKey == "midiChannel")
      sl->setValue(zone->midiChannel, juce::dontSendNotification);
    else if (def.propertyKey == "guitarFretAnchor")
      sl->setValue(zone->guitarFretAnchor, juce::dontSendNotification);
    else if (def.propertyKey == "strumSpeedMs")
      sl->setValue(zone->strumSpeedMs, juce::dontSendNotification);
    else if (def.propertyKey == "releaseDurationMs")
      sl->setValue(zone->releaseDurationMs, juce::dontSendNotification);
    else if (def.propertyKey == "gridInterval")
      sl->setValue(zone->gridInterval, juce::dontSendNotification);

    if (def.propertyKey == "rootNote")
      sl->setEnabled(!zone->useGlobalRoot);
    if (def.propertyKey == "gridInterval")
      sl->setEnabled(zone->layoutStrategy == Zone::LayoutStrategy::Grid);

    juce::Slider *slPtr = sl.get();
    sl->onValueChange = [this, zone, slPtr, def, rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      double v = slPtr->getValue();
      if (def.propertyKey == "rootNote")
        zone->rootNote = static_cast<int>(v);
      else if (def.propertyKey == "chromaticOffset")
        zone->chromaticOffset = static_cast<int>(v);
      else if (def.propertyKey == "degreeOffset")
        zone->degreeOffset = static_cast<int>(v);
      else if (def.propertyKey == "globalRootOctaveOffset")
        zone->globalRootOctaveOffset = static_cast<int>(v);
      else if (def.propertyKey == "baseVelocity")
        zone->baseVelocity = static_cast<int>(v);
      else if (def.propertyKey == "velocityRandom")
        zone->velocityRandom = static_cast<int>(v);
      else if (def.propertyKey == "ghostVelocityScale")
        zone->ghostVelocityScale = static_cast<float>(v) / 100.0f;
      else if (def.propertyKey == "glideTimeMs")
        zone->glideTimeMs = static_cast<int>(v);
      else if (def.propertyKey == "maxGlideTimeMs")
        zone->maxGlideTimeMs = static_cast<int>(v);
      else if (def.propertyKey == "midiChannel")
        zone->midiChannel = static_cast<int>(v);
      else if (def.propertyKey == "guitarFretAnchor")
        zone->guitarFretAnchor = static_cast<int>(v);
      else if (def.propertyKey == "strumSpeedMs")
        zone->strumSpeedMs = static_cast<int>(v);
      else if (def.propertyKey == "releaseDurationMs")
        zone->releaseDurationMs = static_cast<int>(v);
      else if (def.propertyKey == "gridInterval")
        zone->gridInterval = static_cast<int>(v);
      rebuildIfNeeded();
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
  case ZoneControl::Type::StrumTimingVariation: {
    auto rowComp = std::make_unique<StrumTimingVariationRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->check = std::make_unique<juce::ToggleButton>();
    rowComp->check->setClickingTogglesState(true);
    rowComp->slider = std::make_unique<juce::Slider>();
    rowComp->slider->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      rowComp->slider->setTextValueSuffix(" " + def.suffix);

    rowComp->check->setToggleState(zone->strumTimingVariationOn,
                                   juce::dontSendNotification);
    rowComp->slider->setValue(zone->strumTimingVariationMs,
                              juce::dontSendNotification);
    rowComp->slider->setEnabled(zone->strumTimingVariationOn);

    rowComp->check->onClick = [this, zone, rowComp = rowComp.get(),
                               rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->strumTimingVariationOn = rowComp->check->getToggleState();
      rowComp->slider->setEnabled(zone->strumTimingVariationOn);
      rebuildIfNeeded();
    };
    rowComp->slider->onValueChange = [this, zone, rowComp = rowComp.get(),
                                      rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->strumTimingVariationMs =
          static_cast<int>(rowComp->slider->getValue());
      rebuildIfNeeded();
    };

    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->check);
    rowComp->addAndMakeVisible(*rowComp->slider);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case ZoneControl::Type::AddBassWithOctave: {
    auto rowComp = std::make_unique<AddBassWithOctaveRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->check = std::make_unique<juce::ToggleButton>();
    rowComp->check->setClickingTogglesState(true);
    rowComp->check->setButtonText(juce::String());
    rowComp->slider = std::make_unique<juce::Slider>();
    rowComp->slider->setRange(def.min, def.max, def.step);
    rowComp->slider->setNumDecimalPlacesToDisplay(0);

    rowComp->check->setToggleState(zone->addBassNote,
                                   juce::dontSendNotification);
    rowComp->slider->setValue(zone->bassOctaveOffset,
                              juce::dontSendNotification);
    rowComp->slider->setEnabled(zone->addBassNote);

    rowComp->check->onClick = [this, zone, rowComp = rowComp.get(),
                               rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->addBassNote = rowComp->check->getToggleState();
      rowComp->slider->setEnabled(zone->addBassNote);
      rebuildIfNeeded();
    };
    rowComp->slider->onValueChange = [this, zone, rowComp = rowComp.get(),
                                      rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->bassOctaveOffset = static_cast<int>(rowComp->slider->getValue());
      rebuildIfNeeded();
    };

    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->check);
    rowComp->addAndMakeVisible(*rowComp->slider);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case ZoneControl::Type::DelayRelease: {
    auto rowComp = std::make_unique<DelayReleaseRow>();
    rowComp->label = std::make_unique<juce::Label>();
    rowComp->label->setText(def.label + ":", juce::dontSendNotification);
    rowComp->check = std::make_unique<juce::ToggleButton>();
    rowComp->check->setClickingTogglesState(true);
    rowComp->check->setButtonText(juce::String());
    rowComp->slider = std::make_unique<juce::Slider>();
    rowComp->slider->setRange(def.min, def.max, def.step);
    if (def.suffix.isNotEmpty())
      rowComp->slider->setTextValueSuffix(" " + def.suffix);

    rowComp->check->setToggleState(zone->delayReleaseOn,
                                   juce::dontSendNotification);
    rowComp->slider->setValue(zone->releaseDurationMs,
                              juce::dontSendNotification);
    rowComp->slider->setEnabled(zone->delayReleaseOn);

    rowComp->check->onClick = [this, zone, rowComp = rowComp.get(),
                               rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->delayReleaseOn = rowComp->check->getToggleState();
      rowComp->slider->setEnabled(zone->delayReleaseOn);
      rebuildIfNeeded();
    };
    rowComp->slider->onValueChange = [this, zone, rowComp = rowComp.get(),
                                      rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      zone->releaseDurationMs = static_cast<int>(rowComp->slider->getValue());
      rebuildIfNeeded();
    };

    rowComp->addAndMakeVisible(*rowComp->label);
    rowComp->addAndMakeVisible(*rowComp->check);
    rowComp->addAndMakeVisible(*rowComp->slider);
    addItem(std::move(rowComp), def.widthWeight, def.autoWidth);
    break;
  }
  case ZoneControl::Type::ComboBox: {
    auto cb = std::make_unique<juce::ComboBox>();
    for (const auto &p : def.options)
      cb->addItem(p.second, p.first);

    auto setComboFromZone = [cb = cb.get(), zone, &def]() {
      if (def.propertyKey == "showRomanNumerals")
        cb->setSelectedId(zone->showRomanNumerals ? 2 : 1,
                          juce::dontSendNotification);
      else if (def.propertyKey == "polyphonyMode") {
        int id = (zone->polyphonyMode == PolyphonyMode::Poly)   ? 1
                 : (zone->polyphonyMode == PolyphonyMode::Mono) ? 2
                                                                : 3;
        cb->setSelectedId(id, juce::dontSendNotification);
      } else if (def.propertyKey == "instrumentMode")
        cb->setSelectedId(
            zone->instrumentMode == Zone::InstrumentMode::Piano ? 1 : 2,
            juce::dontSendNotification);
      else if (def.propertyKey == "pianoVoicingStyle") {
        int id = (zone->pianoVoicingStyle == Zone::PianoVoicingStyle::Block) ? 1
                 : (zone->pianoVoicingStyle == Zone::PianoVoicingStyle::Close)
                     ? 2
                     : 3;
        cb->setSelectedId(id, juce::dontSendNotification);
      } else if (def.propertyKey == "guitarPlayerPosition")
        cb->setSelectedId(zone->guitarPlayerPosition ==
                                  Zone::GuitarPlayerPosition::Campfire
                              ? 1
                              : 2,
                          juce::dontSendNotification);
      else if (def.propertyKey == "strumPattern") {
        int id = (zone->strumPattern == Zone::StrumPattern::Down) ? 1
                 : (zone->strumPattern == Zone::StrumPattern::Up) ? 2
                                                                  : 3;
        cb->setSelectedId(id, juce::dontSendNotification);
      } else if (def.propertyKey == "chordType") {
        int id = 1;
        if (zone->chordType == ChordUtilities::ChordType::Triad)
          id = 2;
        else if (zone->chordType == ChordUtilities::ChordType::Seventh)
          id = 3;
        else if (zone->chordType == ChordUtilities::ChordType::Ninth)
          id = 4;
        else if (zone->chordType == ChordUtilities::ChordType::Power5)
          id = 5;
        cb->setSelectedId(id, juce::dontSendNotification);
      } else if (def.propertyKey == "playMode")
        cb->setSelectedId(zone->playMode == Zone::PlayMode::Direct ? 1 : 2,
                          juce::dontSendNotification);
      else if (def.propertyKey == "releaseBehavior")
        cb->setSelectedId(
            zone->releaseBehavior == Zone::ReleaseBehavior::Normal ? 1 : 2,
            juce::dontSendNotification);
      else if (def.propertyKey == "layoutStrategy") {
        int id = (zone->layoutStrategy == Zone::LayoutStrategy::Linear) ? 1
                 : (zone->layoutStrategy == Zone::LayoutStrategy::Grid) ? 2
                                                                        : 3;
        cb->setSelectedId(id, juce::dontSendNotification);
      }
    };
    setComboFromZone();

    juce::ComboBox *cbPtr = cb.get();
    cb->onChange = [this, zone, cbPtr, def, rebuildIfNeeded,
                    setComboFromZone]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      int id = cbPtr->getSelectedId();
      if (def.propertyKey == "showRomanNumerals") {
        zone->showRomanNumerals = (id == 2);
      } else if (def.propertyKey == "polyphonyMode") {
        zone->polyphonyMode = (id == 1)   ? PolyphonyMode::Poly
                              : (id == 2) ? PolyphonyMode::Mono
                                          : PolyphonyMode::Legato;
      } else if (def.propertyKey == "instrumentMode") {
        zone->instrumentMode = (id == 1) ? Zone::InstrumentMode::Piano
                                         : Zone::InstrumentMode::Guitar;
      } else if (def.propertyKey == "pianoVoicingStyle") {
        zone->pianoVoicingStyle = (id == 1)   ? Zone::PianoVoicingStyle::Block
                                  : (id == 2) ? Zone::PianoVoicingStyle::Close
                                              : Zone::PianoVoicingStyle::Open;
      } else if (def.propertyKey == "guitarPlayerPosition") {
        zone->guitarPlayerPosition = (id == 1)
                                         ? Zone::GuitarPlayerPosition::Campfire
                                         : Zone::GuitarPlayerPosition::Rhythm;
      } else if (def.propertyKey == "strumPattern") {
        zone->strumPattern = (id == 1)   ? Zone::StrumPattern::Down
                             : (id == 2) ? Zone::StrumPattern::Up
                                         : Zone::StrumPattern::AutoAlternating;
      } else if (def.propertyKey == "chordType") {
        zone->chordType = (id == 1)   ? ChordUtilities::ChordType::None
                          : (id == 2) ? ChordUtilities::ChordType::Triad
                          : (id == 3) ? ChordUtilities::ChordType::Seventh
                          : (id == 4) ? ChordUtilities::ChordType::Ninth
                                      : ChordUtilities::ChordType::Power5;
      } else if (def.propertyKey == "playMode") {
        zone->playMode =
            (id == 1) ? Zone::PlayMode::Direct : Zone::PlayMode::Strum;
      } else if (def.propertyKey == "releaseBehavior") {
        zone->releaseBehavior = (id == 1) ? Zone::ReleaseBehavior::Normal
                                          : Zone::ReleaseBehavior::Sustain;
      } else if (def.propertyKey == "layoutStrategy") {
        zone->layoutStrategy = (id == 1)   ? Zone::LayoutStrategy::Linear
                               : (id == 2) ? Zone::LayoutStrategy::Grid
                                           : Zone::LayoutStrategy::Piano;
        // Rebuild UI so Grid Interval slider enable state updates
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
      }
      rebuildIfNeeded();
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
  case ZoneControl::Type::Toggle: {
    auto tb = std::make_unique<juce::ToggleButton>();
    if (def.propertyKey == "useGlobalRoot")
      tb->setButtonText("Global");
    else if (def.propertyKey == "useGlobalScale")
      tb->setButtonText("Global");
    else if (def.propertyKey == "ignoreGlobalTranspose" ||
             def.propertyKey == "ignoreGlobalSustain")
      tb->setButtonText(juce::String()); // Label is in row; no text on checkbox
    else
      tb->setButtonText(def.label);

    if (def.propertyKey == "useGlobalRoot")
      tb->setToggleState(zone->useGlobalRoot, juce::dontSendNotification);
    else if (def.propertyKey == "useGlobalScale")
      tb->setToggleState(zone->useGlobalScale, juce::dontSendNotification);
    else if (def.propertyKey == "ignoreGlobalTranspose")
      tb->setToggleState(zone->ignoreGlobalTranspose,
                         juce::dontSendNotification);
    else if (def.propertyKey == "ignoreGlobalSustain")
      tb->setToggleState(zone->ignoreGlobalSustain, juce::dontSendNotification);
    else if (def.propertyKey == "strictGhostHarmony")
      tb->setToggleState(zone->strictGhostHarmony, juce::dontSendNotification);
    else if (def.propertyKey == "isAdaptiveGlide")
      tb->setToggleState(zone->isAdaptiveGlide, juce::dontSendNotification);
    else if (def.propertyKey == "strumGhostNotes")
      tb->setToggleState(zone->strumGhostNotes, juce::dontSendNotification);

    juce::ToggleButton *tbPtr = tb.get();
    tb->onClick = [this, zone, tbPtr, def, rebuildIfNeeded]() {
      if (!currentZone || currentZone.get() != zone)
        return;
      bool v = tbPtr->getToggleState();
      if (def.propertyKey == "useGlobalRoot")
        zone->useGlobalRoot = v;
      else if (def.propertyKey == "useGlobalScale")
        zone->useGlobalScale = v;
      else if (def.propertyKey == "ignoreGlobalTranspose")
        zone->ignoreGlobalTranspose = v;
      else if (def.propertyKey == "ignoreGlobalSustain")
        zone->ignoreGlobalSustain = v;
      else if (def.propertyKey == "strictGhostHarmony")
        zone->strictGhostHarmony = v;
      else if (def.propertyKey == "isAdaptiveGlide")
        zone->isAdaptiveGlide = v;
      else if (def.propertyKey == "strumGhostNotes")
        zone->strumGhostNotes = v;
      rebuildIfNeeded();
    };

    if (def.label.isEmpty() || def.propertyKey == "useGlobalRoot" ||
        def.propertyKey == "useGlobalScale") {
      addItem(std::move(tb), def.widthWeight, def.autoWidth);
    } else {
      auto lbl = std::make_unique<juce::Label>("", def.label + ":");
      lbl->setJustificationType(juce::Justification::centredLeft);
      addItem(std::make_unique<LabeledControl>(std::move(lbl), std::move(tb)),
              def.widthWeight, def.autoWidth);
    }
    break;
  }
  case ZoneControl::Type::LabelOnly: {
    auto label = std::make_unique<juce::Label>();
    label->setText(def.label, juce::dontSendNotification);
    label->setColour(juce::Label::textColourId, juce::Colours::grey);
    addItem(std::move(label), 1.0f, false);
    break;
  }
  case ZoneControl::Type::LabelOnlyWrappable: {
    struct WrappableLabel : juce::Component {
      juce::String text;
      void paint(juce::Graphics &g) override {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::Font(13.0f));
        g.drawFittedText(text, getLocalBounds().reduced(4),
                         juce::Justification::left, 10);
      }
    };
    auto comp = std::make_unique<WrappableLabel>();
    comp->text = def.label;
    currentRow.isWrappableLabelRow = true;
    addItem(std::move(comp), 1.0f, false);
    break;
  }
  default:
    break;
  }
}

void ZonePropertiesPanel::createAliasRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;
  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Device Alias:", juce::dontSendNotification);

  auto aliasCombo = std::make_unique<juce::ComboBox>();
  aliasCombo->addItem("Global (All Devices)", 1);
  if (deviceManager) {
    auto aliases = deviceManager->getAllAliasNames();
    for (int i = 0; i < aliases.size(); ++i)
      aliasCombo->addItem(aliases[i], i + 2);
  }
  if (currentZone && currentZone->targetAliasHash == 0)
    aliasCombo->setSelectedItemIndex(0, juce::dontSendNotification);
  else if (currentZone && deviceManager) {
    juce::String name =
        deviceManager->getAliasName(currentZone->targetAliasHash);
    for (int i = 0; i < aliasCombo->getNumItems(); ++i) {
      if (aliasCombo->getItemText(i) == name) {
        aliasCombo->setSelectedItemIndex(i, juce::dontSendNotification);
        break;
      }
    }
  }

  juce::ComboBox *cbPtr = aliasCombo.get();
  aliasCombo->onChange = [this, cbPtr]() {
    if (!currentZone || !deviceManager)
      return;
    int idx = cbPtr->getSelectedItemIndex();
    if (idx >= 0) {
      juce::String name = cbPtr->getItemText(idx);
      currentZone->targetAliasHash = aliasNameToHash(name);
      if (zoneManager) {
        zoneManager->rebuildLookupTable();
        zoneManager->sendChangeMessage();
      }
    }
  };

  rowComp->editor = std::move(aliasCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createLayerRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;
  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Layer:", juce::dontSendNotification);

  auto layerCombo = std::make_unique<juce::ComboBox>();
  for (int i = 0; i <= 8; ++i)
    layerCombo->addItem(
        i == 0 ? "0: Base" : (juce::String(i) + ": Layer " + juce::String(i)),
        i + 1);
  if (currentZone)
    layerCombo->setSelectedId(currentZone->layerID + 1,
                              juce::dontSendNotification);

  juce::ComboBox *cbPtr = layerCombo.get();
  layerCombo->onChange = [this, cbPtr]() {
    if (!currentZone || !zoneManager)
      return;
    int id = cbPtr->getSelectedId() - 1;
    if (id >= 0 && id <= 8) {
      currentZone->layerID = id;
      zoneManager->rebuildLookupTable();
      zoneManager->sendChangeMessage();
    }
  };

  rowComp->editor = std::move(layerCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createNameRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;
  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Zone Name:", juce::dontSendNotification);

  auto nameEdit = std::make_unique<juce::TextEditor>();
  if (currentZone)
    nameEdit->setText(currentZone->name, false);
  juce::TextEditor *tePtr = nameEdit.get();
  nameEdit->onTextChange = [this, tePtr]() {
    if (currentZone)
      currentZone->name = tePtr->getText();
  };

  rowComp->editor = std::move(nameEdit);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createScaleRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;

  auto scaleCombo = std::make_unique<juce::ComboBox>();
  scaleCombo->addItem("Major", 1);
  if (scaleLibrary) {
    auto names = scaleLibrary->getScaleNames();
    for (int i = 0; i < names.size(); ++i)
      scaleCombo->addItem(names[i], i + 2);
  }
  if (currentZone) {
    for (int i = 0; i < scaleCombo->getNumItems(); ++i) {
      if (scaleCombo->getItemText(i) == currentZone->scaleName) {
        scaleCombo->setSelectedItemIndex(i, juce::dontSendNotification);
        break;
      }
    }
  }

  auto globalToggle = std::make_unique<juce::ToggleButton>();
  globalToggle->setButtonText("Global");
  if (currentZone)
    globalToggle->setToggleState(currentZone->useGlobalScale,
                                 juce::dontSendNotification);

  auto editBtn = std::make_unique<juce::TextButton>("Edit...");
  editBtn->onClick = [this]() {
    if (!scaleLibrary)
      return;
    auto *editor = new ScaleEditorComponent(scaleLibrary);
    editor->setSize(600, 400);
    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned(editor);
    opts.dialogTitle = "Scale Editor";
    opts.dialogBackgroundColour = juce::Colour(0xff222222);
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = false;
    opts.resizable = true;
    opts.useBottomRightCornerResizer = true;
    opts.componentToCentreAround = this;
    opts.launchAsync();
  };

  scaleCombo->setEnabled(currentZone && !currentZone->useGlobalScale);
  juce::ComboBox *scalePtr = scaleCombo.get();
  juce::ToggleButton *globalPtr = globalToggle.get();
  scaleCombo->onChange = [this, scalePtr]() {
    if (!currentZone || !scaleLibrary)
      return;
    int idx = scalePtr->getSelectedItemIndex();
    if (idx >= 0) {
      currentZone->scaleName = scalePtr->getItemText(idx);
      rebuildZoneCache();
    }
  };
  globalToggle->onClick = [this, globalPtr, scalePtr]() {
    if (!currentZone || !zoneManager)
      return;
    currentZone->useGlobalScale = globalPtr->getToggleState();
    scalePtr->setEnabled(!currentZone->useGlobalScale);
    rebuildZoneCache();
    if (zoneManager) {
      juce::String sig = ZoneDefinition::getSchemaSignature(currentZone.get());
      if (sig != lastSchemaSignature_)
        juce::MessageManager::callAsync([this]() { rebuildUI(); });
    }
  };

  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Scale:", juce::dontSendNotification);
  rowComp->editor = std::move(scaleCombo);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  addAndMakeVisible(*globalToggle);
  addAndMakeVisible(*editBtn);
  row.items.push_back({std::move(rowComp), 0.7f, false});
  row.items.push_back({std::move(globalToggle), 0.15f, false});
  row.items.push_back({std::move(editBtn), 0.15f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createKeyAssignRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;

  auto captureBtn = std::make_unique<juce::ToggleButton>();
  captureBtn->setButtonText("Assign Keys");
  captureBtn->setClickingTogglesState(true);
  auto removeBtn = std::make_unique<juce::ToggleButton>();
  removeBtn->setButtonText("Remove Keys");
  removeBtn->setClickingTogglesState(true);

  juce::ToggleButton *removeBtnPtr = removeBtn.get();
  juce::ToggleButton *captureBtnPtr = captureBtn.get();
  captureBtn->onClick = [this, removeBtnPtr]() {
    if (captureKeysButtonRef && captureKeysButtonRef->getToggleState())
      removeBtnPtr->setToggleState(false, juce::dontSendNotification);
  };
  removeBtn->onClick = [this, captureBtnPtr]() {
    if (removeKeysButtonRef && removeKeysButtonRef->getToggleState())
      captureBtnPtr->setToggleState(false, juce::dontSendNotification);
  };

  captureKeysButtonRef = captureBtn.get();
  removeKeysButtonRef = removeBtn.get();

  addAndMakeVisible(*captureBtn);
  addAndMakeVisible(*removeBtn);
  row.items.push_back({std::move(captureBtn), 0.5f, false});
  row.items.push_back({std::move(removeBtn), 0.5f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createChipListRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = true;
  chipListRowIndex = static_cast<int>(uiRows.size());

  auto chipList = std::make_unique<KeyChipList>();
  if (currentZone)
    chipList->setKeys(currentZone->inputKeyCodes);
  chipList->onKeyRemoved = [this](int keyCode) {
    if (!currentZone || !scaleLibrary || !zoneManager)
      return;
    currentZone->removeKey(keyCode);
    if (chipListRef)
      chipListRef->setKeys(currentZone->inputKeyCodes);
    rebuildZoneCache();
    zoneManager->rebuildLookupTable();
    zoneManager->sendChangeMessage();
    if (onResizeRequested)
      onResizeRequested();
  };
  chipListRef = chipList.get();

  addAndMakeVisible(*chipList);
  row.items.push_back({std::move(chipList), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::createColorRow() {
  UiRow row;
  row.isSeparatorRow = false;
  row.isChipListRow = false;

  auto rowComp = std::make_unique<LabelEditorRow>();
  rowComp->label = std::make_unique<juce::Label>();
  rowComp->label->setText("Zone Color:", juce::dontSendNotification);

  auto colorBtn = std::make_unique<juce::TextButton>("Color");
  if (currentZone)
    colorBtn->setColour(juce::TextButton::buttonColourId,
                        currentZone->zoneColor);
  juce::TextButton *btnPtr = colorBtn.get();
  colorBtn->onClick = [this, btnPtr]() {
    if (!currentZone)
      return;
    int flags = juce::ColourSelector::showColourspace |
                juce::ColourSelector::showSliders |
                juce::ColourSelector::showColourAtTop;
    auto *sel = new juce::ColourSelector(flags);
    sel->setName("Zone Color");
    sel->setCurrentColour(currentZone->zoneColor);
    sel->setSize(400, 300);
    class ColorListener : public juce::ChangeListener {
    public:
      ColorListener(Zone *z, ZoneManager *zm, juce::TextButton *b)
          : zone(z), zoneMgr(zm), button(b) {}
      void changeListenerCallback(juce::ChangeBroadcaster *source) override {
        auto *selector = dynamic_cast<juce::ColourSelector *>(source);
        if (zone && selector) {
          zone->zoneColor = selector->getCurrentColour();
          button->setColour(juce::TextButton::buttonColourId, zone->zoneColor);
          button->repaint();
          if (zoneMgr)
            zoneMgr->sendChangeMessage();
        }
      }
      Zone *zone;
      ZoneManager *zoneMgr;
      juce::TextButton *button;
    };
    auto *listener = new ColorListener(currentZone.get(), zoneManager, btnPtr);
    sel->addChangeListener(listener);
    juce::CallOutBox::launchAsynchronously(
        std::unique_ptr<juce::Component>(sel), btnPtr->getScreenBounds(), this);
  };

  rowComp->editor = std::move(colorBtn);
  rowComp->addAndMakeVisible(*rowComp->label);
  rowComp->addAndMakeVisible(*rowComp->editor);
  addAndMakeVisible(*rowComp);
  row.items.push_back({std::move(rowComp), 1.0f, false});
  uiRows.push_back(std::move(row));
}

void ZonePropertiesPanel::resized() {
  const int rowHeight = 28;
  const int separatorRowHeight = 15;
  const int spacing = 4;
  const int topPadding = 8;
  const int extraSeparatorMargin = 12;
  auto bounds = getLocalBounds().reduced(8);
  int y = bounds.getY() + topPadding;

  for (size_t rowIdx = 0; rowIdx < uiRows.size(); ++rowIdx) {
    auto &row = uiRows[rowIdx];
    if (row.items.empty())
      continue;

    if (row.isSeparatorRow)
      y += extraSeparatorMargin;

    int h = row.isSeparatorRow ? separatorRowHeight : rowHeight;
    if (row.isWrappableLabelRow)
      h = 44;
    if (row.isChipListRow && chipListRef && currentZone) {
      int n = static_cast<int>(currentZone->inputKeyCodes.size());
      const int chipW = 64, chipH = 28;
      int chipsPerRow = std::max(1, bounds.getWidth() / chipW);
      int rows = (n + chipsPerRow - 1) / chipsPerRow;
      h = juce::jmax(120, rows * chipH + 16);
    }

    int totalAvailable = bounds.getWidth();
    int usedWidth = 0;
    float totalWeight = 0.0f;
    for (auto &item : row.items) {
      if (item.isAutoWidth) {
        if (auto *lc = dynamic_cast<LabeledControl *>(item.component.get()))
          usedWidth += lc->getIdealWidth();
        else
          usedWidth += 80;
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
          w = 80;
      } else {
        w = (totalWeight > 0.0f)
                ? static_cast<int>((item.weight / totalWeight) * remainingWidth)
                : remainingWidth;
      }
      if (item.component)
        item.component->setBounds(x, y, w, h);
      x += w;
    }
    y += h + spacing;
  }
  setSize(getWidth(), y + 8);
}

int ZonePropertiesPanel::getRequiredHeight() const {
  const int controlHeight = 28;
  const int separatorHeight = 15;
  const int separatorTopMargin = 12;
  const int spacing = 4;
  const int topPadding = 8;
  const int bottomPadding = 8;
  int total = topPadding + bottomPadding;
  for (size_t i = 0; i < uiRows.size(); ++i) {
    const auto &row = uiRows[i];
    if (row.isSeparatorRow)
      total += separatorTopMargin;
    int h = row.isSeparatorRow ? separatorHeight : controlHeight;
    if (row.isWrappableLabelRow)
      h = 44;
    if (row.isChipListRow && chipListRef && currentZone) {
      int n = static_cast<int>(currentZone->inputKeyCodes.size());
      const int chipW = 64, chipH = 28;
      int availW = getWidth() > 0 ? getWidth() - 16 : 400;
      int chipsPerRow = std::max(1, availW / chipW);
      int rows = (n + chipsPerRow - 1) / chipsPerRow;
      h = juce::jmax(120, rows * chipH + 16);
    }
    total += h + spacing;
  }
  return total;
}

void ZonePropertiesPanel::rebuildZoneCache() {
  if (!currentZone || !zoneManager || !scaleLibrary)
    return;
  std::vector<int> intervals;
  int root;
  if (currentZone->useGlobalScale)
    intervals = scaleLibrary->getIntervals(zoneManager->getGlobalScaleName());
  else
    intervals = scaleLibrary->getIntervals(currentZone->scaleName);
  if (currentZone->useGlobalRoot)
    root = zoneManager->getGlobalRootNote();
  else
    root = currentZone->rootNote;
  currentZone->rebuildCache(intervals, root);
  zoneManager->sendChangeMessage();
}

void ZonePropertiesPanel::changeListenerCallback(
    juce::ChangeBroadcaster *source) {
  if (source == deviceManager || source == scaleLibrary) {
    juce::MessageManager::callAsync([this]() { rebuildUI(); });
  }
}

void ZonePropertiesPanel::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                                            bool isDown) {
  if (!currentZone || !isDown)
    return;

  if (captureKeysButtonRef && captureKeysButtonRef->getToggleState()) {
    auto &keys = currentZone->inputKeyCodes;
    if (std::find(keys.begin(), keys.end(), keyCode) == keys.end()) {
      keys.push_back(keyCode);
      juce::MessageManager::callAsync([this]() {
        if (chipListRef)
          chipListRef->setKeys(currentZone->inputKeyCodes);
        rebuildZoneCache();
        if (zoneManager) {
          zoneManager->rebuildLookupTable();
          zoneManager->sendChangeMessage();
        }
        if (onResizeRequested)
          onResizeRequested();
      });
    }
    return;
  }

  if (removeKeysButtonRef && removeKeysButtonRef->getToggleState()) {
    auto it = std::find(currentZone->inputKeyCodes.begin(),
                        currentZone->inputKeyCodes.end(), keyCode);
    if (it != currentZone->inputKeyCodes.end()) {
      currentZone->removeKey(keyCode);
      juce::MessageManager::callAsync([this]() {
        if (chipListRef)
          chipListRef->setKeys(currentZone->inputKeyCodes);
        rebuildZoneCache();
        if (zoneManager) {
          zoneManager->rebuildLookupTable();
          zoneManager->sendChangeMessage();
        }
        if (onResizeRequested)
          onResizeRequested();
      });
    }
    return;
  }
}

void ZonePropertiesPanel::handleAxisEvent(uintptr_t deviceHandle, int inputCode,
                                          float value) {}
