Here is the prompt for **Phase 26.2**.

This focuses on fixing the **Legato Glide** logic (ensuring the math and state handoff actually work) and adding the **UI Warning** about channel separation.

***

# 🤖 Cursor Prompt: Phase 26.2 - Fix Legato Glide & UI Warnings

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26.1 Complete. Mono/Legato modes exist.
*   **Issue 1 (Legato):** Gliding doesn't work. The system likely falls back to Retrigger mode because the Pitch Bend Lookup Table (`pbLookup`) is empty or incorrect, or the start-value logic is wrong.
*   **Issue 2 (UX):** Users confusingly put Chords and Mono lines on the same MIDI channel, causing note stealing. We need a UI warning.

**Phase Goal:**
1.  Ensure `VoiceManager` populates the Pitch Bend Lookup Table correctly on startup.
2.  Fix the "Glide Start Value" logic (smooth handoff from previous bends).
3.  Add a warning label to `ZonePropertiesPanel`.

**Strict Constraints:**
1.  **Lookup Table:** The `pbLookup` array MUST be populated using `SettingsManager`'s global range. `VoiceManager` must listen to settings changes.
2.  **Glide Logic:**
    *   If `PortamentoEngine` is active, the **Start Value** for the new glide must be the **Current Value** of the engine.
    *   If idle, **Start Value** is 8192 (Center).
    *   **End Value** comes from the `pbLookup` based on the semitone delta.
3.  **UI:** The warning label should only appear if `PolyphonyMode != Poly`.

---

### Step 1: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Fix the Lookup and Start Logic.

**1. Dependency:**
*   Ensure `VoiceManager` holds a reference to `SettingsManager`.
*   Inherit `juce::ChangeListener`.
*   In `changeListenerCallback`: Call `rebuildPbLookup()`.

**2. Implement `rebuildPbLookup`:**
*   Get `range = settingsManager.getPitchBendRange()`.
*   Loop `i` from -127 to +127.
*   If `abs(i) > range`: `pbLookup[i + 127] = -1;` (Invalid).
*   Else: `pbLookup[i + 127] = 8192 + (int)(i * (8192.0 / range));`
*   **Crucial:** Call this in the Constructor!

**3. Update `noteOn` (Mono Branch):**
*   Calculate `delta`.
*   Check `pbLookup`.
*   **If Valid (Glide):**
    *   `int startVal = portamentoEngine.isActive() ? portamentoEngine.getCurrentValue() : 8192;`
    *   `int targetVal = pbLookup[delta + 127];`
    *   Call `portamentoEngine.startGlide(startVal, targetVal, ...);`
    *   **Do NOT send NoteOn/Off.** Just keep the base note active.

### Step 2: Update `PortamentoEngine` (`Source/PortamentoEngine.h`)
Expose state.

**Update:**
*   Add `int getCurrentValue() const { return (int)currentPbValue; }`
*   Add `bool isActive() const { return active; }`

### Step 3: Update `ZonePropertiesPanel.cpp`
Add the warning.

**UI Update:**
*   Add `juce::Label monoWarningLabel;`
*   Text: *"Note: Use a unique MIDI Channel for Mono zones to avoid conflict."*
*   Color: Orange/Yellow text.
*   **Logic:**
    *   In `setZone` and `resized`: Check `currentZone->polyphonyMode`.
    *   If `Poly`: `monoWarningLabel.setVisible(false);`
    *   Else: `monoWarningLabel.setVisible(true);`

---

**Generate code for: Updated `VoiceManager.h/cpp` (Logic Fixes), `PortamentoEngine.h`, and `ZonePropertiesPanel.h/cpp`.**