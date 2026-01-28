# 🤖 Cursor Prompt: Phase 45.3 - Visualizer Layer Synchronization

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 45.2 Complete. The Visualizer always simulates the "Live" state (checking all active layers).
*   **Problem:** When editing "Layer 1", the Visualizer might still show Layer 0's mappings as "Active", or ignore Layer 1 if it's currently toggled off in the engine.
*   **Phase Goal:** Update the Visualizer to show the **Editing Context**.
    *   If User selects Layer 1: Visualize Layer 1 (Active) -> Layer 0 (Inherited).
    *   If User selects Layer 0: Visualize Layer 0 (Active).

**Strict Constraints:**
1.  **Simulation Logic:** `simulateInput` must accept a `targetLayerId`. It should iterate from `targetLayerId` down to `0`.
    *   If mapping found at `targetLayerId`: State is **Active** (or Override).
    *   If mapping found at `< targetLayerId`: State is **Inherited**.
2.  **Wiring:** `MainComponent` must connect the `MappingEditor`'s selection change to the `Visualizer`.

---

### Step 1: Update `InputProcessor` (`Source/InputProcessor.cpp`)
Update the simulation logic to handle the layer stack.

**Refactor `simulateInput`:**
*   **Signature:** `SimulationResult simulateInput(uintptr_t viewDevice, int key, int targetLayerId);`
*   **Logic:**
    1.  **Loop:** `i` from `targetLayerId` down to `0`.
    2.  Access `layers[i]`. (Ignore `isActive` flag here? **Yes**, because we want to see what *would* happen if we turned the layer on).
    3.  **Check Maps (`configMap`):**
        *   **Specific Device (`viewDevice`):**
            *   Found?
                *   If `i == targetLayerId`: Mark **Active** (or Override/Conflict if lower layers exist).
                *   If `i < targetLayerId`: Mark **Inherited**.
                *   Return Result.
        *   **Global Device (0):**
            *   Found?
                *   If `i == targetLayerId` && `viewDevice == 0`: **Active**.
                *   If `i == targetLayerId` && `viewDevice != 0`: **Inherited** (From Global).
                *   If `i < targetLayerId`: **Inherited**.
                *   Return Result.
    4.  **Fallback:** Check Zones (as before).

### Step 2: Update `VisualizerComponent` (`Source/VisualizerComponent.h/cpp`)
Store the context.

**1. Header:**
*   Member: `int currentVisualizedLayer = 0;`
*   Method: `void setVisualizedLayer(int layerId);`

**2. Implementation:**
*   `setVisualizedLayer`: Update member, set `cacheValid = false`, `needsRepaint = true`.
*   `paint` (inside loop): Call `inputProcessor.simulateInput(currentViewHash, keyCode, currentVisualizedLayer)`.

### Step 3: Update `MappingEditorComponent` (`Source/MappingEditorComponent.h/cpp`)
Expose the selection event.

**1. Header:**
*   `std::function<void(int)> onLayerChanged;`

**2. Constructor:**
*   Inside `layerList.onLayerSelected` lambda:
    *   `if (onLayerChanged) onLayerChanged(newLayerId);`

### Step 4: Update `MainComponent.cpp`
Wire it up.

**Constructor:**
*   `mappingEditor->onLayerChanged = [this](int layerId) { visualizer->setVisualizedLayer(layerId); };`

---

**Generate code for: Updated `InputProcessor.h/cpp`, Updated `VisualizerComponent.h/cpp`, Updated `MappingEditorComponent.h/cpp`, and Updated `MainComponent.cpp`.**