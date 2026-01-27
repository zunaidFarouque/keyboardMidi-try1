# 🤖 Cursor Prompt: Phase 39.1 - Visualizer Hierarchy Fix

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 39 implemented `simulateInput`, but the logic is flawed.
*   **Symptoms:**
    1.  Laptop View shows Overrides as Conflicts (Red).
    2.  Laptop View fails to show Inherited mappings.
    3.  Master View might be showing specific device mappings (leaking).
*   **Phase Goal:** Rewrite `InputProcessor::simulateInput` to strictly follow the 4-layer hierarchy and return a precise `VisualState` enum. Update `VisualizerComponent` to render this state.

**Strict Constraints:**
1.  **Isolation:** When View is "Master" (`hash == 0`), `simulateInput` must **ONLY** check Global Mapping and Global Zone. Do not check specific devices.
2.  **Inheritance:** When View is "Specific" (`hash != 0`), check Specific first. If empty, check Global (and mark as `Inherited`).
3.  **Override Detection:** If Specific exists AND Global exists, mark as `Override`.

---

### Step 1: Update `MappingTypes.h`
Create a clear state enum for the Visualizer.

```cpp
enum class VisualState {
    Empty,
    Active,     // Defined locally, no global conflict
    Inherited,  // Undefined locally, using global
    Override,   // Defined locally, masking global
    Conflict    // (Optional) Hard error state
};

struct SimulationResult {
    std::optional<MidiAction> action;
    VisualState state = VisualState::Empty;
    juce::String sourceName; // e.g. "Mapping" or "Zone: Main"
    // Helper to know if it's a Zone or Mapping for coloring
    bool isZone = false; 
};
```

### Step 2: Update `InputProcessor.cpp`
Implement the strict 4-layer check.

**Refactor `simulateInput(uintptr_t viewDeviceHash, int key)`:**

```cpp
SimulationResult InputProcessor::simulateInput(uintptr_t viewDevice, int key)
{
    SimulationResult res;
    juce::ScopedReadLock lock(mapLock);

    // 1. Check Specific Manual
    auto specificMap = (viewDevice != 0) ? keyMapping.find({viewDevice, key}) : keyMapping.end();
    bool hasSpecificMap = (specificMap != keyMapping.end());

    // 2. Check Global Manual
    auto globalMap = keyMapping.find({0, key});
    bool hasGlobalMap = (globalMap != keyMapping.end());

    // 3. Check Specific Zone
    auto specificZone = (viewDevice != 0) ? zoneManager.handleInput({viewDevice, key}) : std::nullopt;
    
    // 4. Check Global Zone
    auto globalZone = zoneManager.handleInput({0, key});

    // --- LOGIC TREE ---

    if (viewDevice == 0) // MASTER VIEW
    {
        if (hasGlobalMap) {
            res.action = globalMap->second;
            res.state = VisualState::Active;
            res.sourceName = "Global Mapping";
        }
        else if (globalZone) {
            res.action = globalZone;
            res.state = VisualState::Active;
            res.sourceName = "Global Zone";
            res.isZone = true;
        }
        return res;
    }

    // SPECIFIC VIEW
    
    // A. Specific Manual (Highest Priority)
    if (hasSpecificMap) {
        res.action = specificMap->second;
        res.sourceName = "Mapping";
        // Check Override
        if (hasGlobalMap || globalZone) res.state = VisualState::Override;
        else res.state = VisualState::Active;
        return res;
    }

    // B. Global Manual (Inheritance)
    if (hasGlobalMap) {
        res.action = globalMap->second;
        res.state = VisualState::Inherited;
        res.sourceName = "Global Mapping (Inherited)";
        return res;
    }

    // C. Specific Zone
    if (specificZone) {
        res.action = specificZone;
        res.sourceName = "Zone";
        res.isZone = true;
        if (globalZone) res.state = VisualState::Override;
        else res.state = VisualState::Active;
        return res;
    }

    // D. Global Zone (Inheritance)
    if (globalZone) {
        res.action = globalZone;
        res.state = VisualState::Inherited;
        res.sourceName = "Global Zone (Inherited)";
        res.isZone = true;
        return res;
    }

    return res;
}
```

### Step 3: Update `VisualizerComponent.cpp`
Render the new states.

**Refactor `paint` loop:**
1.  Call `simulateInput(currentViewHash, key)`.
2.  **Colors:**
    *   **Fill:**
        *   If `Empty`: Dark Grey.
        *   If `Inherited`: Desaturated/Dim version of the Type Color. (e.g., `Colours::skyblue.withAlpha(0.3f)`).
        *   If `Active/Override`: Full Type Color. (Use `settingsManager.getTypeColor` if Mapping, or Zone Color if Zone).
    *   **Border:**
        *   If `Override`: **Orange** (2px).
        *   If `Active`: **Light Grey** (1px).
        *   If `Inherited`: **Dark Grey** (1px).
3.  **Conflict:**
    *   You can remove the old complex conflict logic for now. The `Override` state covers the most common "Conflict" (Intention).
    *   *Real* conflicts (Double Mapping) are prevented by the Map structure anyway.

---

**Generate code for: `MappingTypes.h`, `InputProcessor.cpp`, and `VisualizerComponent.cpp`.**