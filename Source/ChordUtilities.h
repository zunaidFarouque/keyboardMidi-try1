#pragma once
#include "ScaleUtilities.h"
#include <vector>

class ChordUtilities {
public:
  enum class ChordType {
    None,    // Single note (no chord)
    Triad,   // Root, 3rd, 5th
    Seventh, // Root, 3rd, 5th, 7th
    Ninth,   // Root, 3rd, 5th, 7th, 9th
    Power5   // Root, 5th (power chord)
  };

  enum class Voicing {
    RootPosition,  // Notes as calculated (close together, root position)
    Smooth,        // Inversions to minimize movement (cluster around center)
    GuitarSpread,  // Spread voicings to reduce mud (open/drop voicings)
    SmoothFilled,  // Smooth with ghost notes (filled voicings)
    GuitarFilled   // Guitar spread with ghost notes (filled voicings)
  };

  // Piano voicing style (plan: Block / Close / Open)
  enum class PianoVoicingStyle {
    Block,   // Raw root position (no voicing)
    Close,   // Smart Flow: Triad -> Gravity Well, 7th/9th -> Alternating Grip
    Open     // Drop-2 + Smart Flow (spread, then cluster near center)
  };

  // Represents a chord note with optional ghost flag
  struct ChordNote {
    int pitch;      // MIDI note number
    bool isGhost;   // True if this is a ghost note (fills gaps, quieter velocity)
    
    ChordNote(int p, bool ghost = false) : pitch(p), isGhost(ghost) {}
  };

  // Generate chord notes from root note, scale intervals, and degree index
  // Returns vector of ChordNote (with ghost flags for filled voicings)
  static std::vector<ChordNote> generateChord(int rootNote, 
                                               const std::vector<int>& scaleIntervals, 
                                               int degreeIndex, 
                                               ChordType type, 
                                               Voicing voicing,
                                               bool strictGhostHarmony = true);

  // Generate chord using piano voicing style (Block / Close / Open).
  // Close = Smart Flow (Triad -> Gravity Well, 7th/9th -> Alternating Grip).
  // Open = Drop-2 then Smart Flow. Used at compile time only.
  static std::vector<ChordNote> generateChordForPiano(int rootNote,
                                                       const std::vector<int>& scaleIntervals,
                                                       int degreeIndex,
                                                       ChordType type,
                                                       PianoVoicingStyle style,
                                                       bool strictGhostHarmony = true);

  // Generate chord for guitar: fretboard logic in [fretMin, fretMax].
  // Campfire = 0,4; Rhythm (Virtual Capo) = guitarFretAnchor, guitarFretAnchor+3.
  // Returns 4-6 ChordNotes (no ghost); bass isolation applied (mute low E when root on A, etc.).
  static std::vector<ChordNote> generateChordForGuitar(int rootNote,
                                                       const std::vector<int>& scaleIntervals,
                                                       int degreeIndex,
                                                       ChordType type,
                                                       int fretMin,
                                                       int fretMax);

  // Debug: Export comprehensive voicing report
  static void dumpDebugReport(juce::File targetFile);
  
private:
  // Gravity Well: pick inversion whose average pitch is closest to centerPitch
  static std::vector<int> applyGravityWell(const std::vector<int>& notes, int centerPitch);
  // Alternating Grip: odd degree -> root position, even degree -> 2nd inversion (7th/9th)
  static std::vector<int> applyAlternatingGrip(const std::vector<int>& notes, int degreeIndex);
  // Drop-2: for 4+ note chord, drop 2nd from top down one octave
  static std::vector<int> applyDrop2(std::vector<int> notes);
  // Smart Flow: Triad -> Gravity Well, 7th/9th -> Alternating Grip
  static std::vector<int> applySmartFlow(const std::vector<int>& notes, ChordType type,
                                         int degreeIndex, int centerPitch);
  // Helper: Add ghost notes to fill gaps in voicing
  static void addGhostNotes(std::vector<ChordNote>& notes,
                            int zoneAnchor,
                            ChordType type,
                            const std::vector<int>& scaleIntervals,
                            int degreeIndex,
                            bool strictHarmony);
};
