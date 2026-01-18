#pragma once
#include "MidiEngine.h"
#include "MappingTypes.h"
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <unordered_map>

class StrumEngine : public juce::HighResolutionTimer {
public:
  struct PendingNote {
    int note;
    int velocity;
    int channel;
    double targetTimeMs;
    InputID source;
    bool allowSustain;
  };

  using OnNotePlayedCallback = std::function<void(InputID source, int note, int channel, bool allowSustain)>;

  StrumEngine(MidiEngine& engine, OnNotePlayedCallback onPlayed);
  ~StrumEngine() override;

  // Trigger a strum with multiple notes (with per-note velocities)
  void triggerStrum(const std::vector<int>& notes, const std::vector<int>& velocities, int channel,
                    int speedMs, InputID source, bool allowSustain = true);

  // Cancel all pending notes for a given source
  void cancelPendingNotes(InputID source);

  // Mark source as released - notes will continue for durationMs, then cancel or sustain
  void markSourceReleased(InputID source, int durationMs, bool shouldSustain);

  // Clear entire queue (e.g. for panic)
  void cancelAll();

  // HighResolutionTimer callback
  void hiResTimerCallback() override;

private:
  struct ReleaseInfo {
    double releaseTimeMs;
    int durationMs;
    bool shouldSustain; // If true, don't cancel notes after duration, let them sustain
  };

  MidiEngine& midiEngine;
  OnNotePlayedCallback onNotePlayed;
  std::vector<PendingNote> noteQueue;
  std::unordered_map<InputID, ReleaseInfo> releaseMap; // Track release state per source
  juce::CriticalSection queueLock;
  double currentTimeMs = 0.0;
  double getCurrentTimeMs() const;
};
