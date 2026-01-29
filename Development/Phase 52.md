### 📋 Cursor Prompt: Phase 52.1 - Wiring the Audio Engine

**Target Files:**
1.  `Source/InputProcessor.h`
2.  `Source/InputProcessor.cpp`

**Task:**
Replace the old Map-based lookup system in `InputProcessor` with the new **Grid-based System** we just verified.

**Specific Instructions:**

1.  **Update `Source/InputProcessor.h`:**
    *   **Remove:**
        *   `std::vector<Layer> layers;` (The old structure containing maps).
        *   `rebuildMapFromTree();`
        *   `addMappingFromTree`, `removeMappingFromTree` (Compiler handles this now).
        *   `findMapping`, `lookupAction`, `getZoneForInputResolved`.
    *   **Keep:**
        *   `std::vector<bool> layerActiveState;` (New member to track `isLatched`/`isMomentary` for the 9 layers. We need to persist the *state* of the layers, just not the *data*).
    *   **Add:**
        *   `std::shared_ptr<const CompiledMapContext> activeContext;`
        *   `void rebuildGrid();` (The new compiler call).
        *   `std::shared_ptr<const CompiledMapContext> getContext() const;` (Thread-safe accessor).

2.  **Update `Source/InputProcessor.cpp`:**

    *   **Constructor:** Resize `layerActiveState` to 9. Initialize `activeContext` with an empty context (to avoid null crashes before first compile).

    *   **Implement `rebuildGrid`:**
        ```cpp
        void InputProcessor::rebuildGrid() {
            // 1. Run the Compiler (Tested Logic)
            auto newContext = GridCompiler::compile(presetManager, deviceManager, zoneManager);
            
            // 2. Atomic Swap
            {
                juce::ScopedWriteLock sl(mapLock);
                activeContext = newContext;
                
                // 3. Update Layer Active States from Preset
                // (We still need to know which layers are active by default)
                auto layersList = presetManager.getLayersList();
                for (int i = 0; i < 9; ++i) {
                    auto layerNode = layersList.getChild(i); // Assuming ordered or find by ID
                    // Simple check: if ID matches, get "isActive" property
                    // For now, Layer 0 is always true.
                }
            }
            sendChangeMessage();
        }
        ```

    *   **Rewrite `processEvent` (The Critical Path):**
        *   **Lock:** `ScopedReadLock sl(mapLock);`
        *   **Context:** `auto ctx = activeContext;`
        *   **Device:** Determine `effectiveDevice` (Studio Mode check).
        *   **Loop:** Iterate `layerIdx` from 8 down to 0.
            *   **Check State:** `if (!layerActiveState[layerIdx] && layerIdx != 0) continue;`
            *   **Get Grid:**
                *   Check `ctx->deviceGrids.find(effectiveDevice)`.
                *   If found, use `(*deviceGrid)[layerIdx]`.
                *   Else, use `ctx->globalGrids[layerIdx]`.
            *   **Lookup:** `const auto& slot = (*grid)[input.keyCode];`
            *   **Hit:** `if (slot.isActive)`
                *   **Action:** Trigger `slot.action`.
                *   **Chord:** `if (slot.chordIndex >= 0)` -> iterate `ctx->chordPool[slot.chordIndex]` and trigger.
                *   **Break:** Return immediately (higher priority wins).

3.  **Refactor Listeners:**
    *   Update `valueTreeChanged` callbacks to call `rebuildGrid()` instead of `rebuildMapFromTree()`.

**Goal:**
After this, the App handles MIDI correctly using the new engine. The Visualizer will likely be broken (compilation errors) because we removed `simulateInput`. That is expected; we fix it in 52.2.