#include "MidiNoteUtilities.h"

juce::String MidiNoteUtilities::getMidiNoteName(int noteNumber) {
  // Clamp to valid MIDI range
  noteNumber = juce::jlimit(0, 127, noteNumber);
  
  // Use JUCE's built-in MIDI note name function
  // Parameters: noteNumber, useSharps, includeOctaveNumber, middleC
  return juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 4);
}

int MidiNoteUtilities::getMidiNoteFromText(const juce::String &text) {
  juce::String trimmed = text.trim();
  
  if (trimmed.isEmpty())
    return 60; // Default to Middle C
  
  // First, try to parse as a raw integer
  if (trimmed.containsOnly("0123456789-+")) {
    int value = trimmed.getIntValue();
    return juce::jlimit(0, 127, value);
  }
  
  // Parse as Note Name (e.g., "C#3", "Bb4", "A-1", "Cb2")
  juce::String upper = trimmed.toUpperCase();
  
  // Find note letter (A-G)
  int noteIndex = -1;
  int noteLetterPos = -1;
  for (int i = 0; i < upper.length(); ++i) {
    char c = upper[i];
    if (c >= 'A' && c <= 'G') {
      // Map note letters to semitone offsets from C
      switch (c) {
      case 'C': noteIndex = 0; break;
      case 'D': noteIndex = 2; break;
      case 'E': noteIndex = 4; break;
      case 'F': noteIndex = 5; break;
      case 'G': noteIndex = 7; break;
      case 'A': noteIndex = 9; break;
      case 'B': noteIndex = 11; break;
      }
      noteLetterPos = i;
      break;
    }
  }
  
  if (noteIndex == -1)
    return 60; // Default to Middle C if parsing fails
  
  // Find accidental (# or b) - must come after note letter
  bool hasSharp = false;
  bool hasFlat = false;
  
  if (noteLetterPos + 1 < upper.length()) {
    char next = upper[noteLetterPos + 1];
    if (next == '#')
      hasSharp = true;
    else if (next == 'B' || next == '♭')
      hasFlat = true;
  }
  
  // Also check original case for lowercase 'b'
  if (!hasFlat && noteLetterPos + 1 < trimmed.length()) {
    if (trimmed[noteLetterPos + 1] == 'b' || trimmed[noteLetterPos + 1] == '♭')
      hasFlat = true;
  }
  
  // Adjust for sharp/flat
  if (hasSharp)
    noteIndex++;
  else if (hasFlat)
    noteIndex--;
  
  // Find octave number (can be negative, e.g., "C-1")
  int octave = 4; // Default to octave 4 (Middle C)
  
  // Look for digits that represent octave
  for (int i = 0; i < trimmed.length(); ++i) {
    if (juce::CharacterFunctions::isDigit(trimmed[i]) || 
        (trimmed[i] == '-' && i + 1 < trimmed.length() && 
         juce::CharacterFunctions::isDigit(trimmed[i + 1]))) {
      // Extract the full number (including negative sign)
      juce::String octaveStr;
      int start = i;
      
      // Include minus sign if present
      if (start > 0 && trimmed[start - 1] == '-')
        start--;
      
      // Extract digits
      while (start < trimmed.length() && 
             (juce::CharacterFunctions::isDigit(trimmed[start]) || 
              trimmed[start] == '-')) {
        octaveStr += trimmed[start];
        start++;
        if (start < trimmed.length() && juce::CharacterFunctions::isDigit(trimmed[start]))
          continue;
        else
          break;
      }
      
      octave = octaveStr.getIntValue();
      break;
    }
  }
  
  // Calculate MIDI note: (Octave + 1) * 12 + NoteIndex
  int midiNote = (octave + 1) * 12 + noteIndex;
  
  // Clamp to valid MIDI range
  return juce::jlimit(0, 127, midiNote);
}
