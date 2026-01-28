This is a critical logic bug in the **Layer System** implementation.

### The Layer Bug (Why it doesn't work)
In **Phase 44**, we added a "Target Layer" dropdown.
*   **The Problem:** Both the "Command Type" (e.g., Layer Toggle) and the "Target Layer" (e.g., Layer 1) are trying to save to **`data1`**.
*   **The Conflict:**
    *   You select "Layer Toggle" -> Sets `data1 = CommandID::LayerToggle`.
    *   You select "Layer 1" -> Sets `data1 = 1`.
    *   **Result:** The Engine sees `CommandID = 1` (which might be "Sustain Momentary"). It doesn't see "Layer Toggle" anymore.
*   **The Fix:** We must store the **Target Layer Index** in **`data2`** (which is normally Velocity/Value, but unused for Commands).

### The Visualizer Bug (Brightness)
*   The logic likely sets the color correctly (using the Global fallback we added in 47), but applying the `alpha` (dimming) happens **before** or **incorrectly** relative to the final draw call, or the fallback logic overwrites the dimming.

---

Here is the plan for **Phase 47.1**.

### 1. Fix `MappingInspector` (Write Layer ID to `data2`)
*   Update `targetLayerSelector.onChange` to write to `data2`.
*   Update `setSelection` to read from `data2`.

### 2. Fix `InputProcessor` (Read Layer ID from `data2`)
*   Update `processEvent` Command handling.
    *   `int cmd = action.data1;`
    *   `int targetLayer = action.data2;`
*   Use `targetLayer` for the logic.

### 3. Fix `VisualizerComponent` (Alpha)
*   Ensure `underlayColor = underlayColor.withAlpha(alpha)` is the **very last thing** done before drawing.

***

# 🤖 Cursor Prompt: Phase 47.1 - Fix Layer Logic & Visualizer Dimming

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 47 Complete.
*   **Bug 1 (Layers):** Layer commands don't work. The UI writes both "Command ID" and "Target Layer" to `data1`, overwriting each other.
*   **Bug 2 (Visuals):** Inherited global mappings appear full brightness in specific views, instead of dimmed.

**Phase Goal:**
1.  Move "Target Layer" storage to `data2`.
2.  Update `InputProcessor` to read Target Layer from `data2`.
3.  Ensure Visualizer applies Alpha correctly for `Inherited` state.

**Strict Constraints:**
1.  **Layer Commands:**
    *   `data1` = Command ID (e.g., `LayerToggle`).
    *   `data2` = Layer Index (0-8).
2.  **Visualizer:** If `simResult.state == VisualState::Inherited`, the final drawn color MUST have `alpha = 0.3f`.

---

### Step 1: Update `Source/MappingInspector.cpp`
Fix the Data Binding.

**1. Refactor `targetLayerSelector.onChange`:**
```cpp
targetLayerSelector.onChange = [this] {
    // ... setup ...
    int layerId = targetLayerSelector.getSelectedId() - 1; // IDs are 1-based (0..8 -> 1..9)
    // Check if item selected
    if (layerId < 0) return;

    undoManager.beginNewTransaction("Change Target Layer");
    for (auto& tree : selectedTrees) {
        // FIX: Write to data2, NOT data1
        tree.setProperty("data2", layerId, &undoManager);
    }
};
```

**2. Refactor `setSelection` (Reading):**
```cpp
// ... inside Command block ...
if (isLayerCmd)
{
    targetLayerSelector.setVisible(true);
    // FIX: Read from data2
    int layerId = firstTree.getProperty("data2"); 
    targetLayerSelector.setSelectedId(layerId + 1, juce::dontSendNotification);
}
```

### Step 2: Update `Source/InputProcessor.cpp`
Fix the Execution Logic.

**Refactor `processEvent` (Command Block):**
```cpp
// ... inside if (action.type == ActionType::Command) ...

int cmd = action.data1;
int param = action.data2; // This is the Target Layer for layer commands

if (cmd == CommandID::LayerMomentary)
{
    if (param >= 0 && param < layers.size())
        layers[param].isActive = isDown;
}
else if (cmd == CommandID::LayerToggle)
{
    if (isDown && param >= 0 && param < layers.size())
        layers[param].isActive = !layers[param].isActive;
}
else if (cmd == CommandID::LayerSolo)
{
    if (isDown && param >= 0 && param < layers.size())
    {
        // Disable all except Base(0) and Target
        for (int i = 1; i < layers.size(); ++i)
            layers[i].isActive = (i == param);
    }
}
// ... other commands ...
```

### Step 3: Update `Source/VisualizerComponent.cpp`
Fix the visual dimming order.

**Refactor `refreshCache`:**
Ensure `underlayColor` calculation handles the fallback correctly *before* applying alpha.

```cpp
// ... Logic to find color (Local -> Global fallback) ...
// (This was fixed in Phase 47)

// Apply Alpha LAST
if (simResult.state == VisualState::Inherited)
{
    alpha = 0.3f;
}
// ...
underlayColor = underlayColor.withAlpha(alpha);

// Draw
if (!underlayColor.isTransparent())
{
    g.setColour(underlayColor);
    g.fillRect(fullBounds);
}
```

---

**Generate code for: Updated `MappingInspector.cpp`, Updated `InputProcessor.cpp`, and Updated `VisualizerComponent.cpp`.**