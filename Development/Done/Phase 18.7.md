# 🤖 Cursor Prompt: Phase 18.7 - Ghost Notes & Per-Note Velocity

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.6 Complete. We have adaptive voicings, but the data pipeline assumes every note in a chord has the same velocity.
*   **Phase Goal:** Implement "Filled" voicings (Ghost Notes).
    *   **Strict Mode:** Fill gaps using existing chord tones (Root/5th).
    *   **Loose Mode:** Fill gaps using the next harmonic extension (7th for Triads, 9th for 7ths).
    *   **Architecture:** Upgrade the pipeline to support per-note velocities so ghost notes can be quieter.

**Strict Constraints:**
1.  **Compile-Time Math:** All ghost note calculation happens in `Zone::rebuildCache`.
2.  **Runtime Efficiency:** `InputProcessor` must not do searching/sorting. It simply applies velocity math to the cached list.
3.  **Strumming:** `StrumEngine` must now accept a vector of velocities, not a single int.

---

### Step 1: Update Data Structures

**1. `Source/ChordUtilities.h`:**
*   New Struct: `struct ChordNote { int pitch; bool isGhost; };`
*   Update `generateChord` to return `std::vector<ChordNote>`.
*   Update Enums: Add `Voicing::SmoothFilled` and `Voicing::GuitarFilled`.

**2. `Source/Zone.h`:**
*   Change Cache: `std::unordered_map<int, std::vector<ChordNote>> keyToChordCache;`
*   New Members: `bool strictGhostHarmony = true;`, `float ghostVelocityScale = 0.6f;`.
*   Serialization: Save/Load these new members.

### Step 2: Implement Logic (`Source/ChordUtilities.cpp`)

**New Method:** `addGhostNotes(...)` (Helper called by `generateChord` if "Filled" voicing is selected).

**Logic:**
1.  **Identify Strategy:**
    *   If `Strict`: Candidates are `1, 5` (and `3` if needed).
    *   If `Loose`: Candidates are `7` (if Triad), `9` (if 7th).
2.  **Analyze Gaps:**
    *   Sort current notes.
    *   Find the largest interval between adjacent notes (e.g., Gap > 7 semitones).
    *   Also check "Headroom" (gap between highest note and Zone Root + Range) or "Floor" (gap below lowest note).
3.  **Placement:**
    *   Try to place a Candidate Note into the largest gap.
    *   *Constraint:* Must not create a minor 2nd (1 semitone) clash with existing notes (unless intended for tension, but usually avoid for clarity).
    *   If valid, add as `isGhost = true`.
4.  **Sorting:** Re-sort the final vector by pitch.

### Step 3: Update `InputProcessor` (`Source/InputProcessor.cpp`)
Calculate velocities dynamically.

**Refactor `processEvent`:**
1.  Get `std::vector<ChordNote>` from Zone.
2.  Calculate `mainVelocity` (Base + Random).
3.  Create two vectors: `finalNotes` (int) and `finalVelocities` (int).
4.  **Loop:**
    *   `finalNotes.push_back(cn.pitch);`
    *   If `!cn.isGhost`: `finalVelocities.push_back(mainVelocity);`
    *   If `cn.isGhost`: `finalVelocities.push_back(mainVelocity * zone->ghostVelocityScale);`
5.  Pass these vectors to `VoiceManager`.

### Step 4: Update `VoiceManager` & `StrumEngine`
1.  **`StrumEngine`:** Update `triggerStrum` to accept `std::vector<int> velocities`. Store velocity per note in `PendingNote`.
2.  **`VoiceManager`:** Update `noteOn` signature. Pass the vectors through.

### Step 5: Update `ZonePropertiesPanel`
Add controls for the new features.
1.  **Voicing Combo:** Add "Smooth (Filled)" and "Guitar (Filled)".
2.  **Ghost Settings:**
    *   `juce::ToggleButton strictGhostToggle` ("Strict Harmony").
    *   `juce::Slider ghostVelSlider` ("Ghost Vol %").

---

**Generate code for: `ChordUtilities` (Header/Cpp), `Zone` (Header/Cpp), `InputProcessor`, `StrumEngine`, `VoiceManager`, and `ZonePropertiesPanel`.**