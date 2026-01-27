#pragma once
#include <cstdint>
#include <functional> // For std::hash
#include <optional>   // For std::optional
#include <JuceHeader.h>

// Action types for MIDI mapping
enum class ActionType {
  Note,    // MIDI Note
  CC,     // MIDI Control Change
  Command,// Sustain/Latch/Panic (data1 = CommandID)
  Macro,  // Future: Custom macro actions
  Envelope// ADSR envelope (data1 = CC number or 0 for Pitch Bend, data2 = peak value)
};

// Polyphony modes (Phase 26)
enum class PolyphonyMode {
  Poly,   // Polyphonic (multiple notes simultaneously)
  Mono,   // Monophonic (last note priority, retrigger)
  Legato  // Legato (portamento glide, no retrigger within PB range)
};

// Command IDs for ActionType::Command (stored in MidiAction::data1)
// In namespace to avoid clash with juce::CommandID
namespace OmniKey {
enum class CommandID : int {
  SustainMomentary = 0,  // Press=On, Release=Off
  SustainToggle    = 1,  // Press=Flip
  SustainInverse   = 2,  // Press=Off, Release=On (Palm Mute)
  LatchToggle      = 3,  // Global Latch Mode
  Panic            = 4,  // All Notes Off
  PanicLatch       = 5,  // Kill only Latched notes
  GlobalPitchUp    = 6,  // Chromatic +1
  GlobalPitchDown  = 7,  // Chromatic -1
  GlobalModeUp     = 8,  // Degree +1
  GlobalModeDown   = 9   // Degree -1
};
}

// Pseudo-codes for non-keyboard inputs (Mouse/Trackpad)
namespace InputTypes {
  constexpr int ScrollUp   = 0x1001;
  constexpr int ScrollDown = 0x1002;
  constexpr int PointerX   = 0x2000;
  constexpr int PointerY   = 0x2001;
}

// ADSR envelope target types (Phase 25.1)
enum class AdsrTarget {
  CC,           // Control Change
  PitchBend,    // Standard Pitch Bend
  SmartScaleBend // Scale-based Pitch Bend (pre-compiled lookup)
};

// ADSR envelope settings (Phase 23.1)
struct AdsrSettings {
  int attackMs = 10;   // Attack time in milliseconds
  int decayMs = 10;    // Decay time in milliseconds
  float sustainLevel = 0.7f; // Sustain level (0.0-1.0)
  int releaseMs = 100; // Release time in milliseconds
  AdsrTarget target = AdsrTarget::CC; // Target type
  int ccNumber = 1;    // CC number (if target is CC)
  
  // Legacy compatibility: isPitchBend maps to target
  bool isPitchBend() const { return target == AdsrTarget::PitchBend || target == AdsrTarget::SmartScaleBend; }
};

// Represents a MIDI action to be performed
struct MidiAction {
  ActionType type;
  int channel;
  int data1; // Note number or CC number
  int data2; // Velocity or CC value
  int velocityRandom = 0; // Velocity randomization range (0 = no randomization)
  AdsrSettings adsrSettings; // ADSR settings (for ActionType::Envelope)
  std::vector<int> smartBendLookup; // Pre-compiled PB lookup table (128 entries) for SmartScaleBend
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
  Active,     // Defined locally, no global conflict
  Inherited,  // Undefined locally, using global
  Override,   // Defined locally, masking global
  Conflict    // (Optional) Hard error state
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
  SimulationResult() {
    updateLegacyFields();
  }
  
  void updateLegacyFields() {
    sourceDescription = sourceName;
    isOverride = (state == VisualState::Override);
    isInherited = (state == VisualState::Inherited);
  }
};
