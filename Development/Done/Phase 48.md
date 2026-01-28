The problem is that the **Visualizer is not being notified** when the Layer state changes internally. The logic works (the layer opens), but the screen doesn't update to show "ACTIVE: Base | Shift" until you trigger a repaint some other way (e.g., mouse over).

Also, we need to address the **"Momentary Lock-out"** bug I discovered during analysis: if you map a key on Layer 0 to "Momentary Layer 1", and then map *that same key* on Layer 1 to a Note, you will get stuck in Layer 1 forever because the Key Up event will be consumed by Layer 1 (Note Off) instead of falling through to Layer 0 (Layer Off).

Here is the prompt for **Phase 48** to fix both issues.

***

# 🤖 Cursor Prompt: Phase 48 - Fix Layer Visualization & Momentary Logic

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Layers work in the backend, but the Visualizer HUD doesn't update to show active layers.
*   **Bug 1 (UI):** `InputProcessor` changes layer states (`isActive`) silently. Visualizer doesn't know it needs to repaint the "Active Layers" text.
*   **Bug 2 (Logic):** "Momentary Layer" functionality breaks if the Trigger Key is *also* mapped on the Target Layer. The Target Layer consumes the "Key Up" event, preventing the "Turn Off Layer" logic from running.

**Phase Goal:**
1.  Make `InputProcessor` broadcast changes when layers toggle.
2.  Fix the Momentary Layer logic to ensure Key Up always reaches the switcher.

**Strict Constraints:**
1.  **Broadcasting:** `InputProcessor` must inherit `juce::ChangeBroadcaster`.
2.  **Visualizer:** Must listen to `InputProcessor` and trigger `needsRepaint = true`.
3.  **Momentary Logic:** In `processEvent` (Key Up), if we are in a Momentary Layer state, we must check if the released key matches the "Layer Trigger" before checking normal mappings.
    *   *Implementation:* Store `int momentaryToggleKey = -1;` in `InputProcessor`.
    *   On Down: If command is `LayerMomentary`, set `momentaryToggleKey = input.keyCode`.
    *   On Up: If `input.keyCode == momentaryToggleKey`, Force Layer Off and Return.

---

### Step 1: Update `InputProcessor.h`
**1. Inheritance:** Add `public juce::ChangeBroadcaster`.
**2. Member:** Add `int momentaryToggleKey = -1;` (Track which key is holding the layer open).

### Step 2: Update `InputProcessor.cpp`
**1. Refactor `processEvent`:**

*   **Key Down Block:**
    *   If `LayerMomentary`:
        *   `layers[layerId].isActive = true;`
        *   `momentaryToggleKey = input.keyCode;` // Remember this key
        *   `sendChangeMessage();` // Update UI

*   **Key Up Block (Top Priority Check):**
    *   **Before** calling `lookupAction`:
    *   Check: `if (momentaryToggleKey != -1 && input.keyCode == momentaryToggleKey)`
    *   **Action:**
        *   Turn off *all* temporary layers? Or just the one targeted?
        *   *Simpler:* Iterate layers. If `isActive` && `isMomentary` (add `isMomentary` to Layer struct or just reset all non-0 layers? Better: The Command `data2` tells us which layer. But we don't have the command yet.
        *   *Actually:* Just let the logic fall through, BUT `lookupAction` will find the *Layer 1* mapping (Note) instead of *Layer 0* mapping (Command).
        *   **Fix:** `lookupAction` needs to know we are releasing the toggle.
        *   **New Logic:**
            ```cpp
            if (momentaryToggleKey != -1 && input.keyCode == momentaryToggleKey)
            {
                // This is the layer switch key being released.
                // Force disable the target layer (we need to know which one... store it too?)
                // Better: Just execute the "Turn Off" logic directly here.
                
                // We need to know WHICH layer was momentary.
                // Store `int activeMomentaryLayerId = -1;` instead of just key.
            }
            ```

**Refined Plan for Step 2:**
*   Add `int activeMomentaryLayerId = -1;` to Header.
*   **Key Down:**
    *   If `LayerMomentary`: `activeMomentaryLayerId = layerId;`
*   **Key Up:**
    *   `if (activeMomentaryLayerId != -1 && input.keyCode == (key that triggered it? We need to track that too? No, just check if mapping exists on Base layer? Too slow.)`
    *   **Best:** Just store the key code. `int momentaryTriggerKey = -1;`
    *   **On Up:**
        ```cpp
        if (momentaryTriggerKey != -1 && input.keyCode == momentaryTriggerKey)
        {
             if (activeMomentaryLayerId != -1) {
                 juce::ScopedWriteLock wl(mapLock);
                 layers[activeMomentaryLayerId].isActive = false;
                 sendChangeMessage();
             }
             momentaryTriggerKey = -1;
             activeMomentaryLayerId = -1;
             return; // Swallow event, don't trigger NoteOff for the key itself (it's a modifier)
        }
        ```

**3. Other Commands:**
*   If `LayerToggle` or `LayerSolo` changes state: Call `sendChangeMessage()`.

### Step 3: Update `VisualizerComponent.cpp`
**1. Constructor:** `inputProcessor->addChangeListener(this);`
**2. Destructor:** `inputProcessor->removeChangeListener(this);`
**3. Callback:**
    ```cpp
    if (source == inputProcessor) {
        needsRepaint = true;
    }
    ```

---

**Generate code for: Updated `InputProcessor.h`, Updated `InputProcessor.cpp`, and Updated `VisualizerComponent.cpp`.**