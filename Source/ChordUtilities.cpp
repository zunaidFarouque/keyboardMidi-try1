#include "ChordUtilities.h"
#include "ScaleUtilities.h"

std::vector<int> ChordUtilities::generateChord(int rootNote, 
                                                const std::vector<int>& scaleIntervals, 
                                                int degreeIndex, 
                                                ChordType type, 
                                                Voicing voicing) {
  std::vector<int> notes;

  if (type == ChordType::None) {
    // Single note - just return the root
    int note = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
    notes.push_back(note);
    return notes;
  }

  // Calculate base notes for the chord
  int root = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex);
  int third = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 2);
  int fifth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 4);
  int seventh = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 6);
  int ninth = ScaleUtilities::calculateMidiNote(rootNote, scaleIntervals, degreeIndex + 8);

  // Build chord based on type
  if (type == ChordType::Triad) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
  } else if (type == ChordType::Seventh) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
    notes.push_back(seventh);
  } else if (type == ChordType::Ninth) {
    notes.push_back(root);
    notes.push_back(third);
    notes.push_back(fifth);
    notes.push_back(seventh);
    notes.push_back(ninth);
  } else if (type == ChordType::Power5) {
    notes.push_back(root);
    notes.push_back(fifth);
  }

  // Apply voicing
  if (voicing == Voicing::Close) {
    // Keep notes as calculated
    return notes;
  } else if (voicing == Voicing::Open) {
    // Move the middle note (3rd) up one octave
    if (notes.size() >= 2) {
      notes[1] += 12; // Move 3rd up one octave
    }
    return notes;
  } else if (voicing == Voicing::Guitar) {
    // Guitar voicing: Root, 5th, Octave, 10th (3rd + 12)
    std::vector<int> guitarNotes;
    guitarNotes.push_back(root);
    guitarNotes.push_back(fifth);
    guitarNotes.push_back(root + 12); // Octave
    guitarNotes.push_back(third + 12); // 10th (3rd + 12)
    return guitarNotes;
  }

  return notes;
}
