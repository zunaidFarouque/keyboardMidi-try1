# 🧠 Phase 50: The Unified Grid Architecture (Design & Strategy)

This is the blueprint for the **Massive Refactor**. We are moving from a "Query-based" architecture (calculate on request) to a "Compiler-based" architecture (pre-calculate everything).

### 🚀 The Core Concept: "The Grid"
Instead of `compiledMap` and `configMap`, we will have **Grids**.
A **Grid** is a simple array of 256 slots (one for every possible Virtual Key Code, including distinct L/R Modifiers).

We separate the data into two formats optimized for their specific threads:

1.  **`AudioGrid`**: Extremely lightweight. Optimized for CPU cache. Read by Audio Thread.
2.  **`VisualGrid`**: Rich data (Colors, Labels, Strings). Read by UI Thread.

---

## 1. New Data Structures

### A. The Atoms
```cpp
// 1. The Audio Atom (16-32 bytes max)
// Strictly POD (Plain Old Data). No strings, no vectors.
struct KeyAudioSlot {
    bool isActive = false;      // Is anything mapped here?
    ActionType type;            // Note, CC, Command...
    int channel;
    int data1;                  // Note Number / CC Index
    int data2;                  // Velocity / Value
    // For Complex types (Envelopes, Macros), we store an index 
    // to a separate lookup table to keep this struct small.
    int extendedDataIndex = -1; 
};

// 2. The Visual Atom (Heavier, contains UI state)
struct KeyVisualSlot {
    VisualState state = VisualState::Empty; // Active, Inherited, Override, Conflict
    juce::Colour displayColor;              // Pre-calculated final color
    juce::String label;                     // Pre-calculated text ("C#", "I", "Macro 1")
    juce::String sourceName;                // "Zone: Main", "Mapping: Base"
};
```

### B. The Containers
```cpp
// A flat array covering all Virtual Key Codes (0x00 to 0xFF)
// 0xA0 is LSHIFT, 0xA1 is RSHIFT. No ambiguity.
using AudioGrid = std::array<KeyAudioSlot, 256>;
using VisualGrid = std::array<KeyVisualSlot, 256>;

// The Compiler Output
struct CompiledContext {
    // 1. Hardware Lookup: Map<HardwareHash, AudioGrid>
    // Audio thread finds the Grid for the device, then indexes by KeyCode. O(1).
    std::unordered_map<uintptr_t, std::shared_ptr<const AudioGrid>> deviceGrids;
    
    // 2. Global Fallback: The Grid for "Any Device"
    std::shared_ptr<const AudioGrid> globalGrid;

    // 3. Visual Lookups: Map<AliasHash, Map<LayerID, VisualGrid>>
    // UI thread requests: visualLookup[AliasHash][LayerID]
    std::unordered_map<uintptr_t, std::vector<std::shared_ptr<const VisualGrid>>> visualLookup;
};
```

---

## 2. The New Workflow

### Step A: The "Compiler" (Config Change)
Triggered when: Preset Load, Mapping Change, Zone Change, Device Alias Change.
*Running on: Background Thread or Message Thread (Blocking audio briefly is acceptable, but swapping pointers is lock-free).*

1.  **Instantiate** a new `CompiledContext`.
2.  **Iterate** every defined Alias (and the Global 0 hash).
3.  **Iterate** every Layer (0 to 8).
4.  **Iterate** every KeyCode (0 to 255).
    *   **Logic:**
        1.  Check **Manual Mapping** in current Layer. Found? -> Write to Grid.
        2.  Check **Zone** in current Layer. Found? -> Calculate Chord/Note, Write to Grid.
        3.  Check **Lower Layers** (Inheritance).
    *   **Visual Compilation:** While calculating, determine if the value came from the current layer (Active) or below (Inherited). If current layer has a value but lower layer also did, mark (Override).
    *   **Asset Baking:** Pre-render the key label (e.g., "C# Maj7") into the `VisualGrid` struct now. Do not calculate strings at 60fps.
5.  **Swap:** Atomically swap the `CompiledContext` pointer in `InputProcessor`.

### Step B: Audio Processing (`processEvent`)
*Running on: High Priority Audio/Input Thread.*

1.  **Input:** `(DeviceHandle, KeyCode, IsDown)`
2.  **Lookup:**
    *   `InputProcessor` holds `std::shared_ptr<const CompiledContext>`.
    *   Does `deviceGrids` have `DeviceHandle`?
        *   Yes: Use `AudioGrid` from device.
        *   No: Use `globalGrid`.
3.  **Fetch:** `slot = grid[KeyCode]`.
4.  **Execute:** If `slot.isActive`, send to `VoiceManager`.
5.  **Performance:** Array access is instant. No tree traversal. No hash collisions.

### Step C: Visualization (`paint`)
*Running on: Message Thread.*

1.  **Context:** User selects "Alias: Laptop", "Layer: 2".
2.  **Lookup:** `currentGrid = context->visualLookup[LaptopHash][2]`.
3.  **Render:**
    *   Iterate `KeyboardLayout`.
    *   `slot = currentGrid[KeyCode]`.
    *   Draw `slot.displayColor`, `slot.label`, `slot.state`.
4.  **Performance:** No logic. Just iterating data structs and drawing rects.

---

## 3. Impact Analysis & Changes Required

### 1. `MappingTypes.h` (High Impact)
*   **Action:** Add `VK_LSHIFT` (0xA0) through `VK_RMENU` definitions if missing.
*   **Action:** Define `AudioGrid` and `VisualGrid` structs.

### 2. `InputProcessor` (Complete Rewrite)
*   **Remove:** `rebuildMapFromTree`, `findMapping`, `simulateInput`.
*   **Add:** `GridCompiler` inner class (or separate helper).
*   **Add:** `performAtomicSwap(newContext)`.
*   **Logic:** The complex logic currently scattered across `simulateInput` and `rebuildMapFromTree` moves into the **Compiler**.

### 3. `VisualizerComponent` (Medium Impact)
*   **Remove:** `refreshCache` logic that calculates colors.
*   **Refactor:** `paint` simply reads from `VisualGrid`.
*   **Optimization:** `backgroundCache` can now be a simple screenshot of the `VisualGrid` drawn once when the grid changes.

### 4. `VoiceManager` (Low Impact)
*   Remains mostly the same, but receives cleaner, pre-validated data.

### 5. `RawInputManager` (Low Impact - Specific Fix)
*   **Fix:** Ensure `VK_SHIFT` is not sent. Always send `VK_LSHIFT` or `VK_RSHIFT`.
*   **Grid:** Since the Grid is 256 wide, it handles specific keys naturally. If we want a "Generic Shift" mapping, the **Compiler** simply copies the mapping to *both* 0xA0 and 0xA1 slots during compilation.

---

## 4. Trade-offs

| Benefit | Cost |
| :--- | :--- |
| **Speed:** Audio thread becomes strict O(1). | **Memory:** Higher RAM usage. (256 keys * ~64 bytes * 9 Layers * 5 Aliases ≈ 1MB). Negligible. |
| **Consistency:** Audio and Visuals can never disagree. | **Compilation Lag:** Changing a setting might take 10-20ms to rebuild grids (vs 0ms now). acceptable. |
| **Simplicity:** Visualization code becomes trivial. | **Complexity:** The "Compiler" logic becomes the most complex function in the app. |

---

## 5. Implementation Plan (Phase 50 Breakdown)

We will execute this in strict order to maintain stability.

1.  **Phase 50.1: The Data Layer**
    *   Define `KeyAudioSlot`, `KeyVisualSlot` in `MappingTypes.h`.
    *   Define the `CompiledContext` structure.

2.  **Phase 50.2: The Compiler Logic (Skeleton)**
    *   Create `GridCompiler` class.
    *   Implement the iterator: Layers -> Keys.
    *   Implement "Manual Mapping" compilation first.

3.  **Phase 50.3: Zone Integration**
    *   Port `Zone::processKey` logic into the Compiler.
    *   Instead of calculating chords on keypress, calculate them *into the AudioGrid* extended data.

4.  **Phase 50.4: Visual Baking**
    *   Port color/label logic into the Compiler.
    *   Handle "Generic Modifier" replication (copying Shift settings to L/R slots).

5.  **Phase 50.5: Wiring InputProcessor**
    *   Replace the old maps with the new `CompiledContext`.
    *   Update `processEvent`.

6.  **Phase 50.6: Wiring Visualizer**
    *   Rewrite `VisualizerComponent` to use `VisualGrid`.

---

## 🚦 Ready?
I will start with **Phase 50.1: Defining the Data Structures**. This is the foundation.
Shall I proceed?