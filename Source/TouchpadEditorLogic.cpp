#include "TouchpadEditorLogic.h"
#include "MappingDefinition.h"

namespace TouchpadEditorLogic {

bool applyConfig(TouchpadLayoutConfig *layoutCfg,
                 TouchpadMappingConfig *mappingCfg,
                 const juce::String &propertyId, const juce::var &value) {
  bool changed = false;
  const bool isLayout = (layoutCfg != nullptr);
  const bool hasMappingConfig = (mappingCfg != nullptr);
  const bool hasMappingTree =
      (mappingCfg != nullptr && mappingCfg->mapping.isValid());

  // Shared header: name / layer / group / channel
  if (propertyId == "name") {
    if (isLayout) {
      std::string newName = value.toString().toStdString();
      if (layoutCfg->name != newName) {
        layoutCfg->name = std::move(newName);
        changed = true;
      }
    } else if (hasMappingConfig) {
      std::string newName = value.toString().toStdString();
      if (mappingCfg->name != newName) {
        mappingCfg->name = std::move(newName);
        changed = true;
      }
    }
    return changed;
  }

  if (propertyId == "layerId") {
    int v = juce::jlimit(0, 8, static_cast<int>(value) - 1); // combo ID 1..9
    if (isLayout) {
      if (layoutCfg->layerId != v) {
        layoutCfg->layerId = v;
        changed = true;
      }
    } else if (hasMappingConfig) {
      if (mappingCfg->layerId != v) {
        mappingCfg->layerId = v;
        changed = true;
      }
    }
    return changed;
  }

  if (propertyId == "layoutGroupId") {
    int v = juce::jmax(0, static_cast<int>(value));
    if (isLayout) {
      if (layoutCfg->layoutGroupId != v) {
        layoutCfg->layoutGroupId = v;
        changed = true;
      }
    } else if (hasMappingConfig) {
      if (mappingCfg->layoutGroupId != v) {
        mappingCfg->layoutGroupId = v;
        changed = true;
      }
    }
    return changed;
  }

  if (propertyId == "midiChannel") {
    int v = juce::jlimit(1, 16, static_cast<int>(value));
    if (isLayout) {
      if (layoutCfg->midiChannel != v) {
        layoutCfg->midiChannel = v;
        changed = true;
      }
    } else if (hasMappingConfig) {
      if (mappingCfg->midiChannel != v) {
        mappingCfg->midiChannel = v;
        changed = true;
      }
    }
    return changed;
  }

  // Layout-only properties
  if (isLayout) {
    if (propertyId == "numFaders") {
      int v = juce::jlimit(1, 32, static_cast<int>(value));
      if (layoutCfg->numFaders != v) {
        layoutCfg->numFaders = v;
        return true;
      }
      return false;
    }
    if (propertyId == "ccStart") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->ccStart != v) {
        layoutCfg->ccStart = v;
        return true;
      }
      return false;
    }
    if (propertyId == "inputMin") {
      float v = static_cast<float>(static_cast<double>(value));
      if (layoutCfg->inputMin != v) {
        layoutCfg->inputMin = v;
        return true;
      }
      return false;
    }
    if (propertyId == "inputMax") {
      float v = static_cast<float>(static_cast<double>(value));
      if (layoutCfg->inputMax != v) {
        layoutCfg->inputMax = v;
        return true;
      }
      return false;
    }
    if (propertyId == "outputMin") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->outputMin != v) {
        layoutCfg->outputMin = v;
        return true;
      }
      return false;
    }
    if (propertyId == "outputMax") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->outputMax != v) {
        layoutCfg->outputMax = v;
        return true;
      }
      return false;
    }
    if (propertyId == "quickPrecision") {
      layoutCfg->quickPrecision =
          (static_cast<int>(value) == 2)
              ? TouchpadMixerQuickPrecision::Precision
              : TouchpadMixerQuickPrecision::Quick;
      return true;
    }
    if (propertyId == "absRel") {
      layoutCfg->absRel =
          (static_cast<int>(value) == 2) ? TouchpadMixerAbsRel::Relative
                                         : TouchpadMixerAbsRel::Absolute;
      return true;
    }
    if (propertyId == "lockFree") {
      layoutCfg->lockFree =
          (static_cast<int>(value) == 2) ? TouchpadMixerLockFree::Free
                                         : TouchpadMixerLockFree::Lock;
      return true;
    }
    if (propertyId == "muteButtonsEnabled") {
      bool v = static_cast<bool>(value);
      if (layoutCfg->muteButtonsEnabled != v) {
        layoutCfg->muteButtonsEnabled = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadRows") {
      int v = juce::jlimit(1, 8, static_cast<int>(value));
      if (layoutCfg->drumPadRows != v) {
        layoutCfg->drumPadRows = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadColumns") {
      int v = juce::jlimit(1, 16, static_cast<int>(value));
      if (layoutCfg->drumPadColumns != v) {
        layoutCfg->drumPadColumns = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadMidiNoteStart") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->drumPadMidiNoteStart != v) {
        layoutCfg->drumPadMidiNoteStart = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadBaseVelocity") {
      int v = juce::jlimit(1, 127, static_cast<int>(value));
      if (layoutCfg->drumPadBaseVelocity != v) {
        layoutCfg->drumPadBaseVelocity = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadVelocityRandom") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->drumPadVelocityRandom != v) {
        layoutCfg->drumPadVelocityRandom = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumPadLayoutMode") {
      int id = static_cast<int>(value);
      layoutCfg->drumPadLayoutMode =
          (id == 2) ? DrumPadLayoutMode::HarmonicGrid
                    : DrumPadLayoutMode::Classic;
      return true;
    }
    if (propertyId == "harmonicRowInterval") {
      int v = juce::jlimit(-12, 12, static_cast<int>(value));
      if (layoutCfg->harmonicRowInterval != v) {
        layoutCfg->harmonicRowInterval = v;
        return true;
      }
      return false;
    }
    if (propertyId == "harmonicUseScaleFilter") {
      bool v = static_cast<bool>(value);
      if (layoutCfg->harmonicUseScaleFilter != v) {
        layoutCfg->harmonicUseScaleFilter = v;
        return true;
      }
      return false;
    }
    if (propertyId == "chordPadPreset") {
      int v = juce::jmax(0, static_cast<int>(value));
      if (layoutCfg->chordPadPreset != v) {
        layoutCfg->chordPadPreset = v;
        return true;
      }
      return false;
    }
    if (propertyId == "chordPadLatchMode") {
      bool v = static_cast<bool>(value);
      if (layoutCfg->chordPadLatchMode != v) {
        layoutCfg->chordPadLatchMode = v;
        return true;
      }
      return false;
    }
    if (propertyId == "drumFxSplitSplitRow") {
      int v = juce::jlimit(0, 8, static_cast<int>(value));
      if (layoutCfg->drumFxSplitSplitRow != v) {
        layoutCfg->drumFxSplitSplitRow = v;
        return true;
      }
      return false;
    }
    if (propertyId == "fxCcStart") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->fxCcStart != v) {
        layoutCfg->fxCcStart = v;
        return true;
      }
      return false;
    }
    if (propertyId == "fxOutputMin") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->fxOutputMin != v) {
        layoutCfg->fxOutputMin = v;
        return true;
      }
      return false;
    }
    if (propertyId == "fxOutputMax") {
      int v = juce::jlimit(0, 127, static_cast<int>(value));
      if (layoutCfg->fxOutputMax != v) {
        layoutCfg->fxOutputMax = v;
        return true;
      }
      return false;
    }
    if (propertyId == "fxToggleMode") {
      bool v = static_cast<bool>(value);
      if (layoutCfg->fxToggleMode != v) {
        layoutCfg->fxToggleMode = v;
        return true;
      }
      return false;
    }
  }

  // Region / zIndex / regionLock (shared layout + mapping)
  if (propertyId == "regionLeft" || propertyId == "regionTop" ||
      propertyId == "regionRight" || propertyId == "regionBottom") {
    double d = static_cast<double>(value);
    d = juce::jlimit(0.0, 1.0, d);
    float f = static_cast<float>(d);
    if (isLayout) {
      if (propertyId == "regionLeft" && layoutCfg->region.left != f) {
        layoutCfg->region.left = f;
        changed = true;
      } else if (propertyId == "regionTop" && layoutCfg->region.top != f) {
        layoutCfg->region.top = f;
        changed = true;
      } else if (propertyId == "regionRight" && layoutCfg->region.right != f) {
        layoutCfg->region.right = f;
        changed = true;
      } else if (propertyId == "regionBottom" &&
                 layoutCfg->region.bottom != f) {
        layoutCfg->region.bottom = f;
        changed = true;
      }
    }
    if (hasMappingConfig) {
      if (propertyId == "regionLeft" && mappingCfg->region.left != f) {
        mappingCfg->region.left = f;
        changed = true;
      } else if (propertyId == "regionTop" && mappingCfg->region.top != f) {
        mappingCfg->region.top = f;
        changed = true;
      } else if (propertyId == "regionRight" && mappingCfg->region.right != f) {
        mappingCfg->region.right = f;
        changed = true;
      } else if (propertyId == "regionBottom" &&
                 mappingCfg->region.bottom != f) {
        mappingCfg->region.bottom = f;
        changed = true;
      }
    }
    return changed;
  }

  if (propertyId == "zIndex") {
    int v = juce::jlimit(-100, 100, static_cast<int>(value));
    if (isLayout && layoutCfg->zIndex != v) {
      layoutCfg->zIndex = v;
      changed = true;
    }
    if (hasMappingConfig && mappingCfg->zIndex != v) {
      mappingCfg->zIndex = v;
      changed = true;
    }
    return changed;
  }

  if (propertyId == "regionLock") {
    bool v = static_cast<bool>(value);
    if (isLayout && layoutCfg->regionLock != v) {
      layoutCfg->regionLock = v;
      changed = true;
    }
    if (hasMappingConfig && mappingCfg->regionLock != v) {
      mappingCfg->regionLock = v;
      changed = true;
    }
    return changed;
  }

  // Mapping-only special case: pitchPadAxis maps to inputTouchpadEvent.
  if (propertyId == "pitchPadAxis" && hasMappingTree) {
    int id = juce::jlimit(1, 2, static_cast<int>(value));
    int eventVal =
        (id == 2) ? TouchpadEvent::Finger1Y : TouchpadEvent::Finger1X;
    mappingCfg->mapping.setProperty("inputTouchpadEvent", eventVal, nullptr);
    return true;
  }

  return changed;
}

bool applyMappingValueProperty(juce::ValueTree &mapping,
                               const juce::String &propertyId,
                               const juce::var &value) {
  if (!mapping.isValid())
    return false;

  juce::var valueToSet = value;

  if (propertyId == "releaseBehavior") {
    int id = static_cast<int>(value);
    if (id == 1)
      valueToSet = juce::var("Send Note Off");
    else if (id == 2)
      valueToSet = juce::var("Sustain until retrigger");
    else if (id == 3)
      valueToSet = juce::var("Always Latch");
  } else if (propertyId == "ccReleaseBehavior") {
    int id = static_cast<int>(value);
    if (id == 1)
      valueToSet = juce::var("Send release (instant)");
    else if (id == 2)
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
    if (id == 1)
      valueToSet = juce::var("Absolute");
    else if (id == 2)
      valueToSet = juce::var("Relative");
    else
      valueToSet = juce::var("NRPN");
  } else if (propertyId == "encoderRelativeEncoding") {
    int id = juce::jlimit(1, 4, static_cast<int>(value));
    valueToSet = juce::var(id - 1);
  } else if (propertyId == "encoderPushOutputType") {
    int id = juce::jlimit(1, 3, static_cast<int>(value));
    if (id == 1)
      valueToSet = juce::var("CC");
    else if (id == 2)
      valueToSet = juce::var("Note");
    else
      valueToSet = juce::var("ProgramChange");
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
    if (id == 1)
      valueToSet = juce::var("Left");
    else if (id == 2)
      valueToSet = juce::var("Center");
    else if (id == 3)
      valueToSet = juce::var("Right");
    else
      valueToSet = juce::var("Custom");
  } else if (propertyId == "slideQuickPrecision" ||
             propertyId == "slideAbsRel" || propertyId == "slideLockFree" ||
             propertyId == "slideAxis") {
    int id = static_cast<int>(value);
    if (propertyId == "slideAxis") {
      // 1/2/3 -> 0/1/2
      valueToSet = juce::var(juce::jlimit(1, 3, id) - 1);
    } else {
      // Quick/Precision, Abs/Rel, Lock/Free: 1/2 -> 0/1
      valueToSet = juce::var(id == 1 ? 0 : 1);
    }
  }

  mapping.setProperty(propertyId, valueToSet, nullptr);
  return true;
}

} // namespace TouchpadEditorLogic

