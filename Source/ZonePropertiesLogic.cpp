#include "ZonePropertiesLogic.h"
#include "ChordUtilities.h"

namespace ZonePropertiesLogic {

static bool applySlider(Zone *zone, const juce::String &key, const juce::var &v) {
  if (!v.isDouble() && !v.isInt() && !v.isInt64())
    return false;
  int val = static_cast<int>(v);
  double dval = static_cast<double>(v);

  if (key == "rootNote") {
    zone->rootNote = val;
    return true;
  }
  if (key == "chromaticOffset") {
    zone->chromaticOffset = val;
    return true;
  }
  if (key == "degreeOffset") {
    zone->degreeOffset = val;
    return true;
  }
  if (key == "globalRootOctaveOffset") {
    zone->globalRootOctaveOffset = val;
    return true;
  }
  if (key == "baseVelocity") {
    zone->baseVelocity = val;
    return true;
  }
  if (key == "velocityRandom") {
    zone->velocityRandom = val;
    return true;
  }
  if (key == "ghostVelocityScale") {
    zone->ghostVelocityScale = static_cast<float>(dval);
    return true;
  }
  if (key == "glideTimeMs") {
    zone->glideTimeMs = val;
    return true;
  }
  if (key == "maxGlideTimeMs") {
    zone->maxGlideTimeMs = val;
    return true;
  }
  if (key == "midiChannel") {
    zone->midiChannel = val;
    return true;
  }
  if (key == "guitarFretAnchor") {
    zone->guitarFretAnchor = val;
    return true;
  }
  if (key == "strumSpeedMs") {
    zone->strumSpeedMs = val;
    return true;
  }
  if (key == "voicingMagnetSemitones") {
    zone->voicingMagnetSemitones = val;
    return true;
  }
  if (key == "releaseDurationMs") {
    zone->releaseDurationMs = val;
    return true;
  }
  if (key == "gridInterval") {
    zone->gridInterval = val;
    return true;
  }
  if (key == "strumTimingVariationMs") {
    zone->strumTimingVariationMs = val;
    return true;
  }
  if (key == "bassOctaveOffset") {
    zone->bassOctaveOffset = val;
    return true;
  }
  return false;
}

static bool applyCombo(Zone *zone, const juce::String &key, const juce::var &v) {
  if (!v.isInt() && !v.isInt64())
    return false;
  int id = static_cast<int>(v);

  if (key == "showRomanNumerals") {
    zone->showRomanNumerals = (id == 2);
    return true;
  }
  if (key == "polyphonyMode") {
    zone->polyphonyMode = (id == 1)   ? PolyphonyMode::Poly
                          : (id == 2) ? PolyphonyMode::Mono
                                      : PolyphonyMode::Legato;
    return true;
  }
  if (key == "instrumentMode") {
    zone->instrumentMode = (id == 1) ? Zone::InstrumentMode::Piano
                                     : Zone::InstrumentMode::Guitar;
    return true;
  }
  if (key == "pianoVoicingStyle") {
    zone->pianoVoicingStyle = (id == 1)   ? Zone::PianoVoicingStyle::Block
                              : (id == 2) ? Zone::PianoVoicingStyle::Close
                                          : Zone::PianoVoicingStyle::Open;
    return true;
  }
  if (key == "guitarPlayerPosition") {
    zone->guitarPlayerPosition =
        (id == 1) ? Zone::GuitarPlayerPosition::Campfire
                  : Zone::GuitarPlayerPosition::Rhythm;
    return true;
  }
  if (key == "strumPattern") {
    zone->strumPattern = (id == 1)   ? Zone::StrumPattern::Down
                         : (id == 2) ? Zone::StrumPattern::Up
                                     : Zone::StrumPattern::AutoAlternating;
    return true;
  }
  if (key == "chordType") {
    zone->chordType = (id == 1)   ? ChordUtilities::ChordType::None
                      : (id == 2) ? ChordUtilities::ChordType::Triad
                      : (id == 3) ? ChordUtilities::ChordType::Seventh
                      : (id == 4) ? ChordUtilities::ChordType::Ninth
                                 : ChordUtilities::ChordType::Power5;
    return true;
  }
  if (key == "playMode") {
    zone->playMode =
        (id == 1) ? Zone::PlayMode::Direct : Zone::PlayMode::Strum;
    return true;
  }
  if (key == "releaseBehavior") {
    zone->releaseBehavior = (id == 1) ? Zone::ReleaseBehavior::Normal
                                     : Zone::ReleaseBehavior::Sustain;
    return true;
  }
  if (key == "layoutStrategy") {
    zone->layoutStrategy = (id == 1)   ? Zone::LayoutStrategy::Linear
                           : (id == 2) ? Zone::LayoutStrategy::Grid
                                       : Zone::LayoutStrategy::Piano;
    return true;
  }
  return false;
}

static bool applyToggle(Zone *zone, const juce::String &key, const juce::var &v) {
  if (!v.isBool())
    return false;
  bool b = static_cast<bool>(v);

  if (key == "useGlobalRoot") {
    zone->useGlobalRoot = b;
    return true;
  }
  if (key == "useGlobalScale") {
    zone->useGlobalScale = b;
    return true;
  }
  if (key == "ignoreGlobalTranspose") {
    zone->ignoreGlobalTranspose = b;
    return true;
  }
  if (key == "ignoreGlobalSustain") {
    zone->ignoreGlobalSustain = b;
    return true;
  }
  if (key == "strictGhostHarmony") {
    zone->strictGhostHarmony = b;
    return true;
  }
  if (key == "isAdaptiveGlide") {
    zone->isAdaptiveGlide = b;
    return true;
  }
  if (key == "strumGhostNotes") {
    zone->strumGhostNotes = b;
    return true;
  }
  if (key == "overrideTimer") {
    zone->overrideTimer = b;
    return true;
  }
  if (key == "strumTimingVariationOn") {
    zone->strumTimingVariationOn = b;
    return true;
  }
  if (key == "addBassNote") {
    zone->addBassNote = b;
    return true;
  }
  if (key == "delayReleaseOn") {
    zone->delayReleaseOn = b;
    return true;
  }
  return false;
}

bool setZonePropertyFromKey(Zone *zone, const juce::String &propertyKey,
                             const juce::var &value) {
  if (!zone)
    return false;
  if (applySlider(zone, propertyKey, value))
    return true;
  if (applyCombo(zone, propertyKey, value))
    return true;
  if (applyToggle(zone, propertyKey, value))
    return true;
  return false;
}

} // namespace ZonePropertiesLogic
