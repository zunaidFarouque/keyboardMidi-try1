#include "VoiceManager.h"
#include <algorithm>

VoiceManager::VoiceManager(MidiEngine& engine)
  : midiEngine(engine),
    strumEngine(engine, [this](InputID s, int n, int c, bool a) { addVoiceFromStrum(s, n, c, a); }) {
  startTimer(1); // Check for expired releases every 1ms
}

void VoiceManager::addVoiceFromStrum(InputID source, int note, int channel, bool allowSustain) {
  juce::ScopedLock lock(voicesLock);
  voices.push_back({note, channel, source, allowSustain, VoiceState::Playing});
}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel, bool allowSustain) {
  juce::ScopedLock lock(voicesLock);

  if (globalLatchActive) {
    auto it = std::find_if(voices.begin(), voices.end(), [&](const ActiveVoice& v) {
      return v.source == source && (v.state == VoiceState::Playing || v.state == VoiceState::Latched);
    });
    if (it != voices.end()) {
      for (auto i = voices.begin(); i != voices.end();) {
        if (i->source.deviceHandle == source.deviceHandle && i->source.keyCode == source.keyCode) {
          midiEngine.sendNoteOff(i->midiChannel, i->noteNumber);
          i = voices.erase(i);
        } else
          ++i;
      }
      return;
    }
  }

  midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
  voices.push_back({note, channel, source, allowSustain, VoiceState::Playing});
}

void VoiceManager::noteOn(InputID source, const std::vector<int>& notes, int vel, int channel, int strumSpeedMs, bool allowSustain) {
  if (notes.empty())
    return;

  juce::ScopedLock lock(voicesLock);

  if (globalLatchActive) {
    bool anyFromSource = std::any_of(voices.begin(), voices.end(), [&](const ActiveVoice& v) {
      return v.source == source && (v.state == VoiceState::Playing || v.state == VoiceState::Latched);
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

  if (strumSpeedMs == 0) {
    juce::ScopedLock l2(voicesLock);
    for (int note : notes) {
      midiEngine.sendNoteOn(channel, note, static_cast<float>(vel) / 127.0f);
      voices.push_back({note, channel, source, allowSustain, VoiceState::Playing});
    }
  } else {
    strumEngine.triggerStrum(notes, vel, channel, strumSpeedMs, source, allowSustain);
  }
}

void VoiceManager::strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke) {
  if (notes.empty())
    return;

  {
    juce::ScopedLock lock(voicesLock);
    for (const auto& v : voices) {
      midiEngine.sendNoteOff(v.midiChannel, v.noteNumber);
    }
    voices.clear();
  }

  std::vector<int> notesToStrum = notes;
  if (!downstroke)
    std::reverse(notesToStrum.begin(), notesToStrum.end());

  InputID dummySource = {0, 0};
  strumEngine.triggerStrum(notesToStrum, 100, 1, speedMs, dummySource, true);
}

void VoiceManager::handleKeyUp(InputID source) {
  // Default: immediate cancel
  strumEngine.cancelPendingNotes(source);

  juce::ScopedLock lock(voicesLock);
  for (auto it = voices.begin(); it != voices.end();) {
    if (it->source.deviceHandle != source.deviceHandle || it->source.keyCode != source.keyCode) {
      ++it;
      continue;
    }
    if (globalLatchActive) {
      it->state = VoiceState::Latched;
      ++it;
    } else if (globalSustainActive && it->allowSustain) {
      it->state = VoiceState::Sustained;
      ++it;
    } else {
      midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
      it = voices.erase(it);
    }
  }
}

void VoiceManager::handleKeyUp(InputID source, int releaseDurationMs, bool shouldSustain) {
  // Handle release with duration and behavior
  if (releaseDurationMs > 0) {
    // Mark source as released in strum engine (will continue for durationMs)
    strumEngine.markSourceReleased(source, releaseDurationMs, shouldSustain);
    
    // Track this release so we can send noteOff when duration expires (Normal mode only)
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
  // Check for expired releases and send noteOff in Normal mode
  double now = getCurrentTimeMs();
  
  juce::ScopedLock releasesLockGuard(releasesLock);
  auto it = pendingReleases.begin();
  while (it != pendingReleases.end()) {
    const auto& release = it->second;
    double expirationTime = release.releaseTimeMs + release.durationMs;
    
    if (now >= expirationTime) {
      // Release duration expired - send noteOff to all voices from this source (Normal mode)
      InputID source = it->first;
      
      juce::ScopedLock voicesLockGuard(voicesLock);
      for (auto voiceIt = voices.begin(); voiceIt != voices.end();) {
        if (voiceIt->source.deviceHandle == source.deviceHandle && 
            voiceIt->source.keyCode == source.keyCode) {
          // Check if voice should be released (not latched, not sustained)
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
      
      it = pendingReleases.erase(it);
    } else {
      ++it;
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
  {
    juce::ScopedLock lock(voicesLock);
    voices.clear();
  }
  {
    juce::ScopedLock lock(releasesLock);
    pendingReleases.clear();
  }
  midiEngine.allNotesOff();
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

void VoiceManager::sendCC(int channel, int controller, int value) {
  midiEngine.sendCC(channel, controller, value);
}
