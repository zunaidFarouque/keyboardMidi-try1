#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "StrumEngine.h"
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <unordered_map>

class VoiceManager : public juce::HighResolutionTimer {
public:
  explicit VoiceManager(MidiEngine& engine);
  ~VoiceManager() = default;

  // --- Note playback ---
  void noteOn(InputID source, int note, int vel, int channel, bool allowSustain = true);
  void noteOn(InputID source, const std::vector<int>& notes, int vel, int channel, int strumSpeedMs, bool allowSustain = true);

  void strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke);

  void handleKeyUp(InputID source);
  void handleKeyUp(InputID source, int releaseDurationMs, bool shouldSustain);

  void sendCC(int channel, int controller, int value);

  // --- Sustain (pedal) ---
  void setSustain(bool active);
  bool isSustainActive() const { return globalSustainActive; }

  // --- Latch (toggle hold) ---
  void setLatch(bool active) { globalLatchActive = active; }
  bool isLatchActive() const { return globalLatchActive; }

  // --- Panic ---
  void panic();
  void panicLatch();

private:
  enum class VoiceState { Playing, Sustained, Latched };

  struct ActiveVoice {
    int noteNumber;
    int midiChannel;
    InputID source;
    bool allowSustain;
    VoiceState state;
  };

  void addVoiceFromStrum(InputID source, int note, int channel, bool allowSustain);

  // HighResolutionTimer callback - checks for expired releases
  void hiResTimerCallback() override;

  struct PendingRelease {
    double releaseTimeMs;
    int durationMs;
    bool shouldSustain;
  };

  MidiEngine& midiEngine;
  StrumEngine strumEngine;
  std::vector<ActiveVoice> voices;
  juce::CriticalSection voicesLock;
  std::unordered_map<InputID, PendingRelease> pendingReleases; // Track releases waiting for expiration
  juce::CriticalSection releasesLock;

  bool globalSustainActive = false;
  bool globalLatchActive = false;

  double getCurrentTimeMs() const;
};
