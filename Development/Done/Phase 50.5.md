### 📋 Cursor Prompt: Phase 50.5 - Wiring InputProcessor (The Switchover)

**Target Files:**
1.  `Source/MappingTypes.h`
2.  `Source/GridCompiler.cpp`
3.  `Source/InputProcessor.h`
4.  `Source/InputProcessor.cpp`

**Task:**
Replace the old "Map-based" engine in `InputProcessor` with the new "Grid-based" engine.

**Specific Instructions:**

1.  **Update `MappingTypes.h` (Context Adjustment):**
    We need 9 Audio Grids per device so we can switch layers dynamically on key press.
    Change `CompiledMapContext` members:
    ```cpp
    struct CompiledMapContext {
        // CHANGED: Now stores 9 layers of grids per device
        // Map HardwareHash -> Array of 9 AudioGrids
        std::unordered_map<uintptr_t, std::array<std::shared_ptr<const AudioGrid>, 9>> deviceGrids;
        
        // CHANGED: Global fallback also needs 9 layers
        std::array<std::shared_ptr<const AudioGrid>, 9> globalGrids;
        
        std::vector<std::vector<MidiAction>> chordPool; 
        std::unordered_map<uintptr_t, std::vector<std::shared_ptr<const VisualGrid>>> visualLookup;
    };
    ```

2.  **Update `GridCompiler.cpp` (Populate 9 Audio Layers):**
    In your `compile` method, ensure you are populating the specific layer index in `deviceGrids` / `globalGrids`.
    *   *Note:* Unlike Visuals, Audio Grids do **NOT** need to inherit/copy from lower layers. They should only contain what is explicitly defined in that layer. The `InputProcessor` will handle the "fall through" logic by iterating layers.

3.  **Update `InputProcessor.h`:**
    *   Include `GridCompiler.h`.
    *   Remove `std::vector<Layer> layers`.
    *   Remove `rebuildMapFromTree` (logic moves to `rebuildGrid`).
    *   Add:
        ```cpp
        // The Atomic Hot-Swap Pointer
        std::shared_ptr<const CompiledMapContext> activeContext;
        juce::ReadWriteLock contextLock;
        
        // Helper to swap
        void rebuildGrid();
        ```

4.  **Update `InputProcessor.cpp` (The Core Logic):**

    *   **Implement `rebuildGrid`:**
        ```cpp
        void InputProcessor::rebuildGrid() {
            auto newContext = GridCompiler::compile(presetManager, deviceManager, zoneManager);
            {
                juce::ScopedWriteLock sl(contextLock);
                activeContext = newContext;
            }
            // Trigger UI repaint if needed
            sendChangeMessage(); 
        }
        ```
    *   **Replace `rebuildMapFromTree` calls:** Replace all calls to `rebuildMapFromTree()` with `rebuildGrid()`.

    *   **Rewrite `processEvent` (The O(1) Path):**
        *   Acquire `ScopedReadLock sl(contextLock)`.
        *   Determine `effectiveDevice` (check Studio Mode).
        *   **Layer Loop:** Iterate `i` from 8 down to 0.
            *   Check `layer[i].isActive` (keep your existing `Layer` struct logic for tracking state, but remove the maps inside it).
            *   **Lookup:**
                *   Try `ctx->deviceGrids[effectiveDevice][i]`.
                *   If null/empty, try `ctx->globalGrids[i]`.
            *   **Fetch:** `slot = grid[keyCode]`.
            *   **Hit?** If `slot.isActive`:
                *   Trigger Action (Note/CC/Command).
                *   If `slot.chordIndex >= 0`, look up `ctx->chordPool` and trigger those too.
                *   **RETURN** (Stop searching lower layers).

    *   **Update `simulateInput` (Temporary Fix):**
        *   Update it to read from `activeContext->visualLookup` instead of the old maps.
        *   This ensures the Visualizer (Phase 50.6) will have data to read.

**Critical Note on `Layer` Struct:**
You still need `std::vector<Layer> layers` in `InputProcessor.h` to track the *dynamic state* (isLatched, isMomentary) of layers 0-8. Just remove the `compiledMap` and `configMap` from the `Layer` struct definition in `InputProcessor.h`. Keep the state booleans.