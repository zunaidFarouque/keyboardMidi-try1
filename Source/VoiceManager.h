#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "StrumEngine.h"
#include <JuceHeader.h>
#include <unordered_map>
#include <vector>

// Manages active notes to prevent stuck notes
// Tracks what's currently playing and ensures NoteOff is sent
// Now supports chords (multiple notes per input)
class VoiceManager {
public:
  explicit VoiceManager(MidiEngine &engine);
  ~VoiceManager() = default;

  // Trigger a single note on and track it
  void noteOn(InputID source, int note, int vel, int channel);

  // Trigger multiple notes (chord) with strumming
  void noteOn(InputID source, const std::vector<int>& notes, int vel, int channel, int strumSpeedMs);

  // Strum notes from a buffer (with direction)
  void strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke);

  // Send all NoteOff messages for a given input and remove from map
  void handleKeyUp(InputID source);

  // Send MIDI CC message
  void sendCC(int channel, int controller, int value);

  // Emergency: Send allNotesOff on all channels and clear map
  void panic();

private:
  MidiEngine &midiEngine;
  StrumEngine strumEngine;

  // Maps InputID to (note, channel) for correct note-off
  std::unordered_multimap<InputID, std::pair<int, int>> activeNoteNumbers;
};
