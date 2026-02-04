This is a State Machine problem. You are correctly identifying that **Edge-Triggered** logic (On Press -> Toggle) fails in complex dependency chains. We must switch to **State-Triggered** logic (Re-evaluate "What should be active?" based on "What is currently held?").

Here is the plan for **Phase 40.1**.

### The Solution: Continuous Layer Evaluation
We will change `InputProcessor` to stop trusting the "Previous State" and instead calculate the "Correct State" from scratch on every input change.

1.  **Tracking:** We maintain a `std::set<InputID> heldKeys`.
2.  **Separation:** We split Layer State into:
    *   `latchedState`: Layers toggled ON by `LayerToggle` commands (Persistent).
    *   `momentaryState`: Layers computed dynamically from held keys (Transient).
3.  **The Loop (Recursion Solver):**
    *   Start with `Active = Base + Latched`.
    *   **Pass 1:** Check all `heldKeys` against currently Active layers. Gather all `LayerMomentary` targets. Add them to Active.
    *   **Pass 2:** Check `heldKeys` again against the *new* Active list (to catch keys like "Shift" inside "Ctrl" layer). Add new targets.
    *   *Repeat until stable.* (Max 3-4 passes is enough for any sane layout).

***

# 🤖 Cursor Prompt: Phase 40.1 - Robust Layer State Machine

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Phase 40 Complete. Layers toggle based on key events directly.
*   **The Bug:** Complex layer dependencies (Base->Ctrl->Shift vs Base->Shift->Ctrl) get desynchronized when keys are released in a different order than pressed. "Orphaned" layers remain active.
*   **Phase Goal:** Implement a **State-Reconstruction Engine**. Instead of toggling layers on/off, we recalculate the entire layer stack state whenever a key is pressed or released.

**Strict Constraints:**
1.  **Held Keys:** `InputProcessor` must track `std::set<InputID> currentlyHeldKeys`.
2.  **Layer State:** Split `Layer` struct's `isActive` into `bool isLatched` (User toggled) and `bool isMomentary` (Calculated). `isActive` becomes a getter: `return isLatched || isMomentary`.
3.  **Recalculation:** Implement `void updateLayerState()` that runs iteratively to resolve dependencies (e.g., Layer A enables Layer B).
4.  **Performance:** Run this recalculation synchronously on the Input Thread (it's fast enough).

---

### Step 1: Update `InputProcessor.h`

**1. Update `Layer` struct:**
```cpp
struct Layer {
    // ... existing ...
    bool isLatched = false;   // Toggled by Command
    bool isMomentary = false; // Held by Key
    bool isActive() const { return id == 0 || isLatched || isMomentary; } // Base always on
};
```

**2. Update `InputProcessor` members:**
*   `std::set<InputID> currentlyHeldKeys;`
*   `void updateLayerState();`

### Step 2: Update `InputProcessor.cpp`

**1. Implement `updateLayerState`:**
```cpp
void InputProcessor::updateLayerState()
{
    juce::ScopedWriteLock lock(mapLock);
    
    // 1. Reset Momentary
    for (auto& layer : layers) layer.isMomentary = false;
    
    // 2. Iterative Resolution (Max 5 passes to resolve chains like A->B->C)
    bool changed = true;
    int pass = 0;
    while (changed && pass < 5)
    {
        changed = false;
        
        // Check all held keys against currently active layers
        for (const auto& key : currentlyHeldKeys)
        {
            // Find the HIGHEST priority mapping for this key
            // (Simulate findMapping logic but just for LayerMomentary commands)
            for (int i = (int)layers.size() - 1; i >= 0; --i)
            {
                if (!layers[i].isActive()) continue; // Check calculated state
                
                auto& cm = layers[i].compiledMap;
                auto it = cm.find(key);
                
                // Also check global fallback logic if needed
                if (it == cm.end() && key.deviceHandle != 0) {
                     it = cm.find({0, key.keyCode});
                }

                if (it != cm.end())
                {
                    // Found the active mapping for this key
                    const auto& action = it->second;
                    if (action.type == ActionType::Command && 
                        action.data1 == (int)MIDIQy::CommandID::LayerMomentary)
                    {
                        int targetLayer = action.data2;
                        if (targetLayer >= 0 && targetLayer < layers.size())
                        {
                            if (!layers[targetLayer].isMomentary) {
                                layers[targetLayer].isMomentary = true;
                                changed = true;
                            }
                        }
                    }
                    break; // Key handled by this layer, stop looking down
                }
            }
        }
        pass++;
    }
}
```

**2. Refactor `processEvent`:**
*   **Top:** Update `currentlyHeldKeys`.
    *   If `isDown`: Insert.
    *   If `!isDown`: Erase.
*   **Recalculate:** Call `updateLayerState()`.
*   **Command Logic Change:**
    *   **LayerMomentary:** DO NOTHING here. (Handled by `updateLayerState`).
    *   **LayerToggle:** Toggle `layers[id].isLatched`. Call `updateLayerState()`.
    *   **LayerSolo:** Update `isLatched` for all. Call `updateLayerState()`.
    *   **Others:** Process normally.

### Step 3: Cleanup
*   Remove `momentaryToggleKey` variables. The new system is stateless regarding *which* key triggered the layer.

---

**Generate code for: Updated `InputProcessor.h` (Struct changes), and Updated `InputProcessor.cpp` (Logic Overhaul).**