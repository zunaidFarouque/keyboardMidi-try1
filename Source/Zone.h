#pragma once
#include "ChordUtilities.h"
#include "MappingTypes.h"
#include "ScaleUtilities.h"
#include <JuceHeader.h>
#include <optional>
#include <unordered_map>
#include <vector>

class Zone {
public:
  enum class LayoutStrategy { Linear, Grid, Piano };

  enum class PlayMode {
    Direct, // Play immediately on key press
    Strum   // Buffer notes, play on trigger key
  };

  enum class ReleaseBehavior {
    Normal, // Release to keep playing for N ms, then stop
    Sustain // Release to sustain for N ms (notes continue, then sustain)
  };

  enum class InstrumentMode { Piano, Guitar };

  enum class PianoVoicingStyle { Block, Close, Open };

  enum class GuitarPlayerPosition { Campfire, Rhythm };

  enum class StrumPattern { Down, Up, AutoAlternating };

  Zone();

  // Properties
  juce::String name;
  int layerID = 0;           // Phase 49: layer assignment (0=Base)
  uintptr_t targetAliasHash; // The Alias this zone listens to (0 = Global / All
                             // Devices)
  std::vector<int> inputKeyCodes; // The physical keys, ordered
  int rootNote;                   // Base MIDI note
  juce::String scaleName;         // Scale name (looked up from ScaleLibrary)
  int chromaticOffset;            // Global transpose override
  int degreeOffset;               // Scale degree shift
  bool ignoreGlobalTranspose = false; // If true, ignore global transpose
  LayoutStrategy layoutStrategy =
      LayoutStrategy::Linear; // Layout calculation method
  int gridInterval =
      5; // For Grid mode: semitones per row (default 5 = perfect 4th)
  juce::Colour zoneColor; // Visual color for this zone
  int midiChannel = 1;    // MIDI output channel (1-16)
  ChordUtilities::ChordType chordType =
      ChordUtilities::ChordType::None; // Chord type (None = single note)
  int strumSpeedMs =
      0; // Strum speed in milliseconds (0 = no strum, all notes at once)
  bool strumTimingVariationOn = false; // When true, apply timing jitter
  int strumTimingVariationMs =
      0; // Timing jitter: ±N ms per note (when strum timing variation on)
  PlayMode playMode =
      PlayMode::Direct; // Play mode (Direct = immediate, Strum = buffered)
  bool ignoreGlobalSustain =
      false; // If true, sustain pedal does not hold notes (e.g. Drums)
  ReleaseBehavior releaseBehavior =
      ReleaseBehavior::Normal; // How to handle key release in strum mode
  bool delayReleaseOn =
      false; // When true (Normal only), use releaseDurationMs timer on release
  int releaseDurationMs =
      0; // Delay release duration in ms (used only when delayReleaseOn is true)
  int baseVelocity = 100; // Base MIDI velocity (1-127)
  int velocityRandom = 0; // Velocity randomization range (0-64)
  bool strictGhostHarmony =
      true; // Ghost note harmony mode (true = strict 1/5, false = loose 7/9)
  float ghostVelocityScale =
      0.6f;                  // Velocity multiplier for ghost notes (0.0-1.0)
  bool addBassNote = false;  // If true, add a bass note (root shifted down)
  int bassOctaveOffset = -1; // Octave offset for bass note (-3 to -1)
  InstrumentMode instrumentMode = InstrumentMode::Piano;
  PianoVoicingStyle pianoVoicingStyle = PianoVoicingStyle::Close;
  int voicingMagnetSemitones =
      0; // Piano Close/Open: center offset -6..+6. 0 = root as center.
  GuitarPlayerPosition guitarPlayerPosition = GuitarPlayerPosition::Campfire;
  int guitarFretAnchor = 5; // Fret anchor for Rhythm (Virtual Capo) mode
  StrumPattern strumPattern = StrumPattern::Down;
  bool strumGhostNotes = false; // Lower velocity on middle strings (guitar)
  bool showRomanNumerals =
      false; // If true, display Roman numerals instead of note names
  bool useGlobalScale = false; // If true, inherit ZoneManager::globalScaleName
  bool useGlobalRoot = false;  // If true, inherit ZoneManager::globalRootNote
  int globalRootOctaveOffset = 0; // Octave offset when useGlobalRoot (-2 to +2)
  PolyphonyMode polyphonyMode =
      PolyphonyMode::Poly; // Polyphony mode (Phase 26)
  int glideTimeMs = 50;    // Portamento glide time in milliseconds (Phase 26) -
                           // Static time OR min time if adaptive
  bool isAdaptiveGlide =
      false; // If true, glide time adapts to playing speed (Phase 26.1)
  int maxGlideTimeMs = 200; // Maximum glide time for adaptive mode (Phase 26.1)

  // Performance cache: Pre-compiled key-to-chord mappings (compilation
  // strategy). Config-time: rebuildCache() runs ChordUtilities::generateChord,
  // ScaleUtilities; fills this map. Play-time: getNotesForKey() does O(1) find
  // + O(k) transpose (k = chord size). No chord/scale math.
  std::unordered_map<int, std::vector<ChordUtilities::ChordNote>>
      keyToChordCache; // keyCode -> chord notes (with ghost flags)
  std::unordered_map<int, juce::String>
      keyToLabelCache; // keyCode -> display label (note name or Roman numeral)
  int cacheEffectiveRoot =
      60; // Root used for last rebuild; getNotesForKey uses this

  // Config-time: (re)build keyToChordCache when zone/scale/chord/keys change.
  // Caller provides scaleIntervals and effectiveRoot (global or local per
  // ZoneManager logic).
  void rebuildCache(const std::vector<int> &scaleIntervals, int effectiveRoot);

  bool usesGlobalScale() const { return useGlobalScale; }
  bool usesGlobalRoot() const { return useGlobalRoot; }

  // Play-time: O(1) lookup + O(k) transpose. Returns final MIDI chord notes
  // (with ghost flags) or nullopt if not in zone.
  std::optional<std::vector<ChordUtilities::ChordNote>>
  getNotesForKey(int keyCode, int globalChromTrans, int globalDegTrans);

  // Get display label for a key (note name or Roman numeral)
  juce::String getKeyLabel(int keyCode) const;

  // Process a key input and return MIDI action if this zone matches
  // Note: Returns first note of chord for backward compatibility
  std::optional<MidiAction> processKey(InputID input, int globalChromTrans,
                                       int globalDegTrans);

  // Remove a key from inputKeyCodes
  void removeKey(int keyCode);

  // Get input key codes (for ZoneManager lookup table)
  const std::vector<int> &getInputKeyCodes() const { return inputKeyCodes; }

  // Serialization
  juce::ValueTree toValueTree() const;
  static std::shared_ptr<Zone> fromValueTree(const juce::ValueTree &vt);
};
