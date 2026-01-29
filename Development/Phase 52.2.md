### 📋 Cursor Prompt: Phase 52.2 - Wiring the Visualizer

**Target Files:**
1.  `Source/VisualizerComponent.h`
2.  `Source/VisualizerComponent.cpp`

**Task:**
Refactor `VisualizerComponent` to render the pre-compiled `VisualGrid` from `InputProcessor`.

**Specific Instructions:**

1.  **Update Header (`.h`):**
    *   Remove `findZoneForKey`, `isKeyInAnyZone`.
    *   Remove `SimulationResult` references.
    *   Ensure `juce::Timer` inheritance is present.

2.  **Update Source (`.cpp`):**
    *   **Remove `simulateInput` calls:** Delete any code relying on this.
    *   **Implement `refreshCache`:**
        *   **Get Context:** `auto ctx = inputProcessor->getContext();`
        *   **Select Grid:**
            *   Try `ctx->visualLookup[currentViewHash][currentVisualizedLayer]`.
            *   Fallback: `ctx->visualLookup[0][currentVisualizedLayer]` (Global).
        *   **Iterate Keys (0..255):**
            *   Get `const KeyVisualSlot& slot`.
            *   If `slot.state == VisualState::Empty`, continue.
            *   **Drawing Rules (Strictly follow slot data):**
                *   **Backdrop Color:** `slot.displayColor`.
                *   **Alpha/Dimming:**
                    *   `Inherited`: `alpha = 0.3f`.
                    *   `Active/Override/Conflict`: `alpha = 1.0f`.
                *   **Borders:**
                    *   `Override`: `Orange`, thickness `2.5f`.
                    *   `Conflict`: `Yellow`, thickness `2.5f`.
                    *   Default: `Grey`, thickness `1.0f`.
                *   **Text:** `slot.label`.
                *   **Conflict Handling:** If `slot.state == VisualState::Conflict`, force background to `DarkRed` and text to `White`.
        *   **Draw:** `g.fillRect` (with alpha), `g.drawRoundedRectangle` (border), `g.drawText` (label).

3.  **Update `paint`:**
    *   Remove dynamic simulation logic.
    *   Keep "Live Input" overlays (Yellow/Cyan highlights for pressed keys) on top of the cached image.

**Outcome:**
The Visualizer will now accurately reflect the "Photoshop Layers" logic we tested, showing dimmed keys for inherited mappings and red keys for conflicts.