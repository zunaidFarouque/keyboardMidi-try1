#pragma once
#include <array>
#include <atomic>
#include <cstdint>

class RhythmAnalyzer {
public:
  RhythmAnalyzer();
  ~RhythmAnalyzer() = default;

  // Log a tap (note onset) - calculates interval and updates moving average
  void logTap();

  // Get adaptive speed based on current average (applies safety factor)
  // Returns glide time in milliseconds, clamped between minMs and maxMs
  int getAdaptiveSpeed(int minMs, int maxMs) const;

private:
  // Circular buffer of intervals (deltas in milliseconds)
  std::array<int, 8> intervals;
  int writeIndex = 0;
  int64_t lastTimeMs = 0;
  
  // Lock-free atomic result (moving average in milliseconds)
  std::atomic<int> currentAverageMs{200};

  // Get current time in milliseconds
  int64_t getCurrentTimeMs() const;
};
