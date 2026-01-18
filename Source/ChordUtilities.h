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

  // Debug: Export comprehensive voicing report
  static void dumpDebugReport(juce::File targetFile);
  
private:
  // Helper: Add ghost notes to fill gaps in voicing
  static void addGhostNotes(std::vector<ChordNote>& notes,
                            int zoneAnchor,
                            ChordType type,
                            const std::vector<int>& scaleIntervals,
                            int degreeIndex,
                            bool strictHarmony);
};
