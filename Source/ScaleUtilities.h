#pragma once
#include <JuceHeader.h>
#include <vector>

class ScaleUtilities {
public:
  // Calculate MIDI note from root note, scale intervals, and degree index
  // Intervals are semitone offsets from root (e.g., Major = {0, 2, 4, 5, 7, 9,
  // 11}) Degree index can be negative (moving down octaves)
  static int calculateMidiNote(int rootNote, const std::vector<int> &intervals,
                               int degreeIndex);

  // Generate Roman numeral for a scale degree (e.g., 0 -> "I", 1 -> "ii", 4 ->
  // "V") Returns string like "I", "ii", "iii", "IV", "V", "vi", "vii°", "III+"
  static juce::String getRomanNumeral(int degree,
                                      const std::vector<int> &intervals);

  // Find the scale degree index of a MIDI note given root and intervals
  // Returns the degree index (can be negative for notes below root)
  // If note is not in scale, returns the closest scale degree
  static int findScaleDegree(int midiNote, int rootNote,
                             const std::vector<int> &intervals);

  // Map a SmartScale step offset to a pitch-bend value for a given MIDI note.
  // - midiNote: the currently playing MIDI note (0-127)
  // - rootNote: global scale root note
  // - intervals: global scale intervals (semitone offsets from root)
  // - stepOffset: scale step offset (can be negative)
  // - pitchBendRange: global PB range in semitones (>= 1)
  // Returns a 14-bit PB value in [0, 16383], centered at 8192.
  static int smartStepOffsetToPitchBend(int midiNote, int rootNote,
                                        const std::vector<int> &intervals,
                                        int stepOffset, int pitchBendRange);
};
