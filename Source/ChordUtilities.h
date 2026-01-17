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
    Close,  // Notes as calculated (close together)
    Open,   // Middle note (3rd) moved up one octave
    Guitar  // Root, 5th, Octave, 10th (3rd + 12)
  };

  // Generate chord notes from root note, scale intervals, and degree index
  // Returns vector of MIDI note numbers
  static std::vector<int> generateChord(int rootNote, 
                                        const std::vector<int>& scaleIntervals, 
                                        int degreeIndex, 
                                        ChordType type, 
                                        Voicing voicing);
};
