# 🤖 Cursor Prompt: Phase 17 - Smart Chords & Strumming Core

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 16.10 Complete. We play single notes.
*   **Phase Goal:** Implement `ChordUtilities` (Math) and `StrumEngine` (Timing). Upgrade `VoiceManager` to track multiple notes per input key.

**Strict Constraints:**
1.  **Timing:** Use `juce::HighResolutionTimer` for the Strum Engine to ensure sub-millisecond precision.
2.  **Voice Tracking:** `VoiceManager` currently maps `InputID -> Single Message`. It MUST be updated to map `InputID -> Vector of Messages` to handle chords properly (so releasing the key stops the whole chord).
3.  **CMake:** Provide the snippet to add `ChordUtilities.cpp` and `StrumEngine.cpp`.

---

### Step 1: `ChordUtilities` (`Source/ChordUtilities.h/cpp`)
Static helper for musical math.

**Requirements:**
1.  **Enums:**
    *   `ChordType { None, Triad, Seventh, Ninth, Power5 }`.
    *   `Voicing { Close, Open, Guitar }`.
2.  **Method:**
    `static std::vector<int> generateChord(int rootNote, const std::vector<int>& scaleIntervals, int degreeIndex, ChordType type, Voicing voicing);`
    *   **Logic:**
        *   Start with `rootNote`.
        *   Get the scale indices for the 3rd, 5th, 7th (using `ScaleUtilities` logic: `index + 2`, `index + 4`, etc.).
        *   Calculate their MIDI notes using `ScaleUtilities::calculateMidiNote`.
    *   **Voicing Logic:**
        *   *Close:* Keep calculated notes as is.
        *   *Open:* Take the middle note (3rd) and move it up +1 Octave (+12).
        *   *Guitar:* Root, 5th, Octave, 10th (3rd + 12).

### Step 2: `StrumEngine` (`Source/StrumEngine.h/cpp`)
Manages the playback queue.

**Requirements:**
1.  **Inheritance:** `juce::HighResolutionTimer`.
2.  **Dependencies:** Reference to `MidiEngine`.
3.  **Struct:** `struct PendingNote { int note; int velocity; int channel; double targetTimeMs; };`
4.  **Members:**
    *   `std::vector<PendingNote> noteQueue;`
    *   `juce::CriticalSection queueLock;`
    *   `double currentTime = 0;`
5.  **Methods:**
    *   `void triggerStrum(const std::vector<int>& notes, int velocity, int channel, int speedMs);`
        *   **Logic:** Loop through notes. `targetTime = currentTime + (index * speedMs)`. Add to queue.
    *   `hiResTimerCallback()`:
        *   Increment `currentTime` (or use system time).
        *   Check queue. If `targetTime <= now`, send to `MidiEngine`, remove from queue.

### Step 3: Update `VoiceManager`
Handle Polyphony (One Input -> Many Output Notes).

**Refactor:**
*   Change `activeNotes` container to: `std::unordered_multimap<InputID, int> activeNoteNumbers;` (Stores the MIDI note numbers triggered by an input).
*   **Update `noteOn`:**
    *   Accepts `std::vector<int> notes`.
    *   For each note: Store in map, send to `StrumEngine`.
*   **Update `handleKeyUp`:**
    *   Find range in map for this `InputID`.
    *   For each entry: Send `NoteOff` via `MidiEngine` immediately.
    *   **Critical:** Tell `StrumEngine` to cancel any *pending* notes for this chord (prevents "Ghost Strums" after quick release).

### Step 4: Update `Zone` & `InputProcessor`
1.  **`Zone`:** Add `ChordType chordType` and `int strumSpeedMs` members (Save/Load).
2.  **`InputProcessor`:**
    *   In `processEvent`:
        *   If `zone` is found:
        *   Calculate Base Note (existing).
        *   Call `ChordUtilities::generateChord(...)`.
        *   Call `voiceManager.noteOn(input, chordVector, ...)` instead of single note.

---

**Generate code for: `ChordUtilities`, `StrumEngine`, updated `VoiceManager`, updated `Zone`, and `CMakeLists.txt`.**