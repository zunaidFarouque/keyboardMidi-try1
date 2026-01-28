Here is the Cursor prompt for **Phase 50.1**. This sets up the foundational data structures without breaking the existing logic yet.

### 📋 Cursor Prompt: Phase 50.1 - Data Layer Definitions

**Target File:** `Source/MappingTypes.h`

**Task:**
We are starting a massive refactor to move from a "Query-based" architecture to a "Compiler-based" architecture.
Please update `Source/MappingTypes.h` to define the new "Grid" data structures.

**Specific Instructions:**

1.  **Add Modifier Constants:**
    At the top of the file (inside `namespace InputTypes` or global), define these specific key code constants to ensure we don't need `<windows.h>` here but have clarity:
    ```cpp
    namespace InputTypes {
        // ... existing scroll/pointer codes ...
        // Explicit Modifier Codes (mirroring Windows VK codes)
        constexpr int Key_LShift = 0xA0;
        constexpr int Key_RShift = 0xA1;
        constexpr int Key_LControl = 0xA2;
        constexpr int Key_RControl = 0xA3;
        constexpr int Key_LAlt = 0xA4;
        constexpr int Key_RAlt = 0xA5;
    }
    ```

2.  **Define `KeyAudioSlot` Struct:**
    This is the lightweight atom for the Audio Thread.
    ```cpp
    struct KeyAudioSlot {
        bool isActive = false;
        MidiAction action; // The primary action
        
        // For Chords or complex sequences, we index into a pool in CompiledContext.
        // -1 means use 'action' directly. >= 0 means look up chordPool[chordIndex].
        int chordIndex = -1; 
    };
    ```

3.  **Define `KeyVisualSlot` Struct:**
    This is the rich data for the UI Thread.
    ```cpp
    struct KeyVisualSlot {
        VisualState state = VisualState::Empty;
        juce::Colour displayColor = juce::Colours::transparentBlack;
        juce::String label;      // Pre-calculated text (e.g., "C# Maj7")
        juce::String sourceName; // e.g., "Zone: Main", "Mapping: Base"
    };
    ```

4.  **Define the Grid Aliases:**
    ```cpp
    // 256 slots covering all Virtual Key Codes (0x00 - 0xFF)
    using AudioGrid = std::array<KeyAudioSlot, 256>;
    using VisualGrid = std::array<KeyVisualSlot, 256>;
    ```

5.  **Define `CompiledContext` Struct:**
    This holds the entire pre-calculated state of the engine.
    ```cpp
    struct CompiledMapContext {
        // 1. Audio Data (Read by InputProcessor/AudioThread)
        // Map HardwareHash -> AudioGrid
        std::unordered_map<uintptr_t, std::shared_ptr<const AudioGrid>> deviceGrids;
        std::shared_ptr<const AudioGrid> globalGrid; // Fallback
        
        // Pool for complex chords (referenced by KeyAudioSlot::chordIndex)
        // Vector of MidiActions (one vector per chord)
        std::vector<std::vector<MidiAction>> chordPool; 

        // 2. Visual Data (Read by Visualizer/MessageThread)
        // Map AliasHash -> LayerID (0-8) -> VisualGrid
        // Using vector for layers for O(1) access [0..8]
        std::unordered_map<uintptr_t, std::vector<std::shared_ptr<const VisualGrid>>> visualLookup;
    };
    ```

**Notes:**
*   Keep existing structs (`MidiAction`, `InputID`, etc.) as they are still used.
*   Ensure `#include <array>` and `#include <memory>` are present.
*   Do not remove the old enums yet.