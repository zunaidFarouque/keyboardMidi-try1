#pragma once
#include "MappingTypes.h"
#include "MidiEngine.h"
#include "StrumEngine.h"
#include "PortamentoEngine.h"
#include "SettingsManager.h"
#include <JuceHeader.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <deque>
#include <array>

class VoiceManager : public juce::HighResolutionTimer, public juce::ChangeListener {
public:
  VoiceManager(MidiEngine& engine, SettingsManager& settingsMgr);
  ~VoiceManager() override;

  // --- Note playback ---
  void noteOn(InputID source, int note, int vel, int channel, bool allowSustain = true, int releaseMs = 0, PolyphonyMode polyMode = PolyphonyMode::Poly, int glideTimeMs = 50);
  void noteOn(InputID source, const std::vector<int>& notes, const std::vector<int>& velocities, int channel, int strumSpeedMs, bool allowSustain = true, int releaseMs = 0, PolyphonyMode polyMode = PolyphonyMode::Poly, int glideTimeMs = 50);

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

  // ChangeListener implementation (for SettingsManager PB range changes)
  void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
  // Rebuild PB lookup table when SettingsManager changes
  void rebuildPbLookup();

  // Get current playing note for a channel (for Mono/Legato)
  int getCurrentPlayingNote(int channel) const;

  // Mono/Legato stack management
  void pushToMonoStack(int channel, int note, InputID source);
  void removeFromMonoStack(int channel, InputID source);
  int getMonoStackTop(int channel) const; // Returns -1 if empty
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
  SettingsManager& settingsManager;
  StrumEngine strumEngine;
  PortamentoEngine portamentoEngine;
  mutable std::vector<ActiveVoice> voices; // Mutable for const accessors
  mutable juce::CriticalSection voicesLock; // Mutable for const accessors
  std::unordered_map<InputID, PendingRelease> pendingReleases; // Track releases waiting for expiration
  std::vector<PendingNoteOff> releaseQueue; // Delayed NoteOff (Phase 21.3)
  juce::CriticalSection releasesLock;

  bool globalSustainActive = false;
  bool globalLatchActive = false;

  // PB Lookup Table: Maps semitone delta (-127 to +127) to PB value (0-16383) or -1 if out of range
  std::array<int, 255> pbLookup; // Index = delta + 127

  // Mono Stack: Per-channel deque of notes (last note priority)
  std::unordered_map<int, std::deque<std::pair<int, InputID>>> monoStacks; // channel -> deque<note, source>
  juce::CriticalSection monoStackLock;
  
  // Per-channel polyphony mode and glide time (for handleKeyUp)
  std::unordered_map<int, std::pair<PolyphonyMode, int>> channelPolyModes; // channel -> <mode, glideTimeMs>

  double getCurrentTimeMs() const;
};
