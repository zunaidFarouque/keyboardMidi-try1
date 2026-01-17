#include "ScaleUtilities.h"
#include <cmath>

int ScaleUtilities::calculateMidiNote(int rootNote, const std::vector<int>& intervals, int degreeIndex) {
  int size = static_cast<int>(intervals.size());

  if (size == 0)
    return rootNote; // Safety check

  // Calculate octaves and note index within scale
  // Using floor division for proper negative handling
  int octaves = static_cast<int>(std::floor(static_cast<float>(degreeIndex) / size));
  int noteIndex = degreeIndex % size;

  // Fix negative noteIndex to be in range [0, size-1]
  if (noteIndex < 0) {
    noteIndex += size;
  }

  // Calculate final MIDI note: root + octaves * 12 + interval offset
  int result = rootNote + (octaves * 12) + intervals[noteIndex];

  // Clamp to valid MIDI range
  return juce::jlimit(0, 127, result);
}
