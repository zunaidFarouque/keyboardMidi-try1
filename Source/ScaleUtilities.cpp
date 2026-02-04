#include "ScaleUtilities.h"
#include <cmath>

int ScaleUtilities::calculateMidiNote(int rootNote,
                                      const std::vector<int> &intervals,
                                      int degreeIndex) {
  int size = static_cast<int>(intervals.size());

  if (size == 0)
    return rootNote; // Safety check

  // Calculate octaves and note index within scale
  // Using floor division for proper negative handling
  int octaves =
      static_cast<int>(std::floor(static_cast<float>(degreeIndex) / size));
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

juce::String
ScaleUtilities::getRomanNumeral(int degree, const std::vector<int> &intervals) {
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
  const juce::String romanNumerals[] = {"I", "II", "III", "IV",
                                        "V", "VI", "VII"};
  const juce::String romanNumeralsLower[] = {"i", "ii", "iii", "iv",
                                             "v", "vi", "vii"};

  juce::String baseNumeral;
  if (isMajor || isAugmented) {
    baseNumeral = romanNumerals[noteIndex];
  } else {
    baseNumeral = romanNumeralsLower[noteIndex];
  }

  // Add quality modifiers
  if (isDiminished) {
    baseNumeral += juce::String(
        static_cast<juce::juce_wchar>(0x00B0)); // Degree symbol (°)
  } else if (isAugmented) {
    baseNumeral += "+";
  }

  return baseNumeral;
}

int ScaleUtilities::findScaleDegree(int midiNote, int rootNote,
                                    const std::vector<int> &intervals) {
  if (intervals.empty())
    return 0; // Safety check

  // Calculate semitone offset from root
  int offset = midiNote - rootNote;

  // Normalize offset to be in range [0, 11] (chromatic position)
  int chromaticOffset = ((offset % 12) + 12) % 12;

  // Find which interval matches this chromatic position
  int bestMatch = 0;
  int bestDistance = 12; // Max distance

  // Check all intervals in the scale
  for (size_t i = 0; i < intervals.size(); ++i) {
    int intervalChromatic = intervals[i] % 12;
    int distance = std::abs(chromaticOffset - intervalChromatic);
    if (distance < bestDistance) {
      bestDistance = distance;
      bestMatch = static_cast<int>(i);
    }
  }

  // Calculate octaves
  int octaves = (offset - intervals[bestMatch]) / 12;

  // Return degree index: bestMatch + (octaves * scale size)
  return bestMatch + (octaves * static_cast<int>(intervals.size()));
}

int ScaleUtilities::smartStepOffsetToPitchBend(
    int midiNote, int rootNote, const std::vector<int> &intervals,
    int stepOffset, int pitchBendRange) {
  if (pitchBendRange < 1)
    pitchBendRange = 1;

  if (intervals.empty())
    return 8192; // Neutral if no scale is defined

  // 1. Find the current degree of the note in the global scale.
  int currentDegree = findScaleDegree(midiNote, rootNote, intervals);

  // 2. Target degree after applying the scale step offset.
  int targetDegree = currentDegree + stepOffset;

  // 3. Convert target degree back to a MIDI note in the same scale.
  int targetNote = calculateMidiNote(rootNote, intervals, targetDegree);

  // 4. Convert semitone difference into pitch-bend value using the global PB
  // range.
  int semitoneDelta = targetNote - midiNote;
  double frac =
      static_cast<double>(semitoneDelta) / static_cast<double>(pitchBendRange);

  int pbValue = 8192 + static_cast<int>(std::round(frac * 8192.0));
  return juce::jlimit(0, 16383, pbValue);
}
