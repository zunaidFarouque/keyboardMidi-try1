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

juce::String ScaleUtilities::getRomanNumeral(int degree, const std::vector<int>& intervals) {
  int size = static_cast<int>(intervals.size());
  
  if (size == 0)
    return "";
  
  // Normalize degree to be in range [0, size-1]
  int noteIndex = degree % size;
  if (noteIndex < 0) {
    noteIndex += size;
  }
  
  // Calculate octave offset for wrapping
  int octaves = static_cast<int>(std::floor(static_cast<float>(degree) / size));
  
  // Get intervals for root, third, and fifth
  int rootInterval = intervals[noteIndex];
  
  // Third is at (degree + 2) % size
  int thirdIndex = (noteIndex + 2) % size;
  int thirdInterval = intervals[thirdIndex];
  if (thirdIndex < noteIndex) {
    // Wrapped around - add 12 semitones
    thirdInterval += 12;
  }
  
  // Fifth is at (degree + 4) % size
  int fifthIndex = (noteIndex + 4) % size;
  int fifthInterval = intervals[fifthIndex];
  if (fifthIndex < noteIndex) {
    // Wrapped around - add 12 semitones
    fifthInterval += 12;
  }
  
  // Calculate differences
  int diffThird = thirdInterval - rootInterval;
  int diffFifth = fifthInterval - rootInterval;
  
  // Determine quality
  bool isMajor = (diffThird == 4);
  bool isMinor = (diffThird == 3);
  bool isDiminished = (isMinor && diffFifth == 6);
  bool isAugmented = (isMajor && diffFifth == 8);
  
  // Roman numeral mapping (1-based)
  const juce::String romanNumerals[] = {"I", "II", "III", "IV", "V", "VI", "VII"};
  const juce::String romanNumeralsLower[] = {"i", "ii", "iii", "iv", "v", "vi", "vii"};
  
  juce::String baseNumeral;
  if (isMajor || isAugmented) {
    baseNumeral = romanNumerals[noteIndex];
  } else {
    baseNumeral = romanNumeralsLower[noteIndex];
  }
  
  // Add quality modifiers
  if (isDiminished) {
    baseNumeral += juce::String(static_cast<juce::juce_wchar>(0x00B0)); // Degree symbol (°)
  } else if (isAugmented) {
    baseNumeral += "+";
  }
  
  return baseNumeral;
}
