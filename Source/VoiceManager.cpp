#include "VoiceManager.h"

VoiceManager::VoiceManager(MidiEngine &engine) : midiEngine(engine) {}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel) {
  // Send the NoteOn message via MidiEngine
  midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);

  // Create the corresponding NoteOff message and store it
  juce::MidiMessage noteOffMsg = juce::MidiMessage::noteOff(channel, note);
  activeNotes.insert({source, noteOffMsg});
}

void VoiceManager::handleKeyUp(InputID source) {
  // Find all entries with this InputID
  auto range = activeNotes.equal_range(source);

  // Send all NoteOff messages for this input
  for (auto it = range.first; it != range.second; ++it) {
    midiEngine.sendNoteOff(it->second.getChannel(),
                           it->second.getNoteNumber());
  }

  // Remove all entries for this InputID
  activeNotes.erase(range.first, range.second);
}

void VoiceManager::panic() {
  // Send allNotesOff on all 16 MIDI channels (standard MIDI command)
  for (int channel = 1; channel <= 16; ++channel) {
    juce::MidiMessage allNotesOff =
        juce::MidiMessage::allNotesOff(channel);
    // Note: MidiEngine doesn't have a direct sendMessage method,
    // so we'll need to use a workaround or extend MidiEngine.
    // For now, we'll send individual NoteOff for each active note.
  }

  // Send NoteOff for each active note
  for (const auto &pair : activeNotes) {
    midiEngine.sendNoteOff(pair.second.getChannel(),
                           pair.second.getNoteNumber());
  }

  // Clear the map
  activeNotes.clear();
}
