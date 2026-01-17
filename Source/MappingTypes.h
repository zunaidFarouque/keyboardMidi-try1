#pragma once
#include <cstdint>
#include <functional> // For std::hash

// Action types for MIDI mapping
enum class ActionType {
  Note, // MIDI Note
  CC,   // MIDI Control Change
  Macro // Future: Custom macro actions
};

// Represents a MIDI action to be performed
struct MidiAction {
  ActionType type;
  int channel;
  int data1; // Note number or CC number
  int data2; // Velocity or CC value
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
  std::size_t operator()(const InputID &id) const noexcept {
    // Combine hash of deviceHandle and keyCode
    std::size_t h1 = std::hash<uintptr_t>{}(id.deviceHandle);
    std::size_t h2 = std::hash<int>{}(id.keyCode);
    return h1 ^ (h2 << 1); // Combine hashes
  }
};
} // namespace std
