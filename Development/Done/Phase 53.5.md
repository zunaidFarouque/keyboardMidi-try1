### 📋 Cursor Prompt: Phase 53.5 - Fixing Inheritance Logic

**Target File:** `Source/GridCompiler.cpp`

**Task:**
Refactor `GridCompiler::compile` to fix two specific logic errors:
1.  **Layer Command Leaking:** Layer switching commands must **not** be copied to higher layers during inheritance (they stay on their specific layer).
2.  **Device Inheritance Brightness:** When compiling a specific device view for Layer $L$, mappings from lower layers ($0 \dots L-1$) must be marked as `Inherited` (Dim), not `Active` (Bright).

**Specific Instructions:**

1.  **Update `applyLayerToGrid` Lambda:**
    *   Add a parameter: `VisualState targetState`.
    *   Update the `compileZones` and `compileMappings` calls to pass this state down (you might need to update those helper signatures too, or just handle the logic inside the lambda if you refactor).
    *   **Logic Change when writing to Visual Slot:**
        *   Current logic: `if (empty) Active else Override`.
        *   **New Logic:**
            ```cpp
            if (slot.state == VisualState::Empty) {
                slot.state = targetState; 
            } else {
                // Slot occupied (e.g. by Global). We are writing on top.
                // If we are writing "Active" content (current layer), it's an Override.
                // If we are writing "Inherited" content (lower layer), it stays Inherited.
                if (targetState == VisualState::Active) slot.state = VisualState::Override;
                else slot.state = VisualState::Inherited;
            }
            ```

2.  **Update Pass 1 (Global Vertical Stack):**
    *   Find the loop where `if (L > 0) ... *vGrid = *context->visualLookup...`.
    *   **Add Filtering:** After copying, iterate the new `vGrid`.
    *   If `slot` contains a Layer Command (`ActionType::Command` and `data1` is `LayerMomentary/Toggle/Solo`), reset the slot to `VisualState::Empty`.
    *   *Correction:* Check the `AudioGrid` slot for the type/data, then clear both Audio and Visual slots if it matches.
    *   **Call apply:** `applyLayerToGrid(..., VisualState::Active);`

3.  **Update Pass 2 (Device Horizontal Stack):**
    *   Find the nested loop `for (int k = 0; k <= L; ++k)`.
    *   **Logic:**
        ```cpp
        VisualState stateForPass = (k < L) ? VisualState::Inherited : VisualState::Active;
        applyLayerToGrid(*vGrid, *aGrid, k, devHash, stateForPass);
        ```

4.  **Helper Update (`compileMappingsForLayer`):**
    *   Ensure it uses the `targetState` passed from `applyLayerToGrid` when creating new slots.

**Summary of Behavior:**
*   Global Layer 0 has "Layer 1 Momentary".
*   Pass 1, Layer 1: Copies Layer 0. sees "Layer 1 Momentary". Deletes it. Maps Layer 1 stuff. **(Fixes Leak)**.
*   Device "Laptop", View Layer 1.
*   Base = Global Layer 1.
*   Apply Device Layer 0: Passed `targetState = Inherited`. Writes dim keys. **(Fixes Brightness)**.
*   Apply Device Layer 1: Passed `targetState = Active`. Writes bright keys.

**Run tests after this.** All 14 tests (including the 2 new ones) should pass.