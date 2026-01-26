#include "VoiceManager.h"
#include <algorithm>

VoiceManager::VoiceManager(MidiEngine &engine)
    : midiEngine(engine),
      strumEngine(engine, [this](InputID s, int n, int c, bool a) {
        addVoiceFromStrum(s, n, c, a);
      }) {
  startTimer(1); // Check for expired releases every 1ms
}

void VoiceManager::addVoiceFromStrum(InputID source, int note, int channel,
                                     bool allowSustain) {
  juce::ScopedLock lock(voicesLock);
  voices.push_back({note, channel, source, allowSustain, VoiceState::Playing, 0});
}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel,
                          bool allowSustain, int releaseMs) {
  {
    juce::ScopedLock rl(releasesLock);
    releaseQueue.erase(
      std::remove_if(releaseQueue.begin(), releaseQueue.end(),
        [note, channel](const PendingNoteOff& p) {
          return p.note == note && p.channel == channel;
        }),
      releaseQueue.end());
  }

  juce::ScopedLock lock(voicesLock);

  if (globalLatchActive) {
    auto it =
        std::find_if(voices.begin(), voices.end(), [&](const ActiveVoice &v) {
          return v.source == source && (v.state == VoiceState::Playing ||
                                        v.state == VoiceState::Latched);
        });
    if (it != voices.end()) {
      for (auto i = voices.begin(); i != voices.end();) {
        if (i->source.deviceHandle == source.deviceHandle &&
            i->source.keyCode == source.keyCode) {
          midiEngine.sendNoteOff(i->midiChannel, i->noteNumber);
          i = voices.erase(i);
        } else
          ++i;
      }
      return;
    }
  }

  midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
  voices.push_back({note, channel, source, allowSustain, VoiceState::Playing, releaseMs});
}

void VoiceManager::noteOn(InputID source, const std::vector<int> &notes,
                          const std::vector<int> &velocities, int channel,
                          int strumSpeedMs, bool allowSustain, int releaseMs) {
  if (notes.empty())
    return;

  if (strumSpeedMs == 0) {
    juce::ScopedLock rl(releasesLock);
    for (int n : notes) {
      releaseQueue.erase(
        std::remove_if(releaseQueue.begin(), releaseQueue.end(),
          [n, channel](const PendingNoteOff& p) {
            return p.note == n && p.channel == channel;
          }),
        releaseQueue.end());
    }
  }

  juce::ScopedLock lock(voicesLock);

  if (globalLatchActive) {
    bool anyFromSource =
        std::any_of(voices.begin(), voices.end(), [&](const ActiveVoice &v) {
          return v.source == source && (v.state == VoiceState::Playing ||
                                        v.state == VoiceState::Latched);
        });
    if (anyFromSource) {
      for (auto i = voices.begin(); i != voices.end();) {
        if (i->source == source) {
          midiEngine.sendNoteOff(i->midiChannel, i->noteNumber);
          i = voices.erase(i);
        } else
          ++i;
      }
      strumEngine.cancelPendingNotes(source);
      return;
    }
  }

  std::vector<int> finalVelocities = velocities;
  if (finalVelocities.size() < notes.size()) {
    int defaultVel = finalVelocities.empty() ? 100 : finalVelocities[0];
    finalVelocities.resize(notes.size(), defaultVel);
  }

  if (strumSpeedMs == 0) {
    for (size_t i = 0; i < notes.size(); ++i) {
      int vel = (i < finalVelocities.size()) ? finalVelocities[i] : 100;
      int note = notes[i];
      midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
      voices.push_back(
          {note, channel, source, allowSustain, VoiceState::Playing, releaseMs});
    }
  } else {
    strumEngine.triggerStrum(notes, finalVelocities, channel, strumSpeedMs,
                             source, allowSustain);
  }
}

void VoiceManager::strumNotes(const std::vector<int> &notes, int speedMs,
                              bool downstroke) {
  if (notes.empty())
    return;

  {
    juce::ScopedLock lock(voicesLock);
    for (const auto &v : voices) {
      midiEngine.sendNoteOff(v.midiChannel, v.noteNumber);
    }
    voices.clear();
  }

  std::vector<int> notesToStrum = notes;
  if (!downstroke)
    std::reverse(notesToStrum.begin(), notesToStrum.end());

  // Use default velocity for strumNotes (all notes same velocity)
  std::vector<int> velocities(notesToStrum.size(), 100);
  InputID dummySource = {0, 0};
  strumEngine.triggerStrum(notesToStrum, velocities, 1, speedMs, dummySource,
                           true);
}

void VoiceManager::handleKeyUp(InputID source) {
  strumEngine.cancelPendingNotes(source);

  std::vector<PendingNoteOff> toQueue;
  double now = getCurrentTimeMs();

  {
    juce::ScopedLock lock(voicesLock);
    for (auto it = voices.begin(); it != voices.end();) {
      if (it->source.deviceHandle != source.deviceHandle ||
          it->source.keyCode != source.keyCode) {
        ++it;
        continue;
      }
      if (globalLatchActive) {
        it->state = VoiceState::Latched;
        ++it;
      } else if (globalSustainActive && it->allowSustain) {
        it->state = VoiceState::Sustained;
        ++it;
      } else if (it->releaseMs > 0) {
        toQueue.push_back({it->noteNumber, it->midiChannel, now + it->releaseMs});
        it = voices.erase(it);
      } else {
        midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
        it = voices.erase(it);
      }
    }
  }

  if (!toQueue.empty()) {
    juce::ScopedLock lock(releasesLock);
    for (const auto& p : toQueue)
      releaseQueue.push_back(p);
  }
}

void VoiceManager::handleKeyUp(InputID source, int releaseDurationMs,
                               bool shouldSustain) {
  // Handle release with duration and behavior
  if (releaseDurationMs > 0) {
    // Mark source as released in strum engine (will continue for durationMs)
    strumEngine.markSourceReleased(source, releaseDurationMs, shouldSustain);

    // Track this release so we can send noteOff when duration expires (Normal
    // mode only)
    if (!shouldSustain) {
      // Normal mode: Track release to send noteOff after duration
      juce::ScopedLock lock(releasesLock);
      PendingRelease release;
      release.releaseTimeMs = getCurrentTimeMs();
      release.durationMs = releaseDurationMs;
      release.shouldSustain = false;
      pendingReleases[source] = release;
    }
    // Sustain mode: Don't send noteOff, let notes continue naturally
    // No need to track - notes will just continue playing
    return;
  } else {
    // Duration is 0, cancel immediately (same as normal handleKeyUp)
    handleKeyUp(source);
  }
}

void VoiceManager::hiResTimerCallback() {
  double now = getCurrentTimeMs();

  {
    juce::ScopedLock releasesLockGuard(releasesLock);

    auto it = releaseQueue.begin();
    while (it != releaseQueue.end()) {
      if (it->targetTimeMs <= now) {
        midiEngine.sendNoteOff(it->channel, it->note);
        it = releaseQueue.erase(it);
      } else {
        ++it;
      }
    }

    auto prIt = pendingReleases.begin();
    while (prIt != pendingReleases.end()) {
      const auto &release = prIt->second;
      double expirationTime = release.releaseTimeMs + release.durationMs;

      if (now >= expirationTime) {
        InputID source = prIt->first;
        prIt = pendingReleases.erase(prIt);

        juce::ScopedLock voicesLockGuard(voicesLock);
        for (auto voiceIt = voices.begin(); voiceIt != voices.end();) {
          if (voiceIt->source.deviceHandle == source.deviceHandle &&
              voiceIt->source.keyCode == source.keyCode) {
            if (globalLatchActive) {
              voiceIt->state = VoiceState::Latched;
              ++voiceIt;
            } else if (globalSustainActive && voiceIt->allowSustain) {
              voiceIt->state = VoiceState::Sustained;
              ++voiceIt;
            } else {
              midiEngine.sendNoteOff(voiceIt->midiChannel, voiceIt->noteNumber);
              voiceIt = voices.erase(voiceIt);
            }
          } else {
            ++voiceIt;
          }
        }
      } else {
        ++prIt;
      }
    }
  }
}

double VoiceManager::getCurrentTimeMs() const {
  return juce::Time::getMillisecondCounterHiRes();
}

void VoiceManager::setSustain(bool active) {
  juce::ScopedLock lock(voicesLock);
  bool wasActive = globalSustainActive;
  globalSustainActive = active;

  if (wasActive && !active) {
    for (auto it = voices.begin(); it != voices.end();) {
      if (it->state == VoiceState::Sustained) {
        midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
        it = voices.erase(it);
      } else
        ++it;
    }
  }
}

void VoiceManager::panic() {
  strumEngine.cancelAll();
  
  // 1. Manually kill every tracked note (Robust)
  {
    juce::ScopedLock lock(voicesLock);
    for (const auto& voice : voices) {
      // Send NoteOff for every voice (Playing, Sustained, Latched)
      midiEngine.sendNoteOff(voice.midiChannel, voice.noteNumber);
    }
    // 2. Clear Internal State
    voices.clear();
  }
  
  {
    juce::ScopedLock lock(releasesLock);
    pendingReleases.clear();
    releaseQueue.clear();
  }
  
  // 3. Reset Performance Flags
  globalSustainActive = false;
  globalLatchActive = false;
  
  // 4. Send MIDI Panic (Backup) - All Notes Off on all 16 channels
  for (int ch = 1; ch <= 16; ++ch) {
    midiEngine.sendCC(ch, 123, 0); // CC 123 = All Notes Off
  }
  
  DBG("VoiceManager: HARD PANIC Executed.");
}

void VoiceManager::panicLatch() {
  juce::ScopedLock lock(voicesLock);
  for (auto it = voices.begin(); it != voices.end();) {
    if (it->state == VoiceState::Latched) {
      midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
      it = voices.erase(it);
    } else
      ++it;
  }
}

void VoiceManager::resetPerformanceState() {
  globalSustainActive = false;
  globalLatchActive = false;
}

bool VoiceManager::isKeyLatched(int keyCode) const {
  juce::ScopedLock lock(voicesLock);
  for (const auto& voice : voices) {
    if (voice.source.keyCode == keyCode && voice.state == VoiceState::Latched) {
      return true;
    }
  }
  return false;
}

void VoiceManager::sendCC(int channel, int controller, int value) {
  midiEngine.sendCC(channel, controller, value);
}
