# 🤖 Cursor Prompt: Phase 17.3 - Fix Chord Generation (Cache Integration)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** UI has Chord options, but they don't work. Single notes play.
*   **Diagnosis:** The `Zone` calculates the MIDI note but loses Scale/Degree context. `InputProcessor` receives a raw note and cannot generate the diatonic chord.
*   **Fix:** Move Chord Generation into `Zone::rebuildCache()`. The Zone should compile the *Full Chord Stack* into its cache.

**Strict Constraints:**
1.  **Cache:** `Zone` must map `Key -> std::vector<int>`.
2.  **Piano Mode:** If Layout is "Piano", chords should likely be disabled or forced to Chromatic chords (since Piano mode ignores scales). For now, disable Smart Chords in Piano Mode (Force `None`).

---

### Step 1: Update `Zone.h`
1.  Change `keyToNoteCache` to `std::unordered_map<int, std::vector<int>> keyToChordCache`.
2.  Update `processKey` (or add `getNotes`) to return `std::vector<int>`.

### Step 2: Update `Zone.cpp`
**Refactor `rebuildCache`:**
1.  Retrieve `std::vector<int> intervals` from `ScaleUtilities` (via `scaleName` or a helper).
2.  **Inside the Layout Loop (Linear/Grid):**
    *   You have `degree` and `baseNote`.
    *   Call `ChordUtilities::generateChord(baseNote, intervals, degree, chordType, voicing)`.
    *   Store the resulting vector in `keyToChordCache`.
3.  **Inside Piano Loop:**
    *   Piano mode relies on physical geometry, not degrees.
    *   Store `{ baseNote }` (Single note) only. Chords don't apply nicely here.

**Refactor `processKey`:**
*   Look up `keyToChordCache`.
*   Apply Global Transpose to *every note* in the vector.
*   Return the vector.

### Step 3: Update `InputProcessor.cpp`
1.  In `processEvent`:
    *   Get the vector from `zone->processKey`.
    *   **Direct Mode:** `voiceManager.noteOn(input, notesVector, ...)`
    *   **Strum Mode:** Update buffer with `notesVector`.
2.  In `simulateInput`:
    *   Update to handle vector return type for logging (e.g., Log "C Maj Triad" or "60, 64, 67").

---

**Generate code for: `Zone.h`, `Zone.cpp`, and `InputProcessor.cpp`.**