# 🤖 Cursor Prompt: Phase 40 - Layer Engine Architecture (Backend)

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 39.8 Complete. `InputProcessor` uses `compiledMap` (HardwareID) for Audio and `configMap` (Alias) for UI.
*   **Phase Goal:** Implement a **Multi-Layer System** (Banks/Modifiers).
    *   Allow creating multiple "Layers" of mappings.
    *   Layer 0 is "Base". Layers 1-8 are "Overlays" (Shift, Alt, Drum Bank, etc.).
    *   Runtime checks active layers from Top to Bottom.

**Strict Constraints:**
1.  **Dual Map Per Layer:** Each `Layer` struct must contain its own `compiledMap` and `configMap`.
2.  **Priority Logic:**
    *   **1. Active Layers (Desc):** Check specific mappings in Layers 8 down to 0.
    *   **2. Zones:** If no manual mapping found in *any* active layer, check `ZoneManager`.
3.  **Command Handling:** `processEvent` must handle `LayerMomentary`, `LayerToggle`, `LayerSolo` commands to change the `isActive` state of layers.

---

### Step 1: Update `InputProcessor.h`
Refactor storage to support layers.

**1. Define `Struct Layer`:**
```cpp
struct Layer {
    juce::String name;
    int id = 0;
    bool isActive = false; // Runtime state
    
    // The Maps
    std::unordered_map<InputID, MidiAction> compiledMap; // HardwareID -> Action
    std::unordered_map<InputID, MidiAction> configMap;   // AliasHash -> Action
};
```

**2. Update Members:**
*   Remove the top-level `compiledMap` and `configMap`.
*   Add `std::vector<Layer> layers;` (Size 8 by default).
*   Add `std::atomic<int> activeLayerMask;` (Bitmask optimization? Or just iterate vector boolean. Vector bool is fine for 8 items).

### Step 2: Update `InputProcessor.cpp`
Refactor the Core Logic.

**1. `rebuildMapFromTree` (The Compiler):**
*   Clear all layers.
*   Ensure Layer 0 exists and `isActive = true`.
*   Iterate XML.
    *   Read `layerID` property from Mapping (Default 0).
    *   Resize `layers` vector if needed.
    *   Populate the specific `layers[id].compiledMap` and `configMap`.

**2. `processEvent` (The Audio Loop):**
*   **Loop:** Iterate `layers` from `size()-1` down to `0`.
*   **Check:** `if (!layer.isActive) continue;`
*   **Lookup:** Check `layer.compiledMap`.
*   **Hit:** Match found? Trigger Action. **Return.**
*   **Commands:** If the Action is `LayerMomentary(id)`, set `layers[id].isActive = isDown`.
*   **Fallthrough:** If loop finishes, call `zoneManager.handleInput`.

**3. `simulateInput` (The Visualizer):**
*   Accept `viewDeviceHash`.
*   **Loop:** Iterate `layers` from High to Low.
*   **Check:** `if (!layer.isActive) continue;` (Visualizer should show current reality).
*   **Lookup:** Check `layer.configMap`.
*   **Hit:** Return `VisualState::Active` (or Override logic).

### Step 3: Update `MappingTypes.h`
Add Layer Commands.
*   `LayerMomentary`, `LayerToggle`, `LayerSolo`. (Data1 = Layer ID).

---

**Generate code for: Updated `MappingTypes.h`, `InputProcessor.h`, and `InputProcessor.cpp`.**