#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include <JuceHeader.h>
#include <unordered_map>

// Manages active notes to prevent stuck notes
// Tracks what's currently playing and ensures NoteOff is sent
class VoiceManager {
public:
  explicit VoiceManager(MidiEngine &engine);
  ~VoiceManager() = default;

  // Trigger a note on and track it
  void noteOn(InputID source, int note, int vel, int channel);

  // Send all NoteOff messages for a given input and remove from map
  void handleKeyUp(InputID source);

  // Emergency: Send allNotesOff on all channels and clear map
  void panic();

private:
  MidiEngine &midiEngine;

  // Maps InputID to the corresponding NoteOff message
  // Uses multimap because same key could theoretically trigger multiple notes
  std::unordered_multimap<InputID, juce::MidiMessage> activeNotes;
};
