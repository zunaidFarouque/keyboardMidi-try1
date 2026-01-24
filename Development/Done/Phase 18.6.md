# 🤖 Cursor Prompt: Phase 18.6 - Fix Guitar Voicing (Octave Folding)

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
*   **Current State:** Phase 18.5 Debug Report shows that "Adaptive Guitar" voicing is behaving aggressively.
*   **Problem:** When a chord exceeds the range threshold (e.g., G Major), the engine switches to "Smooth/Closed" voicing. This causes abrupt texture changes and drops chords too low (e.g., B2).
*   **Goal:** Refactor the `GuitarSpread` logic to maintain the **Open Shape** (1-5-10) but simply **Shift Octaves** to keep it in range.

**Strict Constraints:**
1.  **Preserve Texture:** Do NOT fall back to "Smooth" or "Closed" voicings. Maintain the "Spread" intervals.
2.  **Logic:**
    *   Calculate the Open Shape at the natural scale pitch.
    *   Check the distance between the **Chord's Bass Note** and the **Zone Root**.
    *   **High Threshold:** If `Bass - Root > 5` (Higher than F relative to C): **Subtract 12** from ALL notes.
    *   **Low Threshold:** If `Bass - Root < -7` (Lower than F below): **Add 12** to ALL notes.
3.  **Sorting:** Ensure the final vector is sorted `std::sort`.

---

### Step 1: Update `ChordUtilities.cpp`

Refactor the `Voicing::GuitarSpread` case in `generateChord`.

**New Algorithm:**

1.  **Base Generation:**
    *   Calculate the standard diatonic stack (1, 3, 5, 7...) based on the `degreeIndex` and `intervals`. (e.g., if Root is C4, Degree 4 is G4).

2.  **Apply Spread (The "Guitar Shape"):**
    *   Move the **3rd** (Index 1) up +12 semitones.
    *   If 7th/9th/11th exist (Index 3+): Move them up +12 semitones to keep them above the 3rd (preventing mud).
    *   *Current State:* You now have a High Open Chord (e.g., G4, D5, B5).

3.  **Apply Gravity (Octave Folding):**
    *   `int currentBass = notes[0];` (Assuming the vector is roughly ordered, or find `min`).
    *   `int diff = currentBass - zoneAnchorNote;`
    *   **If (`diff > 5`):**
        *   Shift **entire vector** down -12.
        *   *Result for G:* G3, D4, B4. (Perfect open chord).
    *   **Else If (`diff < -7`):**
        *   Shift **entire vector** up +12.

4.  **Finalize:**
    *   `std::sort` the vector.

---

**Generate code for: Updated `Source/ChordUtilities.cpp`.**