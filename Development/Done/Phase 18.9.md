# 🤖 Cursor Prompt: Phase 18.9 - Fix Strict Harmony Logic

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.8 Debug Report reveals a logic bug in `ChordUtilities`.
*   **The Bug:** When `strictGhostHarmony` is **ON**, the engine is still inserting **7ths** (Interval 11) into Triads.
    *   *Example Output:* C Major Triad + Strict Mode -> Adds 'B' (7th) as a ghost note.
*   **Goal:** Fix the candidate selection logic so Strict Mode only ever uses chord tones (Root, 3rd, 5th).

**Strict Constraints:**
1.  **Strict Mode Definition:** Candidates must ONLY be `{0, 4, 7}` (Root, 3rd, 5th) relative to the chord root.
2.  **Loose Mode Definition:** Candidates should prioritize `{11, 14}` (7th, 9th) to add color.
3.  **Fallback:** In Strict Mode, if no Root/3rd/5th fits in the gap without clashing (semitone rub), **add nothing**. Do not fall back to 7ths.

---

### Step 1: Update `ChordUtilities.cpp`

Refactor the `addGhostNotes` helper function (or the logic block inside `generateChord`).

**Revised Logic:**

1.  **Define Candidates:**
    *   `std::vector<int> candidates;`
    *   **If Strict:** `candidates = { 0, 7, 4 };` (Prioritize Root/5th, then 3rd).
    *   **If Loose:** `candidates = { 11, 14 };` (Prioritize 7th, 9th).

2.  **Gap Analysis:**
    *   Identify the largest gap between existing notes.
    *   *Calculation:* `GapStart` and `GapEnd`.

3.  **Candidate Selection Loop:**
    *   Iterate through `candidates`.
    *   Calculate the specific pitch of that candidate that would fit into the `Gap`.
        *   (e.g., if Candidate is Root (0) and Gap is between G3 and E4, try C4).
    *   **Check Validity:**
        *   Is it inside the gap? (`GapStart < Note < GapEnd`)
        *   **Clash Check:** Is `abs(Note - GapStart) > 1` AND `abs(GapEnd - Note) > 1`? (Prevent minor 2nds).
    *   **Success:** If valid, add as Ghost and **BREAK**. (Only add one ghost per chord).

4.  **Strict Fallback (Crucial):**
    *   If Strict Mode was active and no candidate fit, **do NOT** try the Loose candidates. Just return the original chord.

---

**Generate code for: Updated `Source/ChordUtilities.cpp`.**