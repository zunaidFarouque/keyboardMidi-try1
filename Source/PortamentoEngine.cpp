#include "PortamentoEngine.h"

PortamentoEngine::PortamentoEngine(MidiEngine& engine)
    : midiEngine(engine) {
  startTimer(static_cast<int>(timerIntervalMs * 1000.0)); // Convert to microseconds
}

PortamentoEngine::~PortamentoEngine() {
  stopTimer();
  stop(); // Reset PB to center when destroyed
}

void PortamentoEngine::startGlide(int startVal, int endVal, int durationMs, int channel) {
  currentPbValue = static_cast<double>(juce::jlimit(0, 16383, startVal));
  targetPbValue = static_cast<double>(juce::jlimit(0, 16383, endVal));
  midiChannel = channel;
  active = true;
  lastSentValue = -1;

  // Calculate step size: how much to change per timer tick
  if (durationMs <= 0) {
    // Instant: set immediately
    currentPbValue = targetPbValue;
    step = 0.0;
    active = false;
    midiEngine.sendPitchBend(midiChannel, static_cast<int>(currentPbValue));
    lastSentValue = static_cast<int>(currentPbValue);
  } else {
    // Calculate steps needed
    double totalSteps = durationMs / timerIntervalMs;
    if (totalSteps <= 0.0) {
      totalSteps = 1.0;
    }
    step = (targetPbValue - currentPbValue) / totalSteps;
  }
}

void PortamentoEngine::stop() {
  if (active) {
    // Reset to center (8192)
    midiEngine.sendPitchBend(midiChannel, 8192);
    lastSentValue = 8192;
  }
  active = false;
  currentPbValue = 8192.0;
  targetPbValue = 8192.0;
  step = 0.0;
}

void PortamentoEngine::hiResTimerCallback() {
  if (!active)
    return;

  // Update current value
  if (std::abs(targetPbValue - currentPbValue) < std::abs(step)) {
    // Reached target
    currentPbValue = targetPbValue;
    active = false;
  } else {
    currentPbValue += step;
  }

  // Clamp to valid range
  int outputVal = static_cast<int>(juce::jlimit(0.0, 16383.0, currentPbValue));

  // Delta check: only send if value changed
  if (outputVal != lastSentValue) {
    midiEngine.sendPitchBend(midiChannel, outputVal);
    lastSentValue = outputVal;
  }
}
