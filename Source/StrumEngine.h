#pragma once
#include "MidiEngine.h"
#include "MappingTypes.h"
#include <JuceHeader.h>
#include <vector>

class StrumEngine : public juce::HighResolutionTimer {
public:
  struct PendingNote {
    int note;
    int velocity;
    int channel;
    double targetTimeMs;
    InputID source; // Track source to allow cancellation
  };

  explicit StrumEngine(MidiEngine& engine);
  ~StrumEngine() override;

  // Trigger a strum with multiple notes
  // speedMs: delay between each note in milliseconds
  void triggerStrum(const std::vector<int>& notes, 
                    int velocity, 
                    int channel, 
                    int speedMs,
                    InputID source);

  // Cancel all pending notes for a given source (prevents ghost strums)
  void cancelPendingNotes(InputID source);

  // HighResolutionTimer callback
  void hiResTimerCallback() override;

private:
  MidiEngine& midiEngine;
  std::vector<PendingNote> noteQueue;
  juce::CriticalSection queueLock;
  double currentTimeMs = 0.0;
  
  // Get current time in milliseconds
  double getCurrentTimeMs() const;
};
