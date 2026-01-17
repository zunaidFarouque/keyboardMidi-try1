#include "VoiceManager.h"

VoiceManager::VoiceManager(MidiEngine &engine) 
  : midiEngine(engine), strumEngine(engine) {}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel) {
  // Single note version - send immediately
  midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);

  // Store the note number for this input
  activeNoteNumbers.insert({source, note});
}

void VoiceManager::noteOn(InputID source, const std::vector<int>& notes, int vel, int channel, int strumSpeedMs) {
  if (notes.empty())
    return;

  if (strumSpeedMs == 0) {
    for (int note : notes) {
      midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
      activeNoteNumbers.insert({source, note});
    }
  } else {
    // Use strum engine for timed playback
    strumEngine.triggerStrum(notes, vel, channel, strumSpeedMs, source);
    
    // Store all note numbers (they'll be played by StrumEngine)
    for (int note : notes) {
      activeNoteNumbers.insert({source, note});
    }
  }
}

void VoiceManager::strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke) {
  if (notes.empty())
    return;

  // Send NoteOff for any currently playing notes (auto-choke previous strum)
  // Note: This is a simplified approach - in a full implementation, we'd track which notes are playing
  // For now, we'll just send NoteOff for all active notes
  for (const auto& pair : activeNoteNumbers) {
    midiEngine.sendNoteOff(1, pair.second); // Use channel 1 as default
  }
  activeNoteNumbers.clear();

  // Create a copy of notes for strumming
  std::vector<int> notesToStrum = notes;
  
  // Reverse if upstroke
  if (!downstroke) {
    std::reverse(notesToStrum.begin(), notesToStrum.end());
  }

  // Use a dummy InputID for buffered strums (since there's no specific input source)
  InputID dummySource = {0, 0};
  
  // Trigger strum with default velocity and channel
  int velocity = 100;
  int channel = 1;
  strumEngine.triggerStrum(notesToStrum, velocity, channel, speedMs, dummySource);
  
  // Store note numbers for tracking
  for (int note : notesToStrum) {
    activeNoteNumbers.insert({dummySource, note});
  }
}

void VoiceManager::handleKeyUp(InputID source) {
  // Cancel any pending strums for this input (prevents ghost strums)
  strumEngine.cancelPendingNotes(source);

  // Find all entries with this InputID
  auto range = activeNoteNumbers.equal_range(source);

  // Send all NoteOff messages for this input
  for (auto it = range.first; it != range.second; ++it) {
    // We need to determine the channel - for now, use channel 1
    // In a full implementation, we'd track channel per note
    midiEngine.sendNoteOff(1, it->second);
  }

  // Remove all entries for this InputID
  activeNoteNumbers.erase(range.first, range.second);
}

void VoiceManager::sendCC(int channel, int controller, int value) {
  midiEngine.sendCC(channel, controller, value);
}

void VoiceManager::panic() {
  // Cancel all pending strums
  // Note: StrumEngine doesn't have a cancelAll method, so we'll clear the queue
  // For now, we'll just send NoteOff for all active notes
  
  // Send NoteOff for each active note
  for (const auto &pair : activeNoteNumbers) {
    // We need to determine the channel - for now, use channel 1
    midiEngine.sendNoteOff(1, pair.second);
  }

  // Clear the map
  activeNoteNumbers.clear();
}
