#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include <JuceHeader.h>
#include <map>
#include <vector>

class ExpressionEngine : public juce::HighResolutionTimer {
public:
  explicit ExpressionEngine(MidiEngine &engine);
  ~ExpressionEngine() override;

  // Trigger an envelope (key press)
  void triggerEnvelope(InputID source, int channel,
                       const AdsrSettings &settings, int peakValue);

  // Release an envelope (key release)
  void releaseEnvelope(InputID source);

  // HighResolutionTimer callback
  void hiResTimerCallback() override;

  // Run one timer tick (same logic as hiResTimerCallback). For
  // benchmarks/tests.
  void processOneTick();

private:
  enum class Stage { Attack, Decay, Sustain, Release, Finished };

  struct ActiveEnvelope {
    InputID source;
    int channel;
    AdsrSettings settings;
    int peakValue; // Target value (e.g., 127 for CC, 16383 for Pitch Bend)
    int dynamicStartValue = -1; // Value at Level 0.0 (Phase 23.7)
    bool isDormant =
        false; // If true, envelope exists but does not send MIDI (Phase 23.7)
    Stage stage = Stage::Attack;
    double currentLevel = 0.0;  // 0.0-1.0
    double stageProgress = 0.0; // 0.0-1.0 (for progress tracking)
    double stepSize = 0.0;      // Step per timer tick
    int lastSentValue =
        -1; // Last MIDI value sent (for delta check optimization)
  };

  MidiEngine &midiEngine;
  std::vector<ActiveEnvelope> activeEnvelopes;
  juce::CriticalSection lock;

  // Pitch Bend Priority Stack (per-channel, LIFO) (Phase 23.7)
  std::map<int, std::vector<InputID>> pitchBendStacks; // channel -> stack
  int currentPitchBendValues[17] = {
      0}; // channel 1-16, cached output (default 8192)

  static constexpr double timerIntervalMs = 5.0; // 5ms = 200Hz
};
