This prompt rewrites the `GridCompiler::compile` loop to strictly follow the **Vertical Stacking (Photoshop)** logic. This is the foundation for getting the `VerticalInheritance` tests passing.

We are writing the main loop **from scratch** to ensure no old, buggy logic remains.

### 📋 Cursor Prompt: Phase 51.4 - Implementing Vertical Stacking

**Target File:** `Source/GridCompiler.cpp`

**Task:**
Rewrite the `GridCompiler::compile` method to correctly handle **Layer Inheritance** (Vertical Logic).
We are focusing strictly on the **Global Context (Hash 0)** first to fix the basic inheritance tests.

**Specific Instructions:**

1.  **Refactor `compile` Structure:**
    Delete the existing loop body. Replace it with this logic flow:

    ```cpp
    // 1. Setup Context
    auto context = std::make_shared<CompiledMapContext>();
    
    // Ensure vectors are sized for 9 layers
    // (Helper function to initialize vectors if needed)
    
    // 2. Define Helper Lambda "applyLayerToGrid"
    // This encapsulates the logic of adding Zones + Mappings to a specific grid instance.
    auto applyLayerToGrid = [&](VisualGrid& vGrid, AudioGrid& aGrid, int layerId, uintptr_t aliasHash) {
        // Track which keys are touched in THIS layer for conflict detection
        std::vector<bool> touchedKeys(256, false); 

        // A. Zones
        compileZonesForLayer(vGrid, aGrid, zoneMgr, aliasHash, layerId, touchedKeys, context->chordPool);
        
        // B. Manual Mappings (Higher priority than Zones in same layer)
        compileMappingsForLayer(vGrid, aGrid, presetMgr, aliasHash, layerId, touchedKeys, context->chordPool);
    };

    // 3. PASS 1: Compile Global Stack (Vertical)
    // We treat Layer 0 as base. Layer 1 inherits from Layer 0, etc.
    uintptr_t globalHash = 0;
    
    // Ensure the visualLookup map has the vector for globalHash
    context->visualLookup[globalHash].resize(9);

    for (int L = 0; L < 9; ++L) {
        // Create new Grids for this layer
        auto vGrid = std::make_shared<VisualGrid>();
        auto aGrid = std::make_shared<AudioGrid>();

        // INHERITANCE LOGIC (Rule 0 & 2)
        if (L > 0) {
            // Copy from Layer Below
            *vGrid = *context->visualLookup[globalHash][L-1];
            *aGrid = *context->globalGrids[L-1];

            // VISUAL STATE TRANSITION
            // Everything that was "Active" or "Override" below becomes "Inherited" here.
            for (auto& slot : *vGrid) {
                if (slot.state != VisualState::Empty) {
                    slot.state = VisualState::Inherited;
                }
            }
        }

        // APPLY CURRENT LAYER CONTENT (Overrides inherited data)
        applyLayerToGrid(*vGrid, *aGrid, L, globalHash);

        // Store
        context->visualLookup[globalHash][L] = vGrid;
        context->globalGrids[L] = aGrid;
    }

    // (Pass 2 for Devices will go here later)

    return context;
    ```

2.  **Update Helpers (`compileZonesForLayer`, `compileMappingsForLayer`):**
    *   Update signature to accept `std::vector<bool>& touchedKeys` and `chordPool`.
    *   **Conflict Logic (Rule 4):**
        *   Inside the helper loops, before writing to a slot:
        *   `if (touchedKeys[keyCode]) { mark CONFLICT; continue; }`
        *   `touchedKeys[keyCode] = true;`
    *   **State Logic:**
        *   When writing a new active mapping:
        *   If `slot.state == VisualState::Empty` -> Set `Active`.
        *   If `slot.state == VisualState::Inherited` -> Set `Override`. (This indicates we are masking a lower layer).

**Goal:**
This logic ensures that Layer 5 contains everything from Layers 0-4 (dimmed as `Inherited`) plus its own mappings (`Active` or `Override`). This should make `VerticalInheritance` and `VerticalOverride` tests pass.