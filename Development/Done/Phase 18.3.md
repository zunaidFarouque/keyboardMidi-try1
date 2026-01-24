# 🤖 Cursor Prompt: Phase 18.3 - Adaptive Guitar Voicing

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 18.2 added basic voicing, but the "Guitar" mode is too rigid (always spreads, often jumping too high).
*   **Phase Goal:** Implement **"Adaptive Guitar Voicing"**.
*   **Logic:**
    1.  **Primary Goal:** Use "Open Shapes" (Root, 5th, 3rd-up-octave) for a rich, strum-friendly sound.
    2.  **Constraint:** Keep the notes close to the `Zone Root`.
    3.  **Adaptation:** If the Open Shape drifts too high (e.g., > 5 semitones above Root), **collapse** to a Closed Inversion centered on the Root.

**Strict Constraints:**
1.  **Compilation Only:** This logic runs inside `ChordUtilities`, called by `Zone::rebuildCache`. Runtime performance remains $O(1)$.
2.  **Sorting:** Final vector MUST be sorted (Low to High) for the Strum Engine.
3.  **Scale Awareness:** Math must use Scale Degrees first, then convert to MIDI.

---

### Step 1: Update `ChordUtilities` (`Source/ChordUtilities.cpp`)

Refactor the `Voicing::GuitarSpread` case inside `generateChord`.

**Algorithm:**

1.  **Calculate Diatonic Candidates:**
    *   Get Scale Degrees: `Root`, `3rd`, `5th`, `7th`, `9th`.
    *   Example (C Maj): C, E, G, B, D.

2.  **Construct "Open Shape" (Ideal):**
    *   **Triad:** `Root`, `5th`, `3rd (+1 Octave)`. *(Spread)*
    *   **7th:** `Root`, `5th`, `7th`, `3rd (+1 Octave)`.
    *   **9th:** `Root`, `5th`, `7th`, `9th`, `3rd (+1 Octave)`. *(Keep 9th high)*.
    *   *Calculate the MIDI pitch for the Bass Note of this shape.*

3.  **Range Check (Gravity):**
    *   Compare `ShapeBassNote` vs `ZoneRootNote` (passed as `rootNote` argument).
    *   **Threshold:** If `(ShapeBassNote - ZoneRootNote) > 5` (approx a 4th):
        *   **TRIGGER ADAPTATION.** The chord is getting too high.

4.  **Adaptation Strategy (Inversion Fallback):**
    *   Abandon the Open Shape.
    *   Use the **Smooth/Inversion Logic** (from Phase 18.2).
    *   Take the standard Closed notes (1-3-5-7).
    *   Shift them octaves to cluster tightly around `ZoneRootNote`.
    *   *Result:* F Major becomes `C4-F4-A4` (2nd Inv) instead of a high `F4-C5-A5`.

5.  **Finalize:**
    *   `std::sort` the resulting vector.

### Step 2: Verification (`Zone.cpp`)
Ensure `rebuildCache` passes the correct `Zone Root` to `generateChord`.
*   *Note:* In `rebuildCache`, the `rootNote` passed to `generateChord` should be the **Zone's Base Root**, not the calculated note for the specific key.
*   *Correction:* Currently `generateChord` takes `int rootNote` which implies the specific note of the key (e.g. F4).
*   **API Update:** Change `generateChord` signature to accept `int zoneAnchorNote` explicitly for the gravity calculations. Update `Zone.cpp` to pass `this->rootNote` as the anchor.

---

**Generate code for: Updated `ChordUtilities.h` and `ChordUtilities.cpp`. Update `Zone.cpp` call site.**