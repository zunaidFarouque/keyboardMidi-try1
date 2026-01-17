#include "ScaleUtilities.h"
#include <vector>

static const std::vector<int> chromaticIntervals = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11
};

static const std::vector<int> majorIntervals = {
    0, 2, 4, 5, 7, 9, 11  // C, D, E, F, G, A, B
};

static const std::vector<int> minorIntervals = {
    0, 2, 3, 5, 7, 8, 10  // C, D, Eb, F, G, Ab, Bb
};

static const std::vector<int> pentatonicMajorIntervals = {
    0, 2, 4, 7, 9  // C, D, E, G, A
};

static const std::vector<int> pentatonicMinorIntervals = {
    0, 3, 5, 7, 10  // C, Eb, F, G, Bb
};

static const std::vector<int> bluesIntervals = {
    0, 3, 5, 6, 7, 10  // C, Eb, F, F#, G, Bb
};

const std::vector<int> &ScaleUtilities::getScaleIntervals(ScaleType scale) {
  switch (scale) {
  case ScaleType::Chromatic:
    return chromaticIntervals;
  case ScaleType::Major:
    return majorIntervals;
  case ScaleType::Minor:
    return minorIntervals;
  case ScaleType::PentatonicMajor:
    return pentatonicMajorIntervals;
  case ScaleType::PentatonicMinor:
    return pentatonicMinorIntervals;
  case ScaleType::Blues:
    return bluesIntervals;
  default:
    return majorIntervals; // Fallback
  }
}

int ScaleUtilities::calculateMidiNote(int rootNote, ScaleType scale, int degreeIndex) {
  const auto &intervals = getScaleIntervals(scale);
  int scaleSize = static_cast<int>(intervals.size());

  if (scaleSize == 0)
    return rootNote; // Safety check

  // Calculate octave shift and note index within scale
  int octaveShift = degreeIndex / scaleSize;
  int noteIndex = degreeIndex % scaleSize;

  // Handle negative degreeIndex
  if (degreeIndex < 0) {
    octaveShift = (degreeIndex - scaleSize + 1) / scaleSize;
    noteIndex = ((degreeIndex % scaleSize) + scaleSize) % scaleSize;
  }

  // Calculate final MIDI note
  int result = rootNote + (octaveShift * 12) + intervals[noteIndex];

  // Clamp to valid MIDI range
  return juce::jlimit(0, 127, result);
}
