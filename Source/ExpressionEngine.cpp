#include "ExpressionEngine.h"
#include "MappingTypes.h"
#include <algorithm>

ExpressionEngine::ExpressionEngine(MidiEngine& engine)
    : midiEngine(engine) {
  startTimer(static_cast<int>(timerIntervalMs));
}

ExpressionEngine::~ExpressionEngine() {
  stopTimer();
}

void ExpressionEngine::triggerEnvelope(InputID source, int channel, const AdsrSettings& settings, int peakValue) {
  juce::ScopedLock scopedLock(lock);

  // Remove any existing envelope for this source (voice stealing)
  activeEnvelopes.erase(
    std::remove_if(activeEnvelopes.begin(), activeEnvelopes.end(),
      [source](const ActiveEnvelope& env) {
        return env.source == source;
      }),
    activeEnvelopes.end());

  // Create new envelope
  ActiveEnvelope env;
  env.source = source;
  env.channel = channel;
  env.settings = settings;
  env.peakValue = peakValue;
  env.stage = Stage::Attack;
  env.currentLevel = 0.0;
  env.stageProgress = 0.0;
  env.lastSentValue = -1; // Initialize to -1 so first value (0) is always sent

  // Calculate step size for Attack: from 0.0 to 1.0 in attackMs
  if (settings.attackMs > 0) {
    double numSteps = settings.attackMs / timerIntervalMs;
    env.stepSize = 1.0 / numSteps;
  } else {
    // Instant attack
    env.stepSize = 1.0;
    env.currentLevel = 1.0;
    env.stage = Stage::Decay;
  }

  activeEnvelopes.push_back(env);
}

void ExpressionEngine::releaseEnvelope(InputID source) {
  juce::ScopedLock scopedLock(lock);

  for (auto& env : activeEnvelopes) {
    if (env.source == source && env.stage != Stage::Finished) {
      // Transition to Release stage
      env.stage = Stage::Release;

      // Calculate step size for Release: from currentLevel to 0.0 in releaseMs
      if (env.settings.releaseMs > 0 && env.currentLevel > 0.0) {
        double numSteps = env.settings.releaseMs / timerIntervalMs;
        env.stepSize = env.currentLevel / numSteps;
      } else {
        // Instant release
        env.stepSize = env.currentLevel;
      }
      break;
    }
  }
}

void ExpressionEngine::hiResTimerCallback() {
  juce::ScopedLock scopedLock(lock);

  for (auto& env : activeEnvelopes) {
    // Update level based on current stage
    switch (env.stage) {
      case Stage::Attack: {
        env.currentLevel += env.stepSize;
        if (env.currentLevel >= 1.0) {
          env.currentLevel = 1.0;
          // Transition to Decay
          env.stage = Stage::Decay;
          if (env.settings.decayMs > 0) {
            double numSteps = env.settings.decayMs / timerIntervalMs;
            double levelRange = 1.0 - env.settings.sustainLevel;
            env.stepSize = levelRange / numSteps;
          } else {
            // Instant decay
            env.currentLevel = env.settings.sustainLevel;
            env.stage = Stage::Sustain;
          }
        }
        break;
      }

      case Stage::Decay: {
        env.currentLevel -= env.stepSize;
        if (env.currentLevel <= env.settings.sustainLevel) {
          env.currentLevel = env.settings.sustainLevel;
          // Transition to Sustain
          env.stage = Stage::Sustain;
          env.stepSize = 0.0; // No change in Sustain
        }
        break;
      }

      case Stage::Sustain: {
        // Hold sustain level (no change)
        env.currentLevel = env.settings.sustainLevel;
        break;
      }

      case Stage::Release: {
        env.currentLevel -= env.stepSize;
        if (env.currentLevel <= 0.0) {
          env.currentLevel = 0.0;
          env.stage = Stage::Finished;
        }
        break;
      }

      case Stage::Finished:
        // Already finished, will be removed below
        break;
    }

    // Calculate output value with zero point logic
    int zeroPoint = (env.settings.target == AdsrTarget::PitchBend || env.settings.target == AdsrTarget::SmartScaleBend) ? 8192 : 0;
    int peak = env.peakValue;
    double range = static_cast<double>(peak - zeroPoint); // Can be negative for downward bends
    int outputVal = static_cast<int>(zeroPoint + (env.currentLevel * range));
    
    // Clamp to valid ranges
    if (env.settings.target == AdsrTarget::PitchBend || env.settings.target == AdsrTarget::SmartScaleBend) {
      outputVal = juce::jlimit(0, 16383, outputVal);
    } else {
      outputVal = juce::jlimit(0, 127, outputVal);
    }

    // Delta check: only send MIDI if value changed
    if (outputVal != env.lastSentValue) {
      if (env.settings.target == AdsrTarget::PitchBend || env.settings.target == AdsrTarget::SmartScaleBend) {
        midiEngine.sendPitchBend(env.channel, outputVal);
      } else {
        midiEngine.sendCC(env.channel, env.settings.ccNumber, outputVal);
      }
      env.lastSentValue = outputVal;
    }
  }

  // Remove finished envelopes
  activeEnvelopes.erase(
    std::remove_if(activeEnvelopes.begin(), activeEnvelopes.end(),
      [](const ActiveEnvelope& env) {
        return env.stage == Stage::Finished;
      }),
    activeEnvelopes.end());
}
