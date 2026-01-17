#pragma once
#include <JuceHeader.h>

class MidiNoteUtilities {
public:
  // Convert MIDI note number to note name (e.g., 60 -> "C4")
  static juce::String getMidiNoteName(int noteNumber);

  // Parse text input to MIDI note number (e.g., "C#3" -> 49, "60" -> 60)
  static int getMidiNoteFromText(const juce::String &text);
};
