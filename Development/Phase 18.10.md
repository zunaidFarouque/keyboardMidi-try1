# 🤖 Cursor Prompt: Phase 18.10 - Diatonic Ghost Note Fix

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
*   **Current State:** Phase 18.10 Complete.
*   **The Bug:** Ghost notes in "Loose" mode are hardcoded to +11 semitones. This forces a Major 7th on every chord.
*   **The Result:** The V chord (G in C Major) plays an F# (out of key) instead of F natural.
*   **Phase Goal:** Refactor `addGhostNotes` to calculate candidates based on **Scale Degrees**, ensuring all ghost notes stay perfectly in key.

**Strict Constraints:**
1.  **Diatonic Math:** Do not use `11` or `14`. Use `degree + 6` (7th) and `degree + 8` (9th).
2.  **Calculation:** Use `ScaleUtilities::calculateMidiNote` to convert these candidate degrees into valid MIDI notes that fit the gap.
3.  **Strict Mode:** Must still strictly adhere to Root/5th degrees.

---

### Step 1: Update `ChordUtilities.cpp`

Refactor `addGhostNotes`.

**Requirements:**
1.  **Signature Update:** It needs access to `rootNote` (Zone Root), `scaleIntervals`, and `chordRootDegree` (The degree of the chord being played, e.g., 4 for G).
    *   *Note:* You might need to update `generateChord` to pass these down.

**2. Candidate Logic Update:**
*   `std::vector<int> candidateDegrees;`
*   **Strict:** `{ chordRootDegree, chordRootDegree + 4 }` (Root, 5th).
*   **Loose:** `{ chordRootDegree + 6, chordRootDegree + 8 }` (7th, 9th).

**3. Gap Filling Logic (The Hard Part):**
*   We have a MIDI Gap (e.g., between G3 (55) and B4 (71)).
*   We have a Candidate Degree (e.g., 7th of G = F).
*   **Algorithm:**
    *   Convert Candidate Degree to a generic MIDI note (e.g., F0).
    *   Shift it up by octaves (+12) until it sits **inside the Gap**.
    *   If `GapStart < Note < GapEnd`:
        *   Check for semitone clashes (Minor 2nd).
        *   If safe, Add.

---

**Generate code for: Updated `ChordUtilities.h` and `ChordUtilities.cpp`.**