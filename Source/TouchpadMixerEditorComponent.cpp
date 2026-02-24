#include "TouchpadMixerEditorComponent.h"
#include "MappingDefinition.h"
#include "MappingInspectorLogic.h"
#include "MappingTypes.h"
#include "TouchpadMixerDefinition.h"
#include "SettingsManager.h"
#include "ScaleEditorComponent.h"
#include "ScaleLibrary.h"
#include <functional>

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
    TouchpadMixerManager *mgr, SettingsManager *settingsMgr,
    ScaleLibrary *scaleLib)
    : manager(mgr), settingsManager(settingsMgr), scaleLibrary(scaleLib) {
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
  selectionKind = (config != nullptr && index >= 0) ? SelectionKind::Layout
                                                   : SelectionKind::None;
  selectedLayoutIndex = index;
  selectedMappingIndex = -1;
  if (config)
    currentConfig = *config;
  else
    currentConfig = TouchpadMixerConfig();
  rebuildUI();
  setEnabled(config != nullptr);
}

void TouchpadMixerEditorComponent::setMapping(
    int index, const TouchpadMappingConfig *config) {
  selectionKind = (config != nullptr && index >= 0) ? SelectionKind::Mapping
                                                   : SelectionKind::None;
  selectedMappingIndex = index;
  selectedLayoutIndex = -1;
  if (config)
    currentMapping = *config;
  else
    currentMapping = TouchpadMappingConfig();
  rebuildUI();
  setEnabled(config != nullptr);
}

juce::var TouchpadMixerEditorComponent::getConfigValue(
    const juce::String &propertyId) const {
  const bool isLayout = (selectionKind == SelectionKind::Layout);
  const bool isMapping = (selectionKind == SelectionKind::Mapping);

  if (propertyId == "type") {
    if (isLayout) {
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
    // For mappings, read type from the mapping ValueTree.
    if (isMapping && currentMapping.mapping.isValid()) {
      juce::String typeStr =
          currentMapping.mapping.getProperty("type", "Note").toString().trim();
      if (typeStr.equalsIgnoreCase("Note"))
        return juce::var(1);
      if (typeStr.equalsIgnoreCase("Expression"))
        return juce::var(2);
      if (typeStr.equalsIgnoreCase("Command"))
        return juce::var(3);
      return juce::var(1); // default to Note
    }
    return juce::var();
  }
  if (propertyId == "name")
    return juce::var(isLayout ? juce::String(currentConfig.name)
                              : juce::String(currentMapping.name));
  if (propertyId == "layerId")
    return juce::var((isLayout ? currentConfig.layerId : currentMapping.layerId) +
                     1); // combo IDs 1..9
  if (propertyId == "layoutGroupId")
    return juce::var(isLayout ? currentConfig.layoutGroupId
                              : currentMapping.layoutGroupId);
  if (propertyId == "midiChannel")
    return juce::var(isLayout ? currentConfig.midiChannel
                              : currentMapping.midiChannel);
  if (propertyId == "regionLeft")
    return juce::var(static_cast<double>(
        isLayout ? currentConfig.region.left : currentMapping.region.left));
  if (propertyId == "regionTop")
    return juce::var(static_cast<double>(
        isLayout ? currentConfig.region.top : currentMapping.region.top));
  if (propertyId == "regionRight")
    return juce::var(static_cast<double>(
        isLayout ? currentConfig.region.right : currentMapping.region.right));
  if (propertyId == "regionBottom")
    return juce::var(static_cast<double>(
        isLayout ? currentConfig.region.bottom : currentMapping.region.bottom));
  if (propertyId == "zIndex")
    return juce::var(isLayout ? currentConfig.zIndex : currentMapping.zIndex);
  if (propertyId == "regionLock")
    return juce::var(isLayout ? currentConfig.regionLock
                              : currentMapping.regionLock);

  // Mapping-specific properties (read from mapping ValueTree).
  if (isMapping && currentMapping.mapping.isValid()) {
    // Command virtual properties: read from data1/data2, return combo ID
    if (propertyId == "sustainStyle") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      return juce::var((data1 >= 0 && data1 <= 2) ? (data1 + 1) : 1);
    }
    if (propertyId == "panicMode") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      int data2 = (int)currentMapping.mapping.getProperty("data2", 0);
      return juce::var((data2 == 2) ? 3 : (data1 == 5 || data2 == 1) ? 2 : 1);
    }
    if (propertyId == "layerStyle") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      return juce::var((data1 == 11) ? 2 : 1);
    }
    if (propertyId == "commandCategory") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      const int sustainMin = 0, sustainMax = 2;
      if (data1 >= sustainMin && data1 <= sustainMax)
        return juce::var(100); // Sustain
      if (data1 == (int)MIDIQy::CommandID::LatchToggle)
        return juce::var(101); // Latch
      if (data1 == (int)MIDIQy::CommandID::Panic ||
          data1 == (int)MIDIQy::CommandID::PanicLatch)
        return juce::var(102); // Panic
      if (data1 == (int)MIDIQy::CommandID::Transpose ||
          data1 == (int)MIDIQy::CommandID::GlobalPitchDown)
        return juce::var(103); // Transpose
      if (data1 == (int)MIDIQy::CommandID::GlobalModeUp ||
          data1 == (int)MIDIQy::CommandID::GlobalModeDown)
        return juce::var(104); // Global mode
      if (data1 == (int)MIDIQy::CommandID::GlobalRootUp ||
          data1 == (int)MIDIQy::CommandID::GlobalRootDown ||
          data1 == (int)MIDIQy::CommandID::GlobalRootSet)
        return juce::var(105); // Global Root
      if (data1 == (int)MIDIQy::CommandID::GlobalScaleNext ||
          data1 == (int)MIDIQy::CommandID::GlobalScalePrev ||
          data1 == (int)MIDIQy::CommandID::GlobalScaleSet)
        return juce::var(106); // Global Scale
      const int layerMomentary =
          static_cast<int>(MIDIQy::CommandID::LayerMomentary);
      const int layerToggle =
          static_cast<int>(MIDIQy::CommandID::LayerToggle);
      if (data1 == layerMomentary || data1 == layerToggle)
        return juce::var(110); // Layer
      return juce::var(100);
    }
    if (propertyId == "globalModeDirection") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      int id =
          (data1 == (int)MIDIQy::CommandID::GlobalModeDown) ? 2 : 1;
      return juce::var(id);
    }
    if (propertyId == "globalRootMode") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      int id = 1;
      if (data1 == (int)MIDIQy::CommandID::GlobalRootDown)
        id = 2;
      else if (data1 == (int)MIDIQy::CommandID::GlobalRootSet)
        id = 3;
      return juce::var(id);
    }
    if (propertyId == "globalScaleMode") {
      int data1 = (int)currentMapping.mapping.getProperty("data1", 0);
      int id = 1;
      if (data1 == (int)MIDIQy::CommandID::GlobalScalePrev)
        id = 2;
      else if (data1 == (int)MIDIQy::CommandID::GlobalScaleSet)
        id = 3;
      return juce::var(id);
    }
    // All mapping properties are stored in the ValueTree.
    if (currentMapping.mapping.hasProperty(propertyId)) {
      juce::var propVal = currentMapping.mapping.getProperty(propertyId);
      // Convert string properties to ComboBox IDs
      if (propertyId == "releaseBehavior") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Send Note Off"))
          return juce::var(1);
        else if (str.equalsIgnoreCase("Sustain until retrigger"))
          return juce::var(2);
        else if (str.equalsIgnoreCase("Always Latch"))
          return juce::var(3);
        return juce::var(1); // default
      } else if (propertyId == "touchpadHoldBehavior") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Hold to not send note off immediately"))
          return juce::var(1);
        else if (str.equalsIgnoreCase("Ignore, send note off immediately"))
          return juce::var(2);
        return juce::var(1); // default
      } else if (propertyId == "adsrTarget") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("CC"))
          return juce::var(1);
        if (str.equalsIgnoreCase("PitchBend"))
          return juce::var(2);
        if (str.equalsIgnoreCase("SmartScaleBend"))
          return juce::var(3);
        return juce::var(1); // default CC
      } else if (propertyId == "expressionCCMode") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Position")) return juce::var(1);
        if (str.equalsIgnoreCase("Slide")) return juce::var(2);
        if (str.equalsIgnoreCase("Encoder")) return juce::var(3);
        return juce::var(1);
      } else if (propertyId == "encoderAxis") {
        int val = static_cast<int>(propVal);
        return juce::var(juce::jlimit(1, 3, val + 1)); // 0/1/2 -> 1/2/3
      } else if (propertyId == "encoderOutputMode") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Absolute")) return juce::var(1);
        if (str.equalsIgnoreCase("Relative")) return juce::var(2);
        if (str.equalsIgnoreCase("NRPN")) return juce::var(3);
        return juce::var(1);
      } else if (propertyId == "encoderRelativeEncoding") {
        int val = static_cast<int>(propVal);
        return juce::var(juce::jlimit(1, 4, val + 1)); // 0-3 -> 1-4
      } else if (propertyId == "encoderPushOutputType") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("CC")) return juce::var(1);
        if (str.equalsIgnoreCase("Note")) return juce::var(2);
        if (str.equalsIgnoreCase("ProgramChange")) return juce::var(3);
        return juce::var(1);
      } else if (propertyId == "encoderPushMode") {
        int val = static_cast<int>(propVal);
        return juce::var(juce::jlimit(1, 4, val + 1)); // 0-3 -> 1-4
      } else if (propertyId == "encoderPushDetection") {
        int val = static_cast<int>(propVal);
        return juce::var(juce::jlimit(1, 3, val + 1)); // 0-2 -> 1-3
      } else if (propertyId == "pitchPadMode") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Relative")) return juce::var(2);
        return juce::var(1); // Absolute
      } else if (propertyId == "pitchPadStart") {
        juce::String str = propVal.toString().trim();
        if (str.equalsIgnoreCase("Left")) return juce::var(1);
        if (str.equalsIgnoreCase("Center")) return juce::var(2);
        if (str.equalsIgnoreCase("Right")) return juce::var(3);
        if (str.equalsIgnoreCase("Custom")) return juce::var(4);
        return juce::var(2); // Center default
      } else if (propertyId == "smartScaleFollowGlobal") {
        return juce::var(static_cast<bool>(propVal));
      } else if (propertyId == "smartScaleName") {
        return propVal;
      } else if (propertyId == "transposeMode") {
        juce::String modeStr = propVal.toString().trim();
        if (modeStr.isEmpty())
          modeStr = "Global";
        return juce::var(modeStr.equalsIgnoreCase("Local") ? 2 : 1);
      } else if (propertyId == "transposeModify") {
        int modify = static_cast<int>(propVal);
        return juce::var((modify >= 0 && modify <= 4) ? (modify + 1) : 1);
      } else if (propertyId == "slideQuickPrecision" || propertyId == "slideAbsRel" || propertyId == "slideLockFree" || propertyId == "slideAxis") {
        // Convert 0/1 to 1/2 for ComboBox (JUCE uses 1-based IDs)
        int val = static_cast<int>(propVal);
        return juce::var(val == 0 ? 1 : 2);
      }
      return propVal;
    }
    // Return centralized default when property not set.
    juce::var defVal = MappingDefinition::getDefaultValue(propertyId);
    if (!defVal.isVoid()) {
      // ComboBox properties store string in tree; UI needs combo ID.
      if (propertyId == "releaseBehavior") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Send Note Off")) return juce::var(1);
        if (str.equalsIgnoreCase("Sustain until retrigger")) return juce::var(2);
        if (str.equalsIgnoreCase("Always Latch")) return juce::var(3);
        return juce::var(1);
      }
      if (propertyId == "touchpadHoldBehavior") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Hold to not send note off immediately")) return juce::var(1);
        if (str.equalsIgnoreCase("Ignore, send note off immediately")) return juce::var(2);
        return juce::var(1);
      }
      if (propertyId == "adsrTarget") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("CC")) return juce::var(1);
        if (str.equalsIgnoreCase("PitchBend")) return juce::var(2);
        if (str.equalsIgnoreCase("SmartScaleBend")) return juce::var(3);
        return juce::var(1);
      }
      if (propertyId == "expressionCCMode") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Position")) return juce::var(1);
        if (str.equalsIgnoreCase("Slide")) return juce::var(2);
        if (str.equalsIgnoreCase("Encoder")) return juce::var(3);
        return juce::var(1);
      }
      if (propertyId == "encoderAxis") {
        int val = static_cast<int>(defVal);
        return juce::var(juce::jlimit(1, 3, val + 1));
      }
      if (propertyId == "encoderOutputMode") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Absolute")) return juce::var(1);
        if (str.equalsIgnoreCase("Relative")) return juce::var(2);
        if (str.equalsIgnoreCase("NRPN")) return juce::var(3);
        return juce::var(1);
      }
      if (propertyId == "encoderRelativeEncoding") {
        int val = static_cast<int>(defVal);
        return juce::var(juce::jlimit(1, 4, val + 1));
      }
      if (propertyId == "encoderPushOutputType") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("CC")) return juce::var(1);
        if (str.equalsIgnoreCase("Note")) return juce::var(2);
        if (str.equalsIgnoreCase("ProgramChange")) return juce::var(3);
        return juce::var(1);
      }
      if (propertyId == "encoderPushMode") {
        int val = static_cast<int>(defVal);
        return juce::var(juce::jlimit(1, 4, val + 1));
      }
      if (propertyId == "encoderPushDetection") {
        int val = static_cast<int>(defVal);
        return juce::var(juce::jlimit(1, 3, val + 1));
      }
      if (propertyId == "pitchPadMode") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Relative")) return juce::var(2);
        return juce::var(1); // Absolute
      }
      if (propertyId == "pitchPadStart") {
        juce::String str = defVal.toString().trim();
        if (str.equalsIgnoreCase("Left")) return juce::var(1);
        if (str.equalsIgnoreCase("Center")) return juce::var(2);
        if (str.equalsIgnoreCase("Right")) return juce::var(3);
        if (str.equalsIgnoreCase("Custom")) return juce::var(4);
        return juce::var(2); // Center default
      }
      if (propertyId == "slideQuickPrecision" || propertyId == "slideAbsRel" || propertyId == "slideLockFree" || propertyId == "slideAxis") {
        // Convert 0/1 default to 1/2 for ComboBox
        int val = static_cast<int>(defVal);
        return juce::var(val == 0 ? 1 : 2);
      }
      if (propertyId == "smartScaleFollowGlobal")
        return juce::var(static_cast<bool>(defVal));
      if (propertyId == "smartScaleName")
        return defVal;
      if (propertyId == "transposeMode") {
        juce::String str = defVal.toString().trim();
        return juce::var(str.equalsIgnoreCase("Local") ? 2 : 1);
      }
      if (propertyId == "transposeModify") {
        int modify = static_cast<int>(defVal);
        return juce::var((modify >= 0 && modify <= 4) ? (modify + 1) : 1);
      }
      return defVal;
    }
    return juce::var();
  }

  // Layout-only properties below
  if (!isLayout)
    return juce::var();

  if (propertyId == "name")
    return juce::var(juce::String(currentConfig.name));
  if (propertyId == "numFaders")
    return juce::var(currentConfig.numFaders);
  if (propertyId == "ccStart")
    return juce::var(currentConfig.ccStart);
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
  return juce::var();
}

void TouchpadMixerEditorComponent::applyConfigValue(
    const juce::String &propertyId, const juce::var &value) {
  if (!manager)
    return;

  const bool isLayout = (selectionKind == SelectionKind::Layout);
  const bool isMapping = (selectionKind == SelectionKind::Mapping);

  if (isLayout && selectedLayoutIndex < 0)
    return;
  if (isMapping && selectedMappingIndex < 0)
    return;

  if (propertyId == "type") {
    if (isLayout) {
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
      manager->updateLayout(selectedLayoutIndex, currentConfig);
      rebuildUI();
      return;
    } else if (isMapping && currentMapping.mapping.isValid()) {
      // Mapping type: Note/Expression/Command
      int id = static_cast<int>(value);
      juce::String typeStr;
      switch (id) {
      case 1:
        typeStr = "Note";
        break;
      case 2:
        typeStr = "Expression";
        break;
      case 3:
        typeStr = "Command";
        break;
      default:
        typeStr = "Note";
        break;
      }
      currentMapping.mapping.setProperty("type", typeStr, nullptr);
      manager->updateTouchpadMapping(selectedMappingIndex, currentMapping);
      rebuildUI();
      return;
    }
    return;
  } else if (propertyId == "name") {
    if (isLayout)
      currentConfig.name = value.toString().toStdString();
    else if (isMapping)
      currentMapping.name = value.toString().toStdString();
  } else if (propertyId == "layerId") {
    int v = juce::jlimit(0, 8, static_cast<int>(value) - 1); // combo ID 1..9
    if (isLayout)
      currentConfig.layerId = v;
    else if (isMapping)
      currentMapping.layerId = v;
  } else if (propertyId == "layoutGroupId") {
    int v = juce::jmax(0, static_cast<int>(value));
    if (isLayout)
      currentConfig.layoutGroupId = v;
    else if (isMapping)
      currentMapping.layoutGroupId = v;
  }
  else if (propertyId == "numFaders")
    currentConfig.numFaders = juce::jlimit(1, 32, static_cast<int>(value));
  else if (propertyId == "ccStart")
    currentConfig.ccStart = juce::jlimit(0, 127, static_cast<int>(value));
  else if (propertyId == "midiChannel") {
    int v = juce::jlimit(1, 16, static_cast<int>(value));
    if (isLayout)
      currentConfig.midiChannel = v;
    else if (isMapping)
      currentMapping.midiChannel = v;
  }
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
  else if (propertyId == "regionLeft" || propertyId == "regionTop" ||
           propertyId == "regionRight" || propertyId == "regionBottom") {
    double d = static_cast<double>(value);
    d = juce::jlimit(0.0, 1.0, d);
    float f = static_cast<float>(d);
    if (isLayout) {
      if (propertyId == "regionLeft")
        currentConfig.region.left = f;
      else if (propertyId == "regionTop")
        currentConfig.region.top = f;
      else if (propertyId == "regionRight")
        currentConfig.region.right = f;
      else if (propertyId == "regionBottom")
        currentConfig.region.bottom = f;
    } else if (isMapping) {
      if (propertyId == "regionLeft")
        currentMapping.region.left = f;
      else if (propertyId == "regionTop")
        currentMapping.region.top = f;
      else if (propertyId == "regionRight")
        currentMapping.region.right = f;
      else if (propertyId == "regionBottom")
        currentMapping.region.bottom = f;
    }
  } else if (propertyId == "zIndex") {
    int v = juce::jlimit(-100, 100, static_cast<int>(value));
    if (isLayout)
      currentConfig.zIndex = v;
    else if (isMapping)
      currentMapping.zIndex = v;
  } else if (propertyId == "regionLock") {
    bool v = static_cast<bool>(value);
    if (isLayout)
      currentConfig.regionLock = v;
    else if (isMapping)
      currentMapping.regionLock = v;
  }
  else {
    // Mapping-specific properties: write to mapping ValueTree.
    if (isMapping && currentMapping.mapping.isValid()) {
      // Convert ComboBox ID to string for string properties
      juce::var valueToSet = value;
      if (propertyId == "releaseBehavior") {
        int id = static_cast<int>(value);
        if (id == 1)
          valueToSet = juce::var("Send Note Off");
        else if (id == 2)
          valueToSet = juce::var("Sustain until retrigger");
        else if (id == 3)
          valueToSet = juce::var("Always Latch");
      } else if (propertyId == "touchpadHoldBehavior") {
        int id = static_cast<int>(value);
        if (id == 1)
          valueToSet = juce::var("Hold to not send note off immediately");
        else if (id == 2)
          valueToSet = juce::var("Ignore, send note off immediately");
      } else if (propertyId == "adsrTarget") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        if (id == 1)
          valueToSet = juce::var("CC");
        else if (id == 2)
          valueToSet = juce::var("PitchBend");
        else
          valueToSet = juce::var("SmartScaleBend");
      } else if (propertyId == "expressionCCMode") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        if (id == 1)
          valueToSet = juce::var("Position");
        else if (id == 2)
          valueToSet = juce::var("Slide");
        else
          valueToSet = juce::var("Encoder");
      } else if (propertyId == "encoderAxis") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        valueToSet = juce::var(id - 1); // 1/2/3 -> 0/1/2
      } else if (propertyId == "encoderOutputMode") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        if (id == 1) valueToSet = juce::var("Absolute");
        else if (id == 2) valueToSet = juce::var("Relative");
        else valueToSet = juce::var("NRPN");
      } else if (propertyId == "encoderRelativeEncoding") {
        int id = juce::jlimit(1, 4, static_cast<int>(value));
        valueToSet = juce::var(id - 1);
      } else if (propertyId == "encoderPushOutputType") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        if (id == 1) valueToSet = juce::var("CC");
        else if (id == 2) valueToSet = juce::var("Note");
        else valueToSet = juce::var("ProgramChange");
      } else if (propertyId == "encoderPushMode") {
        int id = juce::jlimit(1, 4, static_cast<int>(value));
        valueToSet = juce::var(id - 1);
      } else if (propertyId == "encoderPushDetection") {
        int id = juce::jlimit(1, 3, static_cast<int>(value));
        valueToSet = juce::var(id - 1);
      } else if (propertyId == "pitchPadMode") {
        int id = juce::jlimit(1, 2, static_cast<int>(value));
        valueToSet = juce::var(id == 1 ? "Absolute" : "Relative");
      } else if (propertyId == "smartScaleFollowGlobal") {
        valueToSet = juce::var(static_cast<bool>(value));
      } else if (propertyId == "smartScaleName") {
        valueToSet = value;
      } else if (propertyId == "pitchPadStart") {
        int id = juce::jlimit(1, 4, static_cast<int>(value));
        if (id == 1) valueToSet = juce::var("Left");
        else if (id == 2) valueToSet = juce::var("Center");
        else if (id == 3) valueToSet = juce::var("Right");
        else valueToSet = juce::var("Custom");
      } else if (propertyId == "slideQuickPrecision" || propertyId == "slideAbsRel" || propertyId == "slideLockFree" || propertyId == "slideAxis") {
        // Convert ComboBox ID (1/2) back to stored value (0/1)
        int id = static_cast<int>(value);
        valueToSet = juce::var(id == 1 ? 0 : 1);
      }
      currentMapping.mapping.setProperty(propertyId, valueToSet, nullptr);
      manager->updateTouchpadMapping(selectedMappingIndex, currentMapping);
      // Rebuild UI when schema structure changes (controls show/hide).
      // pitchPadTouchGlideMs omitted: changing it doesn't show/hide controls, and
      // rebuilding on every slider tick destroyed the slider and caused crashes.
      if (propertyId == "type" || propertyId == "useCustomEnvelope" ||
          propertyId == "adsrTarget" || propertyId == "expressionCCMode" ||
          propertyId == "encoderAxis" || propertyId == "encoderOutputMode" ||
          propertyId == "encoderPushMode" || propertyId == "encoderPushOutputType" ||
          propertyId == "pitchPadStart" || propertyId == "pitchPadMode" ||
          propertyId == "pitchPadUseCustomRange" ||
          propertyId == "smartScaleFollowGlobal" ||
          propertyId == "data1" || propertyId == "commandCategory" ||
          propertyId == "sustainStyle" || propertyId == "panicMode" ||
          propertyId == "layerStyle" || propertyId == "transposeMode" ||
          propertyId == "transposeModify")
        rebuildUI();
      return;
    }
    return;
  }

  if (isLayout)
    manager->updateLayout(selectedLayoutIndex, currentConfig);
  else if (isMapping)
    manager->updateTouchpadMapping(selectedMappingIndex, currentMapping);
}

void TouchpadMixerEditorComponent::onRelayoutRegionChosen(float x1, float y1,
                                                         float x2, float y2) {
  if (!manager)
    return;
  if (selectionKind == SelectionKind::Layout && selectedLayoutIndex >= 0) {
    currentConfig.region.left = x1;
    currentConfig.region.top = y1;
    currentConfig.region.right = x2;
    currentConfig.region.bottom = y2;
    manager->updateLayout(selectedLayoutIndex, currentConfig);
  } else if (selectionKind == SelectionKind::Mapping &&
             selectedMappingIndex >= 0) {
    currentMapping.region.left = x1;
    currentMapping.region.top = y1;
    currentMapping.region.right = x2;
    currentMapping.region.bottom = y2;
    manager->updateTouchpadMapping(selectedMappingIndex, currentMapping);
  }
  rebuildUI();
}

void TouchpadMixerEditorComponent::launchRelayoutDialog() {
  if (!manager ||
      (selectionKind == SelectionKind::Layout && selectedLayoutIndex < 0) ||
      (selectionKind == SelectionKind::Mapping && selectedMappingIndex < 0))
    return;
  using Cb = TouchpadRelayoutDialog::RegionChosenCallback;
  Cb onChosen(std::bind(&TouchpadMixerEditorComponent::onRelayoutRegionChosen,
                        this, std::placeholders::_1, std::placeholders::_2,
                        std::placeholders::_3, std::placeholders::_4));
  auto *dialog = new TouchpadRelayoutDialog(std::move(onChosen));
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
    } else if (propId == "smartScaleName" && scaleLibrary) {
      juce::StringArray names = scaleLibrary->getScaleNames();
      for (int i = 0; i < names.size(); ++i)
        cb->addItem(names[i], i + 1);
      juce::String currentName = currentVal.toString().trim();
      for (int i = 0; i < names.size(); ++i) {
        if (names[i] == currentName) {
          cb->setSelectedId(i + 1, juce::dontSendNotification);
          break;
        }
      }
      if (cb->getSelectedId() == 0 && names.size() > 0)
        cb->setSelectedId(1, juce::dontSendNotification);
    } else {
      for (const auto &p : def.options)
        cb->addItem(p.second, p.first);
    }
    if (propId == "smartScaleName")
      cb->setEnabled(def.isEnabled);
    else if (propId != "type")
      ; // Both Mixer and Drum Pad are enabled
    int id = static_cast<int>(currentVal);
    if (propId == "layoutGroupId") {
      cb->setSelectedId(id + 1, juce::dontSendNotification);
    } else if (propId == "smartScaleName") {
      // Selection already set above
    } else if (propId == "data1" && def.options.count(5) == 0) {
      // Command combo: Panic Latch (5) -> 4, GlobalPitchDown (7) -> 6
      int data1 = static_cast<int>(currentVal);
      int display = data1;
      if (data1 == 5)
        display = 4;
      else if (data1 == 7)
        display = 6;
      cb->setSelectedId(display, juce::dontSendNotification);
    } else if (id > 0) {
      cb->setSelectedId(id, juce::dontSendNotification);
    } else if (!def.options.empty()) {
      // If id is 0 or invalid, select first available option (shouldn't happen after conversion, but safety check)
      int firstId = def.options.begin()->first;
      if (firstId > 0)
        cb->setSelectedId(firstId, juce::dontSendNotification);
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
    const bool useMappingInspectorLogic =
        (propId == "commandCategory" || propId == "sustainStyle" ||
         propId == "panicMode" || propId == "layerStyle" ||
         propId == "transposeMode" || propId == "transposeModify" ||
         propId == "globalModeDirection" || propId == "globalRootMode" ||
         propId == "globalScaleMode");
    cb->onChange = [this, propId, cbPtr, useMappingInspectorLogic, def]() {
      if (propId == "smartScaleName") {
        applyConfigValue(propId, juce::var(cbPtr->getText()));
        return;
      }
      int sel = cbPtr->getSelectedId();
      if (propId == "layoutGroupId") {
        applyConfigValue(propId, juce::var(sel - 1)); // map back to groupId
        return;
      }
      if (useMappingInspectorLogic && selectionKind == SelectionKind::Mapping &&
          currentMapping.mapping.isValid() && manager) {
        MappingInspectorLogic::applyComboSelectionToMapping(
            currentMapping.mapping, def, sel, nullptr);
        manager->updateTouchpadMapping(selectedMappingIndex, currentMapping);
        rebuildUI();
        return;
      }
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
    btn->setEnabled(def.isEnabled);
    juce::TextButton *btnPtr = btn.get();
    btnPtr->onClick = [this, propId]() {
      if (propId == "relayoutRegion" && manager &&
          ((selectionKind == SelectionKind::Layout && selectedLayoutIndex >= 0) ||
           (selectionKind == SelectionKind::Mapping && selectedMappingIndex >= 0)))
        launchRelayoutDialog();
      else if (propId == "smartScaleEdit" && scaleLibrary) {
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

  if (selectionKind == SelectionKind::None) {
    resized();
    return;
  }

  InspectorSchema schema;
  if (selectionKind == SelectionKind::Layout) {
    schema = TouchpadMixerDefinition::getSchema(currentConfig.type);
  } else {
    // For mappings: common header (name, layerId, layoutGroupId, midiChannel, zIndex)
    // + mapping body schema (type Note/Expression/Command, touchpad event, etc.)
    // + common region controls.
    InspectorSchema commonHeader = TouchpadMixerDefinition::getCommonLayoutHeader();
    // Remove "type" from common header (layout types) - we'll use mapping type instead.
    commonHeader.erase(
        std::remove_if(commonHeader.begin(), commonHeader.end(),
                       [](const InspectorControl &c) { return c.propertyId == "type"; }),
        commonHeader.end());
    for (const auto &c : commonHeader)
      schema.push_back(c);

    // Enabled in header (all touchpad mapping types)
    InspectorControl enabledCtrl;
    enabledCtrl.propertyId = "enabled";
    enabledCtrl.label = "Enabled";
    enabledCtrl.controlType = InspectorControl::Type::Toggle;
    enabledCtrl.widthWeight = 0.5f;
    schema.push_back(enabledCtrl);

    // Get mapping body schema (includes "type" with Note/Expression/Command).
    if (currentMapping.mapping.isValid() && settingsManager) {
      int pbRange = settingsManager->getPitchBendRange();
      InspectorSchema mappingSchema = MappingDefinition::getSchema(
          currentMapping.mapping, pbRange, true /* forTouchpadEditor */);
      // Add separator before mapping body.
      schema.push_back(MappingDefinition::createSeparator(
          "Mapping", juce::Justification::centredLeft));
      for (const auto &c : mappingSchema)
        schema.push_back(c);
    }

    // Add common region controls at the end.
    for (const auto &c : TouchpadMixerDefinition::getCommonLayoutControls())
      schema.push_back(c);
  }
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
  if (onContentHeightMaybeChanged)
    onContentHeightMaybeChanged();
}

void TouchpadMixerEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
  if (selectedLayoutIndex < 0 && selectedMappingIndex < 0) {
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
