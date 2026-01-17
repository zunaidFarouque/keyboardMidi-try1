#include "VoiceManager.h"

VoiceManager::VoiceManager(MidiEngine &engine) 
  : midiEngine(engine), strumEngine(engine) {}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel) {
  midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
  activeNoteNumbers.insert({source, {note, channel}});
}

void VoiceManager::noteOn(InputID source, const std::vector<int>& notes, int vel, int channel, int strumSpeedMs) {
  if (notes.empty())
    return;

  if (strumSpeedMs == 0) {
    for (int note : notes) {
      midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
      activeNoteNumbers.insert({source, {note, channel}});
    }
  } else {
    strumEngine.triggerStrum(notes, vel, channel, strumSpeedMs, source);
    for (int note : notes)
      activeNoteNumbers.insert({source, {note, channel}});
  }
}

void VoiceManager::strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke) {
  if (notes.empty())
    return;

  for (const auto& p : activeNoteNumbers)
    midiEngine.sendNoteOff(p.second.second, p.second.first);
  activeNoteNumbers.clear();

  std::vector<int> notesToStrum = notes;
  if (!downstroke)
    std::reverse(notesToStrum.begin(), notesToStrum.end());

  InputID dummySource = {0, 0};
  int velocity = 100;
  int channel = 1;
  strumEngine.triggerStrum(notesToStrum, velocity, channel, speedMs, dummySource);
  for (int note : notesToStrum)
    activeNoteNumbers.insert({dummySource, {note, channel}});
}

void VoiceManager::handleKeyUp(InputID source) {
  strumEngine.cancelPendingNotes(source);

  auto range = activeNoteNumbers.equal_range(source);
  for (auto it = range.first; it != range.second; ++it) {
    int note = it->second.first;
    int ch = it->second.second;
    midiEngine.sendNoteOff(ch, note);
  }
  activeNoteNumbers.erase(range.first, range.second);
}

void VoiceManager::sendCC(int channel, int controller, int value) {
  midiEngine.sendCC(channel, controller, value);
}

void VoiceManager::panic() {
  for (const auto& p : activeNoteNumbers)
    midiEngine.sendNoteOff(p.second.second, p.second.first);
  activeNoteNumbers.clear();
}
