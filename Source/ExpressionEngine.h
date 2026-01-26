#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include <JuceHeader.h>
#include <vector>

class ExpressionEngine : public juce::HighResolutionTimer {
public:
  explicit ExpressionEngine(MidiEngine& engine);
  ~ExpressionEngine() override;

  // Trigger an envelope (key press)
  void triggerEnvelope(InputID source, int channel, const AdsrSettings& settings, int peakValue);

  // Release an envelope (key release)
  void releaseEnvelope(InputID source);

  // HighResolutionTimer callback
  void hiResTimerCallback() override;

private:
  enum class Stage { Attack, Decay, Sustain, Release, Finished };

  struct ActiveEnvelope {
    InputID source;
    int channel;
    AdsrSettings settings;
    int peakValue; // Target value (e.g., 127 for CC, 16383 for Pitch Bend)
    Stage stage = Stage::Attack;
    double currentLevel = 0.0; // 0.0-1.0
    double stageProgress = 0.0; // 0.0-1.0 (for progress tracking)
    double stepSize = 0.0; // Step per timer tick
    int lastSentValue = -1; // Last MIDI value sent (for delta check optimization)
  };

  MidiEngine& midiEngine;
  std::vector<ActiveEnvelope> activeEnvelopes;
  juce::CriticalSection lock;

  static constexpr double timerIntervalMs = 5.0; // 5ms = 200Hz
};
