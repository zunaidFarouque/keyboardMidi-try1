#include "ZoneDefinition.h"
#include "ChordUtilities.h"
#include "MappingTypes.h"

ZoneControl ZoneDefinition::createSeparator(const juce::String &label,
                                            juce::Justification align) {
  ZoneControl c;
  c.controlType = ZoneControl::Type::Separator;
  c.label = label;
  c.separatorAlign = align;
  return c;
}

static bool pianoOnly(const Zone *z) {
  return z->instrumentMode == Zone::InstrumentMode::Piano;
}
static bool guitarOnly(const Zone *z) {
  return z->instrumentMode == Zone::InstrumentMode::Guitar;
}
static bool guitarRhythmOnly(const Zone *z) {
  return z->instrumentMode == Zone::InstrumentMode::Guitar &&
         z->guitarPlayerPosition == Zone::GuitarPlayerPosition::Rhythm;
}
static bool legatoOnly(const Zone *z) {
  return z->polyphonyMode == PolyphonyMode::Legato;
}
static bool legatoAdaptiveOnly(const Zone *z) {
  return z->polyphonyMode == PolyphonyMode::Legato && z->isAdaptiveGlide;
}
static bool monoOrLegatoOnly(const Zone *z) {
  return z->polyphonyMode == PolyphonyMode::Mono ||
         z->polyphonyMode == PolyphonyMode::Legato;
}
static bool polyOnly(const Zone *z) {
  return z->polyphonyMode == PolyphonyMode::Poly;
}
static bool chordOn(const Zone *z) {
  return z->chordType != ChordUtilities::ChordType::None;
}
static bool polyAndChordOn(const Zone *z) { return polyOnly(z) && chordOn(z); }
static bool polyAndChordOnPiano(const Zone *z) {
  return polyAndChordOn(z) && pianoOnly(z);
}
static bool pianoCloseOrOpenOnly(const Zone *z) {
  return polyAndChordOnPiano(z) &&
         z->pianoVoicingStyle != Zone::PianoVoicingStyle::Block;
}
static bool polyAndChordOnGuitar(const Zone *z) {
  return polyAndChordOn(z) && guitarOnly(z);
}
static bool polyAndChordOnGuitarRhythm(const Zone *z) {
  return polyAndChordOn(z) && guitarRhythmOnly(z);
}
static bool globalRootOnly(const Zone *z) { return z->useGlobalRoot; }
static bool gridLayoutOnly(const Zone *z) {
  return z->layoutStrategy == Zone::LayoutStrategy::Grid;
}
static bool pianoLayoutOnly(const Zone *z) {
  return z->layoutStrategy == Zone::LayoutStrategy::Piano;
}
static bool strumOnly(const Zone *z) {
  return polyAndChordOn(z) && z->playMode == Zone::PlayMode::Strum;
}
static bool releaseNormalOnly(const Zone *z) {
  return polyAndChordOn(z) &&
         z->releaseBehavior == Zone::ReleaseBehavior::Normal;
}
static bool releaseSustainOnly(const Zone *z) {
  return polyAndChordOn(z) &&
         z->releaseBehavior == Zone::ReleaseBehavior::Sustain;
}

ZoneSchema ZoneDefinition::getSchema(const Zone *zone) {
  ZoneSchema schema;

  // --- Identity (no header) ---
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomAlias;
    c.label = "Device Alias";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomLayer;
    c.label = "Layer";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomName;
    c.label = "Zone Name";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "MIDI Channel";
    c.propertyKey = "midiChannel";
    c.min = 1;
    c.max = 16;
    c.step = 1;
    schema.push_back(c);
  }

  // --- Tuning and velocity ---
  schema.push_back(
      createSeparator("Tuning and velocity", juce::Justification::centredLeft));
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomScale;
    c.label = "Scale";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Root Note";
    c.propertyKey = "rootNote";
    c.min = 0;
    c.max = 127;
    c.step = 1;
    c.affectsCache = true;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Global Root";
    c.propertyKey = "useGlobalRoot";
    c.affectsCache = true;
    c.sameLine = true;
    c.widthWeight = 0.2f;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Octave Offset";
    c.propertyKey = "globalRootOctaveOffset";
    c.min = -2;
    c.max = 2;
    c.step = 1;
    c.affectsCache = true;
    c.visible = globalRootOnly;
    schema.push_back(c);
  }
  schema.push_back(createSeparator("", juce::Justification::centred));
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Chromatic Offset";
    c.propertyKey = "chromaticOffset";
    c.min = -12;
    c.max = 12;
    c.step = 1;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Degree Offset";
    c.propertyKey = "degreeOffset";
    c.min = -7;
    c.max = 7;
    c.step = 1;
    c.affectsCache = true;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Ignore global transpose";
    c.propertyKey = "ignoreGlobalTranspose";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Base Velocity";
    c.propertyKey = "baseVelocity";
    c.min = 1;
    c.max = 127;
    c.step = 1;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Velocity Random";
    c.propertyKey = "velocityRandom";
    c.min = 0;
    c.max = 64;
    c.step = 1;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Ignore global sustain";
    c.propertyKey = "ignoreGlobalSustain";
    schema.push_back(c);
  }

  // --- MIDI Output ---
  schema.push_back(
      createSeparator("MIDI Output", juce::Justification::centredLeft));
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Polyphony Mode";
    c.propertyKey = "polyphonyMode";
    c.options[1] = "Poly";
    c.options[2] = "Mono (Retrigger)";
    c.options[3] = "Legato (Glide)";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Glide Time";
    c.propertyKey = "glideTimeMs";
    c.min = 0;
    c.max = 500;
    c.step = 1;
    c.suffix = " ms";
    c.visible = legatoOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Adaptive Glide";
    c.propertyKey = "isAdaptiveGlide";
    c.visible = legatoOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Max Glide Time";
    c.propertyKey = "maxGlideTimeMs";
    c.min = 50;
    c.max = 500;
    c.step = 1;
    c.suffix = " ms";
    c.visible = legatoAdaptiveOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::LabelOnly;
    c.label = "Mono/Legato: one MIDI channel per zone.";
    c.visible = monoOrLegatoOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Chord Type";
    c.propertyKey = "chordType";
    c.options[1] = "Off";
    c.options[2] = "Triad";
    c.options[3] = "Seventh";
    c.options[4] = "Ninth";
    c.options[5] = "Power5";
    c.affectsCache = true;
    c.visible = polyOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Play Mode";
    c.propertyKey = "playMode";
    c.options[1] = "Direct";
    c.options[2] = "Strum Buffer";
    c.visible = polyAndChordOn;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Strum Speed";
    c.propertyKey = "strumSpeedMs";
    c.min = 0;
    c.max = 500;
    c.step = 1;
    c.suffix = " ms";
    c.visible = strumOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::StrumTimingVariation;
    c.label = "Strumming timing variation";
    c.min = 0;
    c.max = 100;
    c.step = 1;
    c.suffix = " ms";
    c.visible = strumOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Release Behavior";
    c.propertyKey = "releaseBehavior";
    c.options[1] = "Normal";
    c.options[2] = "Sustain";
    c.visible = polyAndChordOn;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::LabelOnlyWrappable;
    c.label =
        "Sustain behaves like a latch: notes stay on until you play another "
        "chord. To clear without playing, map a key to Command, Panic, Panic "
        "chords.";
    c.visible = releaseSustainOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::DelayRelease;
    c.label = "Delay release";
    c.min = 0;
    c.max = 5000;
    c.step = 1;
    c.suffix = " ms";
    c.visible = releaseNormalOnly;
    schema.push_back(c);
  }
  // Override timer checkbox (only when delay release is on)
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Cancel previous";
    c.propertyKey = "overrideTimer";
    c.visible = [](const Zone *z) {
      return z && z->releaseBehavior == Zone::ReleaseBehavior::Normal &&
             z->delayReleaseOn;
    };
    schema.push_back(c);
  }
  // Chord voicing: Instrument, Voicing Style, Add Bass (only when poly + chord)
  {
    ZoneControl c =
        createSeparator("Chord voicing", juce::Justification::centredLeft);
    c.visible = polyAndChordOn;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Instrument";
    c.propertyKey = "instrumentMode";
    c.options[1] = "Piano";
    c.options[2] = "Guitar";
    c.affectsCache = true;
    c.visible = polyAndChordOn;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Voicing Style";
    c.propertyKey = "pianoVoicingStyle";
    c.options[1] = "Block (Raw)";
    c.options[2] = "Close (Pop)";
    c.options[3] = "Open (Cinematic)";
    c.affectsCache = true;
    c.visible = polyAndChordOnPiano;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Magnet";
    c.propertyKey = "voicingMagnetSemitones";
    c.min = -6;
    c.max = 6;
    c.step = 1;
    c.suffix = " (0=root)";
    c.affectsCache = true;
    c.visible = pianoCloseOrOpenOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Player Position";
    c.propertyKey = "guitarPlayerPosition";
    c.options[1] = "Campfire (Open)";
    c.options[2] = "Rhythm (Virtual Capo)";
    c.affectsCache = true;
    c.visible = polyAndChordOnGuitar;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Fret Anchor";
    c.propertyKey = "guitarFretAnchor";
    c.min = 1;
    c.max = 12;
    c.step = 1;
    c.affectsCache = true;
    c.visible = polyAndChordOnGuitarRhythm;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Strum Pattern";
    c.propertyKey = "strumPattern";
    c.options[1] = "Down";
    c.options[2] = "Up";
    c.options[3] = "Auto-alternating";
    c.visible = polyAndChordOnGuitar;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Toggle;
    c.label = "Ghost Notes (middle strings)";
    c.propertyKey = "strumGhostNotes";
    c.visible = polyAndChordOnGuitar;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::AddBassWithOctave;
    c.label = "Add Bass";
    c.min = -3;
    c.max = -1;
    c.step = 1;
    c.visible = polyAndChordOn;
    c.affectsCache = true;
    schema.push_back(c);
  }

  // --- Keys & Layout ---
  schema.push_back(
      createSeparator("Keys & Layout", juce::Justification::centredLeft));
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomKeyAssign;
    c.label = "Assign / Remove Keys";
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Layout Strategy";
    c.propertyKey = "layoutStrategy";
    c.options[1] = "Linear";
    c.options[2] = "Grid";
    c.options[3] = "Piano";
    c.affectsCache = true;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::Slider;
    c.label = "Grid Interval";
    c.propertyKey = "gridInterval";
    c.min = -12;
    c.max = 12;
    c.step = 1;
    c.affectsCache = true;
    c.visible = gridLayoutOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::LabelOnly;
    c.label = "Requires 2 rows of keys";
    c.visible = pianoLayoutOnly;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomChipList;
    c.label = "Assigned Keys";
    schema.push_back(c);
  }

  // --- Display and appearance ---
  schema.push_back(createSeparator("Display and appearance",
                                   juce::Justification::centredLeft));
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::ComboBox;
    c.label = "Display Mode";
    c.propertyKey = "showRomanNumerals";
    c.options[1] = "Note Name";
    c.options[2] = "Roman Numeral";
    c.affectsCache = true;
    schema.push_back(c);
  }
  {
    ZoneControl c;
    c.controlType = ZoneControl::Type::CustomColor;
    c.label = "Zone Color";
    schema.push_back(c);
  }

  // Filter by visibility
  ZoneSchema filtered;
  for (const auto &c : schema) {
    if (c.visible && !c.visible(zone))
      continue;
    filtered.push_back(c);
  }
  return filtered;
}

juce::String ZoneDefinition::getSchemaSignature(const Zone *zone) {
  ZoneSchema schema = getSchema(zone);
  juce::String s;
  for (const auto &c : schema) {
    s += juce::String(static_cast<int>(c.controlType)) + ":" + c.propertyKey +
         ",";
  }
  return s;
}
