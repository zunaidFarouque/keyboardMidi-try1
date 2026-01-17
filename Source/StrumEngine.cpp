#include "StrumEngine.h"
#include "MappingTypes.h"

StrumEngine::StrumEngine(MidiEngine& engine) 
  : midiEngine(engine) {
  startTimer(1); // Update every 1ms for sub-millisecond precision
}

StrumEngine::~StrumEngine() {
  stopTimer();
}

void StrumEngine::triggerStrum(const std::vector<int>& notes, 
                                int velocity, 
                                int channel, 
                                int speedMs,
                                InputID source) {
  juce::ScopedLock lock(queueLock);
  
  double now = getCurrentTimeMs();
  
  for (size_t i = 0; i < notes.size(); ++i) {
    PendingNote pending;
    pending.note = notes[i];
    pending.velocity = velocity;
    pending.channel = channel;
    pending.targetTimeMs = now + (static_cast<double>(i) * speedMs);
    pending.source = source;
    
    noteQueue.push_back(pending);
  }
}

void StrumEngine::cancelPendingNotes(InputID source) {
  juce::ScopedLock lock(queueLock);
  
  // Remove all pending notes for this source
  noteQueue.erase(
    std::remove_if(noteQueue.begin(), noteQueue.end(),
      [source](const PendingNote& p) {
        return p.source.deviceHandle == source.deviceHandle && 
               p.source.keyCode == source.keyCode;
      }),
    noteQueue.end()
  );
}

void StrumEngine::hiResTimerCallback() {
  juce::ScopedLock lock(queueLock);
  
  double now = getCurrentTimeMs();
  currentTimeMs = now;
  
  // Process all notes that are ready
  auto it = noteQueue.begin();
  while (it != noteQueue.end()) {
    if (it->targetTimeMs <= now) {
      // Time to play this note
      midiEngine.sendNoteOn(it->channel, it->note, static_cast<float>(it->velocity) / 127.0f);
      
      // Remove from queue
      it = noteQueue.erase(it);
    } else {
      ++it;
    }
  }
}

double StrumEngine::getCurrentTimeMs() const {
  return juce::Time::getMillisecondCounterHiRes();
}
