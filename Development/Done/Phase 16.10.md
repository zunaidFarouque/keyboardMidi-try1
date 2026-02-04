# 🤖 Cursor Prompt: Phase 16.10 - Keyboard-Based Key Removal

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 16.9 Complete. Zone Editor supports "Assign Keys" (Add) via keyboard press, and removing keys via mouse click on chips.
*   **Phase Goal:** Implement "Remove Keys" mode. The user clicks a toggle, presses keys on their physical keyboard, and those keys are *removed* from the Zone.

**Strict Constraints:**
1.  **Mutual Exclusion:** "Assign Keys" and "Remove Keys" buttons MUST behave like a Radio Group (only one active at a time).
2.  **Engine Sync:** Removing a key must trigger `zoneManager->rebuildLookupTable()` immediately so the engine stops playing that note.
3.  **Thread Safety:** UI updates (ChipList refresh) must happen on the Message Thread.

---

### Step 1: Update `ZonePropertiesPanel.h`
Add the new control.

**Requirements:**
1.  **Member:** `juce::ToggleButton removeKeysButton;`.

### Step 2: Update `ZonePropertiesPanel.cpp`
Implement the logic.

**1. Constructor:**
*   Configure `removeKeysButton` text: "Remove Keys".
*   **Toggle Logic:**
    *   `assignKeysButton.onClick`: If checked, set `removeKeysButton` to false.
    *   `removeKeysButton.onClick`: If checked, set `assignKeysButton` to false.

**2. `handleRawKeyEvent` (The Core Logic):**
*   **Existing Logic:** Keep the "Assign Keys" block.
*   **New Logic:**
    *   `if (removeKeysButton.getToggleState() && isDown)`:
    *   Call `currentZone->removeKey(keyCode)`.
    *   **Refresh:** `chipList.setKeys(currentZone->inputKeyCodes);`
    *   **Sync:** Call `zoneManager->rebuildLookupTable();`
    *   **Feedback:** Update the "X Keys Assigned" label.

**3. `resized`:**
*   Place the new button next to "Assign Keys".

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**