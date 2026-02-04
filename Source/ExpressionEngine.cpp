#include "ExpressionEngine.h"
#include "MappingTypes.h"
#include <algorithm>

namespace {
static bool isPitchBendTarget(const AdsrTarget target) {
  return target == AdsrTarget::PitchBend ||
         target == AdsrTarget::SmartScaleBend;
}
} // namespace

ExpressionEngine::ExpressionEngine(MidiEngine &engine) : midiEngine(engine) {
  for (int ch = 0; ch < 17; ++ch)
    currentPitchBendValues[ch] = 8192;
  startTimer(static_cast<int>(timerIntervalMs));
}

ExpressionEngine::~ExpressionEngine() {
  stopTimer(); // HighResolutionTimer

  const juce::ScopedLock sl(lock);
  activeEnvelopes.clear();
}

void ExpressionEngine::triggerEnvelope(InputID source, int channel,
                                       const AdsrSettings &settings,
                                       int peakValue) {
  juce::ScopedLock scopedLock(lock);

  // Phase 56.1: Fast path for simple CC/PB (no envelope curve)
  if (settings.attackMs == 0 && settings.decayMs == 0 &&
      settings.releaseMs == 0) {
    if (isPitchBendTarget(settings.target)) {
      midiEngine.sendPitchBend(channel, peakValue);
    } else {
      midiEngine.sendCC(channel, settings.ccNumber, settings.valueWhenOn);
    }
    return;
  }

  // If this source exists in the PB stack for this channel, remove it (re-press
  // moves to top)
  if (isPitchBendTarget(settings.target)) {
    auto &stack = pitchBendStacks[channel];
    stack.erase(std::remove(stack.begin(), stack.end(), source), stack.end());
  }

  // Remove any existing envelope for this source (voice stealing)
  activeEnvelopes.erase(std::remove_if(activeEnvelopes.begin(),
                                       activeEnvelopes.end(),
                                       [source](const ActiveEnvelope &env) {
                                         return env.source == source;
                                       }),
                        activeEnvelopes.end());

  // Create new envelope: ADSR goes value when off -> value when on -> sustain
  // -> value when off
  ActiveEnvelope env;
  env.source = source;
  env.channel = channel;
  env.settings = settings;
  env.peakValue = peakValue;
  if (settings.target == AdsrTarget::SmartScaleBend ||
      settings.target == AdsrTarget::PitchBend) {
    env.valueWhenOn = peakValue; // 0-16383 (from lookup or semitones)
    env.valueWhenOff = 8192;     // neutral
  } else {
    env.valueWhenOn = settings.valueWhenOn; // 0-127
    env.valueWhenOff = settings.valueWhenOff;
  }
  env.stage = Stage::Attack;
  env.currentLevel = 0.0;
  env.stageProgress = 0.0;
  env.lastSentValue = -1; // Initialize to -1 so first value is always sent
  env.isDormant = false;

  if (isPitchBendTarget(settings.target)) {
    auto &stack = pitchBendStacks[channel];
    // If another envelope is currently driving this channel, make it dormant
    if (!stack.empty()) {
      InputID prevTop = stack.back();
      for (auto &e : activeEnvelopes) {
        if (e.source == prevTop && e.channel == channel &&
            isPitchBendTarget(e.settings.target)) {
          e.isDormant = true;
          break;
        }
      }
    }
    // Push new owner to top of stack
    stack.push_back(source);
    // Dynamic handoff: start from current physical pitch
    env.dynamicStartValue = currentPitchBendValues[channel];
  } else {
    // CC: start at value when off
    env.dynamicStartValue = settings.valueWhenOff;
  }

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

  auto findEnvIt = std::find_if(
      activeEnvelopes.begin(), activeEnvelopes.end(),
      [source](const ActiveEnvelope &e) { return e.source == source; });

  if (findEnvIt == activeEnvelopes.end())
    return;

  auto &env = *findEnvIt;
  if (env.stage == Stage::Finished)
    return;

  // Pitch Bend priority stack behavior (Phase 23.7)
  if (isPitchBendTarget(env.settings.target)) {
    auto &stack = pitchBendStacks[env.channel];
    auto it = std::find(stack.begin(), stack.end(), source);
    if (it == stack.end()) {
      // Fallback: behave like normal release
      env.stage = Stage::Release;
      if (env.settings.releaseMs > 0 && env.currentLevel > 0.0) {
        double numSteps = env.settings.releaseMs / timerIntervalMs;
        env.stepSize = env.currentLevel / numSteps;
      } else {
        env.stepSize = env.currentLevel;
      }
      return;
    }

    const bool wasTop = (!stack.empty() && stack.back() == source);
    stack.erase(it);

    if (!wasTop) {
      // Background key released: remove silently
      env.isDormant = true;
      env.stage = Stage::Finished;
      return;
    }

    if (!stack.empty()) {
      // Active driver released -> handoff back to previous key, re-attack it
      env.isDormant = true;
      env.stage = Stage::Finished; // Kill released owner immediately

      const InputID newTop = stack.back();
      for (auto &e : activeEnvelopes) {
        if (e.source == newTop && e.channel == env.channel &&
            isPitchBendTarget(e.settings.target) &&
            e.stage != Stage::Finished) {
          e.isDormant = false;
          e.dynamicStartValue = currentPitchBendValues[env.channel];
          e.currentLevel = 0.0;
          e.stageProgress = 0.0;
          e.lastSentValue = -1;
          e.stage = Stage::Attack;

          if (e.settings.attackMs > 0) {
            double numSteps = e.settings.attackMs / timerIntervalMs;
            e.stepSize = 1.0 / numSteps;
          } else {
            e.stepSize = 1.0;
            e.currentLevel = 1.0;
            e.stage = Stage::Decay;
          }
          break;
        }
      }
      return;
    }

    // Stack empty: let this envelope release to value when off
    env.isDormant = false;
    env.stage = Stage::Release;
    env.lastSentValue = -1;
    env.dynamicStartValue = (env.settings.target == AdsrTarget::SmartScaleBend)
                                ? 8192
                                : env.valueWhenOff;
    env.peakValue = currentPitchBendValues[env.channel];
    env.currentLevel = 1.0;
    env.stageProgress = 0.0;

    if (env.settings.releaseMs > 0) {
      double numSteps = env.settings.releaseMs / timerIntervalMs;
      env.stepSize = 1.0 / numSteps;
    } else {
      env.stepSize = 1.0;
    }
    return;
  }

  // CC: Standard release
  env.stage = Stage::Release;

  // Calculate step size for Release: from currentLevel to 0.0 in releaseMs
  if (env.settings.releaseMs > 0 && env.currentLevel > 0.0) {
    double numSteps = env.settings.releaseMs / timerIntervalMs;
    env.stepSize = env.currentLevel / numSteps;
  } else {
    // Instant release
    env.stepSize = env.currentLevel;
  }
}

void ExpressionEngine::hiResTimerCallback() { processOneTick(); }

void ExpressionEngine::processOneTick() {
  juce::ScopedLock scopedLock(lock);

  for (auto &env : activeEnvelopes) {
    if (env.isDormant)
      continue;

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

    const bool isPB = isPitchBendTarget(env.settings.target);
    const bool isSmartBend =
        (env.settings.target == AdsrTarget::SmartScaleBend);

    // ADSR: output = valueWhenOff + level * (valueWhenOn - valueWhenOff)
    // For CC: valueWhenOn/Off are 0–127. For PitchBend/SmartScaleBend they are
    // already 0–16383 (no extra scaling).
    int outputVal = env.valueWhenOff +
                    static_cast<int>(env.currentLevel *
                                     static_cast<double>(env.valueWhenOn -
                                                         env.valueWhenOff));

    // Clamp to valid ranges
    if (isPB) {
      outputVal = juce::jlimit(0, 16383, outputVal);
    } else {
      outputVal = juce::jlimit(0, 127, outputVal);
    }

    // Delta check: only send MIDI if value changed
    if (outputVal != env.lastSentValue) {
      if (isPB) {
        currentPitchBendValues[env.channel] = outputVal;
        midiEngine.sendPitchBend(env.channel, outputVal);
      } else {
        midiEngine.sendCC(env.channel, env.settings.ccNumber, outputVal);
      }
      env.lastSentValue = outputVal;
    }
  }

  // Remove finished envelopes
  activeEnvelopes.erase(std::remove_if(activeEnvelopes.begin(),
                                       activeEnvelopes.end(),
                                       [](const ActiveEnvelope &env) {
                                         return env.stage == Stage::Finished;
                                       }),
                        activeEnvelopes.end());
}
