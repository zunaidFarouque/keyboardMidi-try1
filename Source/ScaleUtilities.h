#pragma once
#include <JuceHeader.h>
#include <vector>

class ScaleUtilities {
public:
  // Calculate MIDI note from root note, scale intervals, and degree index
  // Intervals are semitone offsets from root (e.g., Major = {0, 2, 4, 5, 7, 9, 11})
  // Degree index can be negative (moving down octaves)
  static int calculateMidiNote(int rootNote, const std::vector<int>& intervals, int degreeIndex);
  
  // Generate Roman numeral for a scale degree (e.g., 0 -> "I", 1 -> "ii", 4 -> "V")
  // Returns string like "I", "ii", "iii", "IV", "V", "vi", "vii°", "III+"
  static juce::String getRomanNumeral(int degree, const std::vector<int>& intervals);
};
