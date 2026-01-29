### 📋 Cursor Prompt: Fix Visual Inheritance (Interleaved Stacking)

**Target File:** `Source/GridCompiler.cpp`

**Task:**
Rewrite the visual compilation loop in `GridCompiler::compile` to correctly handle the interleaved priority of Global vs Specific Device mappings.

**Current Problem:**
Currently, we stack layers vertically (`Layer 0 -> Layer 1`).
However, for a specific device, the priority rules are: `Device[L] > Global[L] > Device[L-1] > Global[L-1]`.
The current implementation doesn't merge Global into Device correctly at every step.

**Required Logic:**

1.  **Phase 1: Compile Global Grids First.**
    *   Iterate Layers `L = 0` to `8`.
    *   `Grid = (L > 0) ? copy(visualLookup[0][L-1]) : Empty`.
    *   Apply `Global Zones[L]`.
    *   Apply `Global Mappings[L]`.
    *   Store in `visualLookup[0][L]`.

2.  **Phase 2: Compile Specific Device Grids.**
    *   Iterate `Alias != 0`.
    *   Iterate Layers `L = 0` to `8`.
    *   **The Fix:** Do not just copy `Device[L-1]`.
    *   **Base:** Start with `visualLookup[0][L]` (The Global grid at this layer).
        *   *Why?* Because `Global[L]` > `Device[L-1]`. This correctly "pulls in" the global state.
    *   **Re-Apply Device Stack:**
        *   We must re-apply *all* lower device layers to ensure `Device[0]` overwrites `Global[0]` (which is inside `Global[L]`).
        *   Loop `k = 0` to `L`:
            *   Apply `Device[Alias] Zones[k]`.
            *   Apply `Device[Alias] Mappings[k]`.
    *   Store in `visualLookup[Alias][L]`.

**Specific Implementation Steps:**

1.  Modify `compile` to run two distinct passes:
    *   **Pass 1:** Global (Hash 0) only.
    *   **Pass 2:** Specific Aliases (Hash != 0).

2.  Refactor the "Apply Zones/Mappings" logic into a reusable lambda or helper that takes a `VisualGrid&` and a `layerId`, so we can call it inside the inner loops efficiently.

3.  **Optimization:**
    For Phase 2 (Device), instead of re-applying `k=0..L` every time:
    `VisualGrid[Alias][L]` = `VisualGrid[Global][L]` merged with `VisualGrid[Alias][L] (Local Only)`?
    No, sticking to the **Re-Apply Loop** (`k=0..L`) is safer and cleaner to implement, ensuring exact priority match with the Audio Engine. The performance cost is negligible.

**Code Skeleton for the Loop:**

```cpp
// Helper to apply a single layer's content to a grid
auto applyLayerContent = [&](std::shared_ptr<VisualGrid> grid, uintptr_t aliasHash, int layerId) {
    // 1. Compile Zones for (alias, layer) -> write to grid
    compileZonesForLayer(*grid, zoneMgr, aliasHash, layerId, settings);
    // 2. Compile Mappings for (alias, layer) -> write to grid
    compileMappingsForLayer(*grid, presetMgr, aliasHash, layerId, settings);
};

// 1. Compile Global Chain
uintptr_t globalHash = 0;
for (int L = 0; L < 9; ++L) {
    auto grid = std::make_shared<VisualGrid>();
    if (L > 0) *grid = *context->visualLookup[globalHash][L-1]; // Inherit previous Global
    
    applyLayerContent(grid, globalHash, L);
    context->visualLookup[globalHash][L] = grid;
}

// 2. Compile Device Chains
for (uintptr_t aliasHash : aliases) {
    if (aliasHash == 0) continue; // Skip global, already done

    for (int L = 0; L < 9; ++L) {
        // Start with the Global state at this layer (Global L > Device L-1)
        auto grid = std::make_shared<VisualGrid>(*context->visualLookup[globalHash][L]);
        
        // Re-apply specific device stack from 0 to L (Device K > Global K)
        for (int k = 0; k <= L; ++k) {
            applyLayerContent(grid, aliasHash, k);
        }
        
        context->visualLookup[aliasHash][L] = grid;
    }
}
```

Please replace the existing main loop in `GridCompiler.cpp` with this logic.