# 🤖 Cursor Prompt: Phase 18.2 - Advanced Voicing Logic

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.1 Complete. `ChordUtilities` currently generates basic root-position stacks.
*   **Problem:** Basic chords sound robotic. 9th/11th chords sound muddy because notes are crammed into one octave.
*   **Phase Goal:** Implement "Smooth Voicing" (Inversions to minimize movement) and "Guitar Voicing" (Spread voicings to reduce mud).

**Strict Constraints:**
1.  **Sorting:** The final vector returned by `generateChord` MUST be sorted (`std::sort`) by pitch (Low to High). This ensures strums always sweep cleanly.
2.  **Performance:** This logic runs inside `Zone::rebuildCache` (Configuration time). It does not impact audio latency.
3.  **Center Point:** For "Smooth" voicing, use the `rootNote` (Zone Root) passed to the function as the "Gravity Center".

---

### Step 1: Update `ChordUtilities` (`Source/ChordUtilities.h`)
Update the Enums to support the new strategies.

**Updates:**
*   **Enum `Voicing`:** Update items to: `{ RootPosition, Smooth, GuitarSpread }`.
*   **Method Signature:** Ensure `generateChord` accepts the necessary arguments (Root, Scale, Degree, Type, Voicing).

### Step 2: Implement Logic in `ChordUtilities.cpp`
Refactor `generateChord`.

**Algorithm Flow:**
1.  **Step A: Generate Raw Stack.**
    *   Calculate the diatonic notes (1-3-5-7...) based on `ChordType` and `Scale`.
    *   Store in `std::vector<int> notes`.

2.  **Step B: Apply Voicing Strategy.**
    *   **Case `RootPosition`:** Do nothing. (Keep as 1-3-5).
    *   **Case `Smooth` (Inversions):**
        *   Target Center = The `rootNote` argument (Zone Root).
        *   For each note in `notes`:
            *   While `note > Center + 6`: `note -= 12`.
            *   While `note < Center - 6`: `note += 12`.
        *   *Result:* Notes cluster tightly around the Zone Root.
    *   **Case `GuitarSpread` (Open/Drop):**
        *   *Logic:* Move the 2nd note (the 3rd) up one octave.
        *   If `notes.size() >= 4` (7ths/9ths): Move the 4th note (the 7th) up one octave.
        *   *Result:* Creates a wide, open sound ideal for Strumming.

3.  **Step C: Final Polish.**
    *   `std::sort(notes.begin(), notes.end());`
    *   Return `notes`.

### Step 3: Update `ZonePropertiesPanel.cpp`
Update the UI to reflect the new options.

**Constructor:**
*   Update the `voicingSelector` item list:
    *   ID 1: "Root Position (Bass)"
    *   ID 2: "Smooth / Inversions (Pads)"
    *   ID 3: "Guitar / Spread (Strum)"

---

**Generate code for: Updated `ChordUtilities.h`, Updated `ChordUtilities.cpp`, and Updated `ZonePropertiesPanel.cpp`.**