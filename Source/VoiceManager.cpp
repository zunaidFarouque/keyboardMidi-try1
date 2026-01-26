#include "VoiceManager.h"
#include <algorithm>

VoiceManager::VoiceManager(MidiEngine &engine, SettingsManager &settingsMgr)
    : midiEngine(engine), settingsManager(settingsMgr),
      strumEngine(engine, [this](InputID s, int n, int c,
                                 bool a) { addVoiceFromStrum(s, n, c, a); }),
      portamentoEngine(engine) {
  juce::HighResolutionTimer::startTimer(1); // Check for expired releases every 1ms
  juce::Timer::startTimer(100); // Watchdog for stuck notes every 100ms (Phase 26.6)

  // Register as listener to SettingsManager
  settingsManager.addChangeListener(this);

  // Initialize PB lookup table
  rebuildPbLookup();
}

VoiceManager::~VoiceManager() {
  // 1. Stop Watchdog (juce::Timer)
  juce::Timer::stopTimer();
  
  // 2. Stop Release Envelopes (juce::HighResolutionTimer)
  juce::HighResolutionTimer::stopTimer();
  
  // 3. Remove listeners
  settingsManager.removeChangeListener(this);
  
  // 4. Clear data
  {
    const juce::ScopedLock sl(monoCriticalSection);
    voices.clear();
  }
  
  // 5. Reset portamento
  portamentoEngine.stop(); // Reset PB to center
}

void VoiceManager::addVoiceFromStrum(InputID source, int note, int channel,
                                     bool allowSustain) {
  juce::ScopedLock lock(voicesLock);
  voices.push_back(
      {note, channel, source, allowSustain, VoiceState::Playing, 0, PolyphonyMode::Poly});
}

void VoiceManager::rebuildPbLookup() {
  int globalRange = settingsManager.getPitchBendRange();
  double stepsPerSemitone = 8192.0 / static_cast<double>(globalRange);

  // Fill lookup table: index = delta + 127 (so -127 maps to 0, +127 maps to
  // 254)
  for (int delta = -127; delta <= 127; ++delta) {
    int index = delta + 127;
    if (std::abs(delta) <= globalRange) {
      // Within range: calculate PB value
      int pbValue = static_cast<int>(8192.0 + (delta * stepsPerSemitone));
      pbLookup[index] = juce::jlimit(0, 16383, pbValue);
    } else {
      // Out of range: use sentinel
      pbLookup[index] = -1;
    }
  }
}

int VoiceManager::getCurrentPlayingNote(int channel) const {
  juce::ScopedLock lock(voicesLock);
  for (const auto &voice : voices) {
    if (voice.midiChannel == channel && voice.state == VoiceState::Playing) {
      return voice.noteNumber;
    }
  }
  return -1; // No note playing
}

void VoiceManager::pushToMonoStack(int channel, int note, InputID source) {
  juce::ScopedLock lock(monoStackLock);
  auto &stack = monoStacks[channel];
  // Remove any existing entry for this source
  stack.erase(std::remove_if(stack.begin(), stack.end(),
                             [source](const std::pair<int, InputID> &entry) {
                               return entry.second == source;
                             }),
              stack.end());
  // Push to back (last note priority)
  stack.push_back({note, source});
}

void VoiceManager::removeFromMonoStack(int channel, InputID source) {
  juce::ScopedLock lock(monoStackLock);
  auto it = monoStacks.find(channel);
  if (it != monoStacks.end()) {
    auto &stack = it->second;
    stack.erase(std::remove_if(stack.begin(), stack.end(),
                               [source](const std::pair<int, InputID> &entry) {
                                 return entry.second == source;
                               }),
                stack.end());
    // Remove empty stacks
    if (stack.empty()) {
      monoStacks.erase(it);
    }
  }
}

int VoiceManager::getMonoStackTop(int channel) const {
  juce::ScopedLock lock(monoStackLock);
  auto it = monoStacks.find(channel);
  if (it != monoStacks.end() && !it->second.empty()) {
    return it->second.back().first; // Last element (most recent)
  }
  return -1; // Stack empty
}

std::pair<int, InputID> VoiceManager::getMonoStackTopWithSource(int channel) const {
  juce::ScopedLock lock(monoStackLock);
  auto it = monoStacks.find(channel);
  if (it != monoStacks.end() && !it->second.empty()) {
    return it->second.back(); // Last element (most recent) - returns {note, source}
  }
  return {-1, {0, 0}}; // Stack empty
}

void VoiceManager::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &settingsManager) {
    // PB Range changed, rebuild lookup table
    rebuildPbLookup();
  }
}

void VoiceManager::noteOn(InputID source, int note, int vel, int channel,
                          bool allowSustain, int releaseMs,
                          PolyphonyMode polyMode, int glideSpeed) {
  {
    juce::ScopedLock rl(releasesLock);
    releaseQueue.erase(std::remove_if(releaseQueue.begin(), releaseQueue.end(),
                                      [note, channel](const PendingNoteOff &p) {
                                        return p.note == note &&
                                               p.channel == channel;
                                      }),
                       releaseQueue.end());
  }

  // Handle Mono/Legato modes
  if (polyMode == PolyphonyMode::Mono || polyMode == PolyphonyMode::Legato) {
    const juce::ScopedLock monoLock(monoCriticalSection); // Phase 26.5: Thread safety
    
    // Remember mode + glide per channel so handleKeyUp can reactivate previous notes
    channelPolyModes[channel] = { polyMode, glideSpeed };

    // Zombie Check (Phase 26.5): If stack is empty but a voice is still active, kill it
    {
      juce::ScopedLock stackLock(monoStackLock);
      auto stackIt = monoStacks.find(channel);
      bool stackEmpty = (stackIt == monoStacks.end() || stackIt->second.empty());
      
      if (stackEmpty) {
        // Check for zombie voices on this channel
        juce::ScopedLock lock(voicesLock);
        for (auto it = voices.begin(); it != voices.end();) {
          if (it->midiChannel == channel) {
            // Zombie voice found - kill it (self-healing)
            midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
            it = voices.erase(it);
          } else {
            ++it;
          }
        }
        // Reset PB and stop portamento for clean state
        portamentoEngine.stop();
        midiEngine.sendPitchBend(channel, 8192);
      }
    }

    pushToMonoStack(channel, note, source);

    int currentNote = getCurrentPlayingNote(channel);

    if (currentNote >= 0 && currentNote != note) {
      // Calculate semitone delta
      int delta = note - currentNote;
      int lookupIndex = delta + 127;

      // Check if Legato glide is possible
      if (polyMode == PolyphonyMode::Legato && lookupIndex >= 0 &&
          lookupIndex < 255 && pbLookup[lookupIndex] != -1) {
        // Legato glide: use portamento
        // Start from current PB value if portamento is active, otherwise center (8192)
        int startPB = portamentoEngine.isActive() ? portamentoEngine.getCurrentValue() : 8192;
        int targetPB = pbLookup[lookupIndex];
        portamentoEngine.startGlide(startPB, targetPB, glideSpeed, channel);
        // Do NOT send NoteOff/NoteOn - just glide the PB
        return; // Exit early, no new note triggered
      } else {
        // Retrigger: NoteOff current, reset PB, NoteOn new
        juce::ScopedLock lock(voicesLock);
        for (auto it = voices.begin(); it != voices.end();) {
          if (it->midiChannel == channel && it->noteNumber == currentNote) {
            midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
            it = voices.erase(it);
          } else {
            ++it;
          }
        }
        // Reset PB to center
        portamentoEngine.stop();
        midiEngine.sendPitchBend(channel, 8192);
      }
    } else if (currentNote < 0) {
      // No note currently playing, reset PB to center
      portamentoEngine.stop();
      midiEngine.sendPitchBend(channel, 8192);
    }
  } else {
    // Poly: clear any mono/legato tracking for this channel
    channelPolyModes.erase(channel);
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
  voices.push_back(
      {note, channel, source, allowSustain, VoiceState::Playing, releaseMs, PolyphonyMode::Poly});
}

void VoiceManager::noteOn(InputID source, const std::vector<int> &notes,
                          const std::vector<int> &velocities, int channel,
                          int strumSpeedMs, bool allowSustain, int releaseMs,
                          PolyphonyMode polyMode, int glideSpeed) {
  if (notes.empty())
    return;

  if (strumSpeedMs == 0) {
    juce::ScopedLock rl(releasesLock);
    for (int n : notes) {
      releaseQueue.erase(
          std::remove_if(releaseQueue.begin(), releaseQueue.end(),
                         [n, channel](const PendingNoteOff &p) {
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
      voices.push_back({note, channel, source, allowSustain,
                        VoiceState::Playing, releaseMs, polyMode});
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

  int releasedChannel = -1;
  int releasedNote = -1;
  PolyphonyMode releasedPolyMode = PolyphonyMode::Poly;
  bool legatoVoicePreserved = false; // Track if Legato voice was preserved (Phase 26.4)

  {
    juce::ScopedLock lock(voicesLock);
    for (auto it = voices.begin(); it != voices.end();) {
      if (it->source.deviceHandle != source.deviceHandle ||
          it->source.keyCode != source.keyCode) {
        ++it;
        continue;
      }

      releasedChannel = it->midiChannel;
      releasedNote = it->noteNumber;
      releasedPolyMode = it->polyphonyMode; // Store polyphony mode (Phase 26.3)

      if (globalLatchActive) {
        it->state = VoiceState::Latched;
        ++it;
      } else if (globalSustainActive && it->allowSustain) {
        it->state = VoiceState::Sustained;
        ++it;
      } else if (it->releaseMs > 0) {
        toQueue.push_back(
            {it->noteNumber, it->midiChannel, now + it->releaseMs});
        it = voices.erase(it);
      } else {
        // Check if this is a Legato anchor that should be preserved
        bool shouldPreserveVoice = false;
        if (releasedPolyMode == PolyphonyMode::Legato) {
          juce::ScopedLock monoLock(monoStackLock);
          auto stackIt = monoStacks.find(releasedChannel);
          if (stackIt != monoStacks.end()) {
            auto &stack = stackIt->second;
            // Check if source is in the stack
            bool foundInStack = false;
            for (const auto &entry : stack) {
              if (entry.second == source) {
                foundInStack = true;
                break;
              }
            }
            
            if (foundInStack) {
              // Remove source from stack
              stack.erase(std::remove_if(stack.begin(), stack.end(),
                                         [source](const std::pair<int, InputID> &entry) {
                                           return entry.second == source;
                                         }),
                          stack.end());
              
              // Check if stack is still not empty
              if (!stack.empty()) {
                // Stack not empty: preserve anchor voice, glide PB to new top note
                shouldPreserveVoice = true;
                legatoVoicePreserved = true; // Mark that we preserved it
                
                // Get new top note from stack
                auto newTopEntry = getMonoStackTopWithSource(releasedChannel);
                int newTopNote = newTopEntry.first;
                
                if (newTopNote >= 0 && newTopNote != releasedNote) {
                  // Calculate delta for glide
                  int delta = newTopNote - releasedNote;
                  int lookupIndex = delta + 127;
                  
                  if (lookupIndex >= 0 && lookupIndex < 255 && pbLookup[lookupIndex] != -1) {
                    // Glide PB to new top note
                    int startPB = portamentoEngine.getCurrentValue();
                    int targetPB = pbLookup[lookupIndex];
                    auto polyModeIt = channelPolyModes.find(releasedChannel);
                    int glideSpeed = (polyModeIt != channelPolyModes.end()) ? polyModeIt->second.second : 50;
                    if (glideSpeed < 1) glideSpeed = 50;
                    
                    if (std::abs(startPB - targetPB) > 1) {
                      portamentoEngine.startGlide(startPB, targetPB, glideSpeed, releasedChannel);
                    }
                  }
                }
              } else {
                // Stack is now empty - will kill voice below
                monoStacks.erase(stackIt);
              }
            }
          }
        }
        
        if (!shouldPreserveVoice) {
          // Standard release: send NoteOff and remove voice
          midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
          it = voices.erase(it);
        } else {
          // Preserve voice (Legato anchor with non-empty stack)
          ++it;
        }
      }
    }
  }

  // If this key had no active voice, check if it's in a mono stack
  // This handles both background key releases AND Legato mode (where new notes
  // aren't added to voices, only PB is glided)
  if (releasedChannel < 0) {
    juce::ScopedLock monoLock(monoStackLock);
    int foundChannel = -1;
    for (auto it = monoStacks.begin(); it != monoStacks.end();) {
      auto &stack = it->second;
      auto beforeSize = stack.size();
      // Check if this source is in the stack
      bool foundInStack = false;
      for (const auto &entry : stack) {
        if (entry.second == source) {
          foundInStack = true;
          foundChannel = it->first;
          break;
        }
      }
      
      if (foundInStack) {
        // Remove from stack
        stack.erase(std::remove_if(stack.begin(), stack.end(),
                                   [source](const std::pair<int, InputID> &entry) {
                                     return entry.second == source;
                                   }),
                    stack.end());
        if (stack.empty()) {
          it = monoStacks.erase(it);
        } else {
          ++it;
        }
        
        // If this is a Legato mode channel, we need to handle the glide back
        // even though there's no active voice
        if (foundChannel >= 0) {
          auto polyModeIt = channelPolyModes.find(foundChannel);
          if (polyModeIt != channelPolyModes.end()) {
            PolyphonyMode polyMode = polyModeIt->second.first;
            if (polyMode == PolyphonyMode::Legato) {
              // This was a Legato glide note release - handle glide back
              auto previousEntry = getMonoStackTopWithSource(foundChannel);
              int previousNote = previousEntry.first;
              
              if (previousNote >= 0) {
                // There's a previous note - glide back to it
                int currentNote = getCurrentPlayingNote(foundChannel);
                if (currentNote >= 0) {
                  // Calculate delta from current playing note to previous note
                  // This tells us how many semitones to glide back
                  int delta = previousNote - currentNote;
                  int lookupIndex = delta + 127;
                  
                  if (lookupIndex >= 0 && lookupIndex < 255 && pbLookup[lookupIndex] != -1) {
                    // Legato glide back to previous note's PB value
                    // Always use getCurrentValue() - it holds the last PB value even when inactive
                    int startPB = portamentoEngine.getCurrentValue();
                    // Target is the PB value for the previous note (relative to current note)
                    int targetPB = pbLookup[lookupIndex];
                    int returnGlideSpeed = polyModeIt->second.second; // Use stored glide speed
                    
                    // Ensure we have a valid glide speed (at least 1ms)
                    if (returnGlideSpeed < 1) {
                      returnGlideSpeed = 50; // Default fallback
                    }
                    
                    // Only start glide if we're not already at the target
                    if (std::abs(startPB - targetPB) > 1) {
                      // Start the glide back
                      portamentoEngine.startGlide(startPB, targetPB, returnGlideSpeed, foundChannel);
                    } else {
                      // Already at target, just ensure it's set
                      if (!portamentoEngine.isActive()) {
                        midiEngine.sendPitchBend(foundChannel, targetPB);
                      }
                    }
                  } else {
                    // Out of range or invalid, just reset to center
                    portamentoEngine.stop();
                    midiEngine.sendPitchBend(foundChannel, 8192);
                  }
                } else {
                  // No note playing, reset PB
                  portamentoEngine.stop();
                  midiEngine.sendPitchBend(foundChannel, 8192);
                }
              } else {
                // Stack empty, reset PB
                portamentoEngine.stop();
                midiEngine.sendPitchBend(foundChannel, 8192);
                channelPolyModes.erase(polyModeIt);
              }
            }
          }
        }
        break; // Found and handled, exit loop
      } else {
        ++it;
      }
    }
    
    // If we didn't find it in any stack, we're done
    if (foundChannel < 0) {
      return; // Not in any mono stack, nothing to do
    }
  }

  // Handle Mono/Legato stack if this was a mono/legato voice (Phase 26.4)
  // Unified decision tree for both Mono and Legato modes
  // Note: This handles the case where a voice was released (releasedChannel >= 0)
  // For Legato, skip if the voice was already preserved above (legatoVoicePreserved = true)
  if (releasedChannel >= 0 && !legatoVoicePreserved) {
    const juce::ScopedLock monoLock(monoCriticalSection); // Phase 26.5: Thread safety
    
    auto polyModeIt = channelPolyModes.find(releasedChannel);
    if (polyModeIt != channelPolyModes.end()) {
      PolyphonyMode polyMode = polyModeIt->second.first;
      
      if (polyMode == PolyphonyMode::Mono || polyMode == PolyphonyMode::Legato) {
        // Remove source from stack
        removeFromMonoStack(releasedChannel, source);
        
        // Get stack status after removal
        auto topEntry = getMonoStackTopWithSource(releasedChannel);
        int targetNote = topEntry.first;
        InputID targetSource = topEntry.second;
        
        // CASE 1: Stack is empty (Final Release) - Hard Stop (Phase 26.5)
        if (targetNote < 0) {
          // Force-kill ANY voice on this channel (don't trust the ID)
          juce::ScopedLock lock(voicesLock);
          for (auto it = voices.begin(); it != voices.end();) {
            if (it->midiChannel == releasedChannel) {
              midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
              it = voices.erase(it);
            } else {
              ++it;
            }
          }
          // Reset PB and force-stop portamento engine
          portamentoEngine.stop();
          midiEngine.sendPitchBend(releasedChannel, 8192);
          channelPolyModes.erase(polyModeIt);
        }
        // CASE 2: Stack is not empty (Handoff / Retrigger)
        else {
          // Find the "Anchor" (The voice actually playing audio on this channel)
          ActiveVoice* anchor = nullptr;
          {
            juce::ScopedLock lock(voicesLock);
            for (auto& v : voices) {
              if (v.midiChannel == releasedChannel && v.state == VoiceState::Playing) {
                anchor = &v;
                break;
              }
            }
          }
          
          // Sub-Case A: No Anchor exists (was killed by a non-legato note or out-of-range retrigger)
          if (anchor == nullptr) {
            // We must RETRIGGER the target note
            midiEngine.sendNoteOn(releasedChannel, targetNote, 100.0f / 127.0f);
            juce::ScopedLock lock(voicesLock);
            voices.push_back({targetNote, releasedChannel, targetSource,
                              true, VoiceState::Playing, 0, polyMode});
            // Reset PB to center for the new note
            portamentoEngine.stop();
            midiEngine.sendPitchBend(releasedChannel, 8192);
          }
          // Sub-Case B: Anchor exists
          else {
            int currentRoot = anchor->noteNumber;
            int delta = targetNote - currentRoot;
            int lookupIndex = delta + 127;
            
            // Check PB Range (using lookup)
            if (lookupIndex >= 0 && lookupIndex < 255 && pbLookup[lookupIndex] != -1) {
              // GLIDE (Ghost Anchor) - Keep Anchor alive, just move PB
              int startPB = portamentoEngine.getCurrentValue();
              int targetPB = pbLookup[lookupIndex];
              int glideSpeed = polyModeIt->second.second;
              if (glideSpeed < 1) glideSpeed = 50;
              
              if (std::abs(startPB - targetPB) > 1) {
                portamentoEngine.startGlide(startPB, targetPB, glideSpeed, releasedChannel);
              }
            } else {
              // HARD SWITCH (Retrigger) - Range too far. Kill Anchor. Start Target.
              juce::ScopedLock lock(voicesLock);
              for (auto it = voices.begin(); it != voices.end();) {
                if (it->midiChannel == releasedChannel && it->noteNumber == currentRoot) {
                  midiEngine.sendNoteOff(it->midiChannel, it->noteNumber);
                  it = voices.erase(it);
                } else {
                  ++it;
                }
              }
              // Trigger target note
              midiEngine.sendNoteOn(releasedChannel, targetNote, 100.0f / 127.0f);
              voices.push_back({targetNote, releasedChannel, targetSource,
                                true, VoiceState::Playing, 0, polyMode});
              // Reset PB to center
              portamentoEngine.stop();
              midiEngine.sendPitchBend(releasedChannel, 8192);
            }
          }
        }
      }
    }
  }

  if (!toQueue.empty()) {
    juce::ScopedLock lock(releasesLock);
    for (const auto &p : toQueue)
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

void VoiceManager::timerCallback() {
  // Watchdog Timer: Check for stuck notes (Phase 26.6)
  // Try to lock. If the Audio thread is busy processing a note,
  // skip this check to avoid blocking audio. Efficiency first.
  juce::ScopedTryLock tryLock(monoCriticalSection);
  if (tryLock.isLocked()) {
    juce::ScopedLock stackLock(monoStackLock);
    
    // 1. Iterate over all active voices
    juce::ScopedLock lock(voicesLock);
    for (auto it = voices.begin(); it != voices.end();) {
      // Only care about Mono/Legato voices (Poly handles itself)
      // If Stack is tracked for this channel, we use Stack logic.
      
      int ch = it->midiChannel;
      bool channelHasStack = (monoStacks.count(ch) > 0);
      
      if (channelHasStack) {
        bool stackEmpty = monoStacks[ch].empty();
        bool sustainActive = globalSustainActive && it->allowSustain;
        bool latched = (it->state == VoiceState::Latched);

        // ZOMBIE CONDITION:
        // Stack is Empty AND Not Sustained AND Not Latched.
        if (stackEmpty && !sustainActive && !latched) {
          // Kill it.
          midiEngine.sendNoteOff(ch, it->noteNumber);
          midiEngine.sendPitchBend(ch, 8192); // Reset PB
          
          // Force stop glide if on this channel
          // Check if portamento is active and on this channel
          if (portamentoEngine.isActive()) {
            portamentoEngine.stop();
          }

          // Remove from vector
          it = voices.erase(it);
          continue; // Loop continues
        }
      }
      
      ++it;
    }
  }
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

  // Phase 26.5: Lock mono critical section for state integrity
  const juce::ScopedLock monoLock(monoCriticalSection);

  // 1. Manually kill every tracked note (Robust)
  {
    juce::ScopedLock lock(voicesLock);
    for (const auto &voice : voices) {
      // Send NoteOff for every voice (Playing, Sustained, Latched)
      midiEngine.sendNoteOff(voice.midiChannel, voice.noteNumber);
    }
    // 2. Clear Internal State
    voices.clear();
  }

  // Phase 26.5: Stop portamento engine and reset PB on all channels
  portamentoEngine.stop();
  for (int ch = 1; ch <= 16; ++ch) {
    midiEngine.sendPitchBend(ch, 8192);
  }

  // Phase 26.5: Clear Mono/Legato state
  {
    juce::ScopedLock stackLock(monoStackLock);
    monoStacks.clear();
  }
  channelPolyModes.clear();

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
  for (const auto &voice : voices) {
    if (voice.source.keyCode == keyCode && voice.state == VoiceState::Latched) {
      return true;
    }
  }
  return false;
}

void VoiceManager::sendCC(int channel, int controller, int value) {
  midiEngine.sendCC(channel, controller, value);
}
