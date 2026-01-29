### 📋 Cursor Prompt: Phase 50.6 - Wiring the Visualizer

**Target Files:**
1.  `Source/InputProcessor.h`
2.  `Source/VisualizerComponent.h`
3.  `Source/VisualizerComponent.cpp`

**Task:**
Refactor `VisualizerComponent` to stop calculating state dynamically. It must now strictly read the pre-compiled `VisualGrid` from `InputProcessor`.

**Specific Instructions:**

1.  **Update `InputProcessor.h` (Expose Context):**
    Add a thread-safe accessor to get the current context:
    ```cpp
    // Returns a copy of the shared_ptr to ensure it doesn't vanish during use
    std::shared_ptr<const CompiledMapContext> getContext() const;
    ```
    (Implement this in `.cpp` using `juce::ScopedReadLock`).

2.  **Update `VisualizerComponent.h` (Cleanup):**
    *   Remove `findZoneForKey`, `isKeyInAnyZone`.
    *   Remove `SimulationResult` references (no longer needed here).

3.  **Update `VisualizerComponent.cpp` (`refreshCache` Rewrite):**
    Delete the old loop logic inside `refreshCache` and replace with this "Grid Reader" approach:

    *   **Step A: Acquire Data**
        *   Call `inputProcessor->getContext()`. If null, return.
        *   Determine the **Target Grid**:
            *   Look up `context->visualLookup[currentViewHash]`.
            *   If not found (or hash is 0), look up `context->visualLookup[0]` (Global).
            *   Get the specific layer: `targetGrid = visualLookupEntry[currentVisualizedLayer]`.
            *   *Safety:* Check if `targetGrid` is null.

    *   **Step B: Iterate Layout & Draw**
        *   Iterate `KeyboardLayoutUtils::getLayout()`.
        *   For each `keyCode`:
            *   Get `slot = (*targetGrid)[keyCode];`.
            *   **Skip Empty:** If `slot.state == VisualState::Empty`, draw standard grey key background.
            *   **Draw Active:**
                *   **Color:** Use `slot.displayColor`.
                *   **Opacity:**
                    *   `Inherited`: alpha = 0.3f
                    *   `Active/Override`: alpha = 1.0f
                *   **Border:**
                    *   `Override`: Orange, thickness 2.0f.
                    *   `Conflict`: Red, thickness 2.0f.
                    *   Others: Grey, thickness 1.0f.
                *   **Label:** Use `slot.label` (Pre-baked string).

4.  **Update `paint` (Dynamic Overlays):**
    *   The `refreshCache` handles the *map state* (zones, mappings).
    *   `paint` handles the *input state* (pressing keys).
    *   Keep the logic that draws Yellow/Cyan overlays for `activeKeys` and `latchedKeys` on top of the cached image.
    *   *Optimization:* No need to call `simulateInput` inside `paint` anymore. Just check `voiceManager.isKeyLatched(keyCode)` or `activeKeys.contains(keyCode)`.

**Outcome:**
The Visualizer effectively becomes a "dumb" renderer of the `VisualGrid`. Switching layers or aliases becomes instant ($O(1)$ pointer swap), and no complex logic runs on the UI thread.