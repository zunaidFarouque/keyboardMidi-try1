#pragma once
#include <JuceHeader.h>

class ScaleUtilities {
public:
  enum class ScaleType {
    Chromatic,
    Major,
    Minor,
    PentatonicMajor,
    PentatonicMinor,
    Blues
  };

  // Calculate MIDI note from root note, scale type, and degree index
  static int calculateMidiNote(int rootNote, ScaleType scale, int degreeIndex);

private:
  // Get scale intervals for a given scale type
  static const std::vector<int> &getScaleIntervals(ScaleType scale);
};
