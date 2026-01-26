#pragma once
#include "MidiEngine.h"
#include <JuceHeader.h>

class PortamentoEngine : public juce::HighResolutionTimer {
public:
  explicit PortamentoEngine(MidiEngine& engine);
  ~PortamentoEngine() override;

  // Start a glide from startVal to endVal over durationMs milliseconds
  void startGlide(int startVal, int endVal, int durationMs, int channel);

  // Stop the current glide (reset PB to center)
  void stop();

  // Check if a glide is currently active
  bool isActive() const { return active; }

  // Get current Pitch Bend value (for smooth handoff in Legato mode)
  int getCurrentValue() const { return static_cast<int>(currentPbValue); }

  // HighResolutionTimer callback
  void hiResTimerCallback() override;

private:
  MidiEngine& midiEngine;
  double currentPbValue = 8192.0; // Current Pitch Bend value (0-16383)
  double targetPbValue = 8192.0;  // Target Pitch Bend value
  double step = 0.0;               // Step size per timer tick
  int midiChannel = 1;              // MIDI channel for this glide
  bool active = false;              // Is glide active?
  int lastSentValue = -1;            // Last sent PB value (for delta check)

  static constexpr double timerIntervalMs = 5.0; // 5ms timer interval

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PortamentoEngine)
};
