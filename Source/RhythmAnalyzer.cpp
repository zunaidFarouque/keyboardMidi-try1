#include "RhythmAnalyzer.h"
#include <JuceHeader.h>
#include <algorithm>

RhythmAnalyzer::RhythmAnalyzer() {
  // Initialize buffer with default interval (200ms)
  intervals.fill(200);
  lastTimeMs = getCurrentTimeMs();
}

void RhythmAnalyzer::logTap() {
  int64_t now = getCurrentTimeMs();
  int64_t delta = now - lastTimeMs;
  
  // Ignore massive deltas (> 2000ms) to reset logic (user paused)
  if (delta > 2000) {
    // Reset: fill buffer with current delta and reset average
    intervals.fill(static_cast<int>(delta));
    currentAverageMs.store(static_cast<int>(delta));
    lastTimeMs = now;
    writeIndex = 0;
    return;
  }
  
  // Write to circular buffer
  intervals[writeIndex] = static_cast<int>(delta);
  writeIndex = (writeIndex + 1) % 8;
  
  // Recalculate average: sum buffer / 8
  int sum = 0;
  for (int interval : intervals) {
    sum += interval;
  }
  int average = sum / 8;
  
  // Store atomically (lock-free write)
  currentAverageMs.store(average);
  
  lastTimeMs = now;
}

int RhythmAnalyzer::getAdaptiveSpeed(int minMs, int maxMs) const {
  // Load atomically (lock-free read)
  int average = currentAverageMs.load();
  
  // Apply safety factor: Target = Average * 0.7f
  // Glide should be faster than the notes to feel responsive
  float target = static_cast<float>(average) * 0.7f;
  
  // Clamp to valid range
  return juce::jlimit(minMs, maxMs, static_cast<int>(target));
}

int64_t RhythmAnalyzer::getCurrentTimeMs() const {
  return static_cast<int64_t>(juce::Time::getMillisecondCounterHiRes());
}
