#pragma once
#include <JuceHeader.h>
#include <array>
#include <cstdint>
#include <functional> // For std::hash
#include <memory>     // For std::shared_ptr
#include <optional>   // For std::optional
#include <unordered_map>
#include <vector>

// Action types for MIDI mapping
enum class ActionType {
  Note,       // MIDI Note
  Expression, // Phase 56.1: Unified CC + Envelope (key/button → CC or PB)
  Command,    // Sustain/Latch/Panic (data1 = CommandID)
  Macro       // Future: Custom macro actions
};

// Polyphony modes (Phase 26)
enum class PolyphonyMode {
  Poly,  // Polyphonic (multiple notes simultaneously)
  Mono,  // Monophonic (last note priority, retrigger)
  Legato // Legato (portamento glide, no retrigger within PB range)
};

// Command IDs for ActionType::Command (stored in MidiAction::data1)
// In namespace to avoid clash with juce::CommandID
namespace OmniKey {
enum class CommandID : int {
  SustainMomentary = 0, // Press=On, Release=Off
  SustainToggle = 1,    // Press=Flip
  SustainInverse = 2,   // Press=Off, Release=On (Palm Mute)
  LatchToggle = 3,      // Global Latch Mode
  Panic = 4,            // All Notes Off
  PanicLatch = 5,       // Kill only Latched notes
  GlobalPitchUp = 6,    // Chromatic +1
  GlobalPitchDown = 7,  // Chromatic -1
  GlobalModeUp = 8,     // Degree +1
  GlobalModeDown = 9,   // Degree -1
  LayerMomentary = 10,  // Press=layer on, Release=layer off (data2 = layer ID)
  LayerToggle = 11,     // Press=flip layer active (data2 = layer ID)
  LayerSolo = 12        // Press=only this layer active (data2 = layer ID)
};
}

// Pseudo-codes for non-keyboard inputs (Mouse/Trackpad) and explicit modifier
// key codes (mirroring Windows VK codes without including <windows.h>)
namespace InputTypes {
constexpr int ScrollUp = 0x1001;
constexpr int ScrollDown = 0x1002;
constexpr int PointerX = 0x2000;
constexpr int PointerY = 0x2001;

// Explicit Modifier Codes (mirroring Windows VK codes)
constexpr int Key_LShift = 0xA0;
constexpr int Key_RShift = 0xA1;
constexpr int Key_LControl = 0xA2;
constexpr int Key_RControl = 0xA3;
constexpr int Key_LAlt = 0xA4;
constexpr int Key_RAlt = 0xA5;
} // namespace InputTypes

// ADSR envelope target types (Phase 25.1)
enum class AdsrTarget {
  CC,            // Control Change
  PitchBend,     // Standard Pitch Bend
  SmartScaleBend // Scale-based Pitch Bend (pre-compiled lookup)
};

// ADSR envelope settings (Phase 23.1)
struct AdsrSettings {
  int attackMs = 10;                  // Attack time in milliseconds
  int decayMs = 10;                   // Decay time in milliseconds
  float sustainLevel = 0.7f;          // Sustain level (0.0-1.0)
  int releaseMs = 100;                // Release time in milliseconds
  AdsrTarget target = AdsrTarget::CC; // Target type
  int ccNumber = 1;                   // CC number (if target is CC)
  bool useCustomEnvelope = false;     // Phase 56.1: false = fast path (simple CC/PB)

  // Legacy compatibility: isPitchBend maps to target
  bool isPitchBend() const {
    return target == AdsrTarget::PitchBend ||
           target == AdsrTarget::SmartScaleBend;
  }
};

// Represents a MIDI action to be performed
struct MidiAction {
  ActionType type;
  int channel;
  int data1;              // Note number or CC number
  int data2;              // Velocity or CC value
  int velocityRandom = 0; // Velocity randomization range (0 = no randomization)
  AdsrSettings adsrSettings;        // ADSR settings (for ActionType::Expression)
  std::vector<int> smartBendLookup; // Pre-compiled PB lookup table (128
                                    // entries) for SmartScaleBend

  // Phase 55.4: Note options
  bool isOneShot = false; // If true, do not send NoteOff on key release

  // Phase 55.4: CC options
  bool sendReleaseValue = false; // If true, send a specific value on key release
  int releaseValue = 0;
};

// Represents a unique input source (device + key)
// Uses uintptr_t to store device handle without including Windows headers
struct InputID {
  uintptr_t deviceHandle; // Device handle cast to uintptr_t
  int keyCode;            // Virtual key code

  // Equality operator for use as map key
  bool operator==(const InputID &other) const {
    return deviceHandle == other.deviceHandle && keyCode == other.keyCode;
  }

  // Ordering operator (Phase 40.1: used for std::set<InputID> / deterministic
  // ordering)
  bool operator<(const InputID &other) const {
    if (deviceHandle != other.deviceHandle)
      return deviceHandle < other.deviceHandle;
    return keyCode < other.keyCode;
  }
};

// Hash specialization for InputID (required for
// std::unordered_map/unordered_multimap)
namespace std {
template <> struct hash<InputID> {
  std::size_t operator()(const InputID &k) const noexcept {
    // Robust hash combine (based on boost::hash_combine)
    std::size_t h1 = std::hash<uintptr_t>{}(k.deviceHandle);
    std::size_t h2 = std::hash<int>{}(k.keyCode);
    // seed ^ (hash + 0x9e3779b9 + (seed<<6) + (seed>>2))
    return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
  }
};
} // namespace std

// Visual state enum for Visualizer (Phase 39.1)
enum class VisualState {
  Empty,
  Active,    // Defined locally, no global conflict
  Inherited, // Undefined locally, using global
  Override,  // Defined locally, masking global
  Conflict   // (Optional) Hard error state
};

// Rich return type for simulation (Phase 39.1)
struct SimulationResult {
  std::optional<MidiAction> action;
  VisualState state = VisualState::Empty;
  juce::String sourceName; // e.g. "Mapping" or "Zone: Main"
  // Helper to know if it's a Zone or Mapping for coloring
  bool isZone = false;

  // Legacy compatibility (Phase 39)
  juce::String sourceDescription; // For Log (maps to sourceName)
  bool isOverride = false;        // Maps to (state == VisualState::Override)
  bool isInherited = false;       // Maps to (state == VisualState::Inherited)

  // Constructor to maintain backward compatibility
  SimulationResult() { updateLegacyFields(); }

  void updateLegacyFields() {
    sourceDescription = sourceName;
    isOverride = (state == VisualState::Override);
    isInherited = (state == VisualState::Inherited);
  }
};

// ---------------------------------------------------------------------------
// Phase 50.1 - Grid-based Compiler Data Structures
// ---------------------------------------------------------------------------

// Lightweight atom for the Audio Thread.
// For simple mappings, 'action' is used directly.
// For chords or complex sequences, 'chordIndex' points into
// CompiledContext::chordPool.
struct KeyAudioSlot {
  bool isActive = false;
  MidiAction action; // The primary action

  // For Chords or complex sequences, we index into a pool in CompiledContext.
  // -1 means use 'action' directly. >= 0 means look up chordPool[chordIndex].
  int chordIndex = -1;
};

// Rich data for the UI / Visualizer thread.
struct KeyVisualSlot {
  VisualState state = VisualState::Empty;
  juce::Colour displayColor = juce::Colours::transparentBlack;
  juce::String label;      // Pre-calculated text (e.g., "C# Maj7")
  juce::String sourceName; // e.g., "Zone: Main", "Mapping: Base"
  bool isGhost = false;    // Phase 54.1: Ghost note (quieter, dimmed in UI)
};

// 256 slots covering all Virtual Key Codes (0x00 - 0xFF)
using AudioGrid = std::array<KeyAudioSlot, 256>;
using VisualGrid = std::array<KeyVisualSlot, 256>;

// Holds the entire pre-calculated state of the engine.
struct CompiledContext {
  // 1. Audio Data (Read by InputProcessor/AudioThread)
  // Map HardwareHash -> Array of 9 AudioGrids (one per layer 0..8)
  std::unordered_map<uintptr_t, std::array<std::shared_ptr<const AudioGrid>, 9>>
      deviceGrids;

  // Global fallback: 9 AudioGrids (one per layer 0..8)
  std::array<std::shared_ptr<const AudioGrid>, 9> globalGrids;

  // Pool for complex chords (referenced by KeyAudioSlot::chordIndex)
  // Vector of MidiActions (one vector per chord)
  std::vector<std::vector<MidiAction>> chordPool;

  // 2. Visual Data (Read by Visualizer/MessageThread)
  // Map AliasHash -> LayerID (0-8) -> VisualGrid
  // Using vector for layers for O(1) access [0..8]
  std::unordered_map<uintptr_t, std::vector<std::shared_ptr<const VisualGrid>>>
      visualLookup;
};

// Backward-compatible alias used in development docs/prompts.
using CompiledMapContext = CompiledContext;
