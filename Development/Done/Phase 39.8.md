# 🤖 Cursor Prompt: Phase 39.8 - Manual vs. Zone Conflict Detection

**Role:** Expert C++ Audio Developer (System Logic).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 39.7 Complete.
*   **The Bug:** When a user creates a Manual Mapping for a Key that is *already* part of a Zone on the **same Device Alias**, the Visualizer shows it as a normal "Override" (Blue).
*   **The Goal:** This specific scenario (Same Key, Same Device, Both Manual and Zone defined) must be flagged as a **Conflict** (Red) to warn the user.

**Strict Constraints:**
1.  **Logic Update:** In `InputProcessor::simulateInput`:
    *   If `Specific Manual` exists AND `Specific Zone` exists: Return `VisualState::Conflict`.
    *   If `Specific Manual` exists AND `Global Zone` exists (but no Specific Zone): Return `VisualState::Override`.
2.  **Color:** `VisualizerComponent` must ensure `VisualState::Conflict` renders with `Colours::red`.

---

### Step 1: Update `Source/InputProcessor.cpp`

**Refactor `simulateInput` (Specific View Block):**

```cpp
    // ... Inside if (viewDevice != 0) block ...

    // A. Specific Manual (Highest Priority)
    if (hasSpecificMap) 
    {
        res.action = specificMap->second;
        res.sourceName = "Mapping";
        
        // CRITICAL CHECK: Manual vs Zone Collision
        
        // 1. Same Layer Collision (Local vs Local) -> Red Conflict
        if (specificZone.has_value())
        {
            res.state = VisualState::Conflict;
            res.sourceName = "Conflict: Mapping + Zone";
        }
        // 2. Hierarchy Override (Local vs Global) -> Orange Override
        else if (hasGlobalMap || globalZone.has_value())
        {
            res.state = VisualState::Override;
        }
        // 3. Clean
        else 
        {
            res.state = VisualState::Active;
        }
        
        return res;
    }
```

### Step 2: Verify `VisualizerComponent.cpp`
Ensure the paint loop handles the Red color for Conflict. (It likely does from 39.7, but verify the precedence: Conflict color overrides Mapping color).

**Check `refreshCache`:**
```cpp
if (result.state == VisualState::Conflict)
{
    underlayColor = juce::Colours::red; // Force Red
    borderColor = juce::Colours::red;
    alpha = 1.0f;
}
// Ensure this block comes AFTER the "Determine Base Color" block so it overwrites the Blue/Orange.
```

---

**Generate code for: Updated `InputProcessor.cpp` and `VisualizerComponent.cpp`.**