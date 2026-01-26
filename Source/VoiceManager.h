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
  void noteOn(InputID source, int note, int vel, int channel, bool allowSustain = true, int releaseMs = 0);
  void noteOn(InputID source, const std::vector<int>& notes, const std::vector<int>& velocities, int channel, int strumSpeedMs, bool allowSustain = true, int releaseMs = 0);

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
  
  // Check if a specific key code has any latched voices
  bool isKeyLatched(int keyCode) const;

  // --- Panic ---
  void panic();
  void panicLatch();

  // --- State flush (when mappings change) ---
  void resetPerformanceState();

private:
  enum class VoiceState { Playing, Sustained, Latched };

  struct ActiveVoice {
    int noteNumber;
    int midiChannel;
    InputID source;
    bool allowSustain;
    VoiceState state;
    int releaseMs = 0; // Zone release envelope; 0 = instant NoteOff
  };

  void addVoiceFromStrum(InputID source, int note, int channel, bool allowSustain);

  // HighResolutionTimer callback - checks for expired releases
  void hiResTimerCallback() override;

  struct PendingRelease {
    double releaseTimeMs;
    int durationMs;
    bool shouldSustain;
  };

  struct PendingNoteOff {
    int note;
    int channel;
    double targetTimeMs;
  };

  MidiEngine& midiEngine;
  StrumEngine strumEngine;
  mutable std::vector<ActiveVoice> voices; // Mutable for const accessors
  mutable juce::CriticalSection voicesLock; // Mutable for const accessors
  std::unordered_map<InputID, PendingRelease> pendingReleases; // Track releases waiting for expiration
  std::vector<PendingNoteOff> releaseQueue; // Delayed NoteOff (Phase 21.3)
  juce::CriticalSection releasesLock;

  bool globalSustainActive = false;
  bool globalLatchActive = false;

  double getCurrentTimeMs() const;
};
