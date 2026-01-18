#include "StrumEngine.h"
#include "MappingTypes.h"
#include <unordered_map>

StrumEngine::StrumEngine(MidiEngine& engine, OnNotePlayedCallback onPlayed)
  : midiEngine(engine), onNotePlayed(std::move(onPlayed)) {
  startTimer(1);
}

StrumEngine::~StrumEngine() {
  stopTimer();
}

void StrumEngine::triggerStrum(const std::vector<int>& notes, const std::vector<int>& velocities, int channel,
                               int speedMs, InputID source, bool allowSustain) {
  juce::ScopedLock lock(queueLock);
  double now = getCurrentTimeMs();
  for (size_t i = 0; i < notes.size(); ++i) {
    PendingNote p;
    p.note = notes[i];
    // Use per-note velocity if available, otherwise fall back to first velocity
    p.velocity = (i < velocities.size()) ? velocities[i] : (velocities.empty() ? 100 : velocities[0]);
    p.channel = channel;
    p.targetTimeMs = now + (static_cast<double>(i) * speedMs);
    p.source = source;
    p.allowSustain = allowSustain;
    noteQueue.push_back(p);
  }
}

void StrumEngine::cancelPendingNotes(InputID source) {
  juce::ScopedLock lock(queueLock);
  // Remove release info if present
  releaseMap.erase(source);
  // Cancel all pending notes for this source
  noteQueue.erase(
    std::remove_if(noteQueue.begin(), noteQueue.end(),
      [source](const PendingNote& p) {
        return p.source.deviceHandle == source.deviceHandle && p.source.keyCode == source.keyCode;
      }),
    noteQueue.end()
  );
}

void StrumEngine::markSourceReleased(InputID source, int durationMs, bool shouldSustain) {
  juce::ScopedLock lock(queueLock);
  if (durationMs > 0) {
    ReleaseInfo info;
    info.releaseTimeMs = getCurrentTimeMs();
    info.durationMs = durationMs;
    info.shouldSustain = shouldSustain;
    releaseMap[source] = info;
  } else {
    // Duration is 0, cancel immediately (unless shouldSustain is true, then just remove from release map)
    if (!shouldSustain) {
      cancelPendingNotes(source);
    } else {
      releaseMap.erase(source);
    }
  }
}

void StrumEngine::cancelAll() {
  juce::ScopedLock lock(queueLock);
  noteQueue.clear();
  releaseMap.clear();
}

void StrumEngine::hiResTimerCallback() {
  juce::ScopedLock lock(queueLock);
  double now = getCurrentTimeMs();
  currentTimeMs = now;

  // Check for expired releases
  auto releaseIt = releaseMap.begin();
  while (releaseIt != releaseMap.end()) {
    const auto& releaseInfo = releaseIt->second;
    double expirationTime = releaseInfo.releaseTimeMs + releaseInfo.durationMs;
    
    if (now >= expirationTime) {
      // Release duration expired
      InputID source = releaseIt->first;
      if (!releaseInfo.shouldSustain) {
        // Normal mode: Cancel remaining notes
        noteQueue.erase(
          std::remove_if(noteQueue.begin(), noteQueue.end(),
            [source](const PendingNote& p) {
              return p.source.deviceHandle == source.deviceHandle && p.source.keyCode == source.keyCode;
            }),
          noteQueue.end()
        );
      }
      // Remove release info (for Sustain mode, notes continue playing naturally)
      releaseIt = releaseMap.erase(releaseIt);
    } else {
      ++releaseIt;
    }
  }

  // Process notes that are ready to play
  auto it = noteQueue.begin();
  while (it != noteQueue.end()) {
    // Check if this note's source is released and if we should skip it
    auto releaseIt = releaseMap.find(it->source);
    if (releaseIt != releaseMap.end()) {
      // Source is released - only play if note time has passed
      // (We continue playing notes that were scheduled before expiration)
      double expirationTime = releaseIt->second.releaseTimeMs + releaseIt->second.durationMs;
      if (it->targetTimeMs > expirationTime) {
        // This note is scheduled after release expiration
        if (!releaseIt->second.shouldSustain) {
          // Normal mode: Skip this note
          it = noteQueue.erase(it);
          continue;
        }
        // Sustain mode: Continue playing (don't cancel)
      }
    }

    if (it->targetTimeMs <= now) {
      midiEngine.sendNoteOn(it->channel, it->note, static_cast<float>(it->velocity) / 127.0f);
      if (onNotePlayed)
        onNotePlayed(it->source, it->note, it->channel, it->allowSustain);
      it = noteQueue.erase(it);
    } else {
      ++it;
    }
  }
}

double StrumEngine::getCurrentTimeMs() const {
  return juce::Time::getMillisecondCounterHiRes();
}
