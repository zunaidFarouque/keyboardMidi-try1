# 🤖 Cursor Prompt: Phase 17.1 - Strum Buffer & Triggers

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 17 Complete. We can generate chords and strum them, but they play immediately upon key press (`Direct` mode).
*   **Phase Goal:** Implement `PlayMode::Strum`.
    *   **Behavior:** Pressing Zone Keys updates a "Buffer" (Silent). Pressing a "Trigger Key" (Space/Shift) plays the Buffer.

**Strict Constraints:**
1.  **Visual Feedback:** Buffered keys must light up **Light Blue** on the Visualizer to show they are "Ready". Playing keys remain Yellow.
2.  **Thread Safety:** The Buffer is written by the Input Thread and read by the Visualizer (Message Thread). Use `juce::ReadWriteLock`.
3.  **Triggers:** For this phase, hardcode **Spacebar** (`0x20`) as "Strum Down" and **Shift** (`0x10`) as "Strum Up" within `InputProcessor` to test the logic.

---

### Step 1: Update `Zone` (`Source/Zone.h`)
Add the mode switch.

**Requirements:**
1.  **Enum:** `enum class PlayMode { Direct, Strum };`
2.  **Member:** `PlayMode playMode = PlayMode::Direct;`
3.  **Serialization:** Save/Load this property in `toValueTree`/`fromValueTree`.

### Step 2: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Implement the Buffer Logic.

**Requirements:**
1.  **Members:**
    *   `std::vector<int> noteBuffer;` (Stores currently held MIDI note numbers).
    *   `juce::ReadWriteLock bufferLock;`
2.  **Logic Update (`processEvent`):**
    *   **Check Triggers First:**
        *   If Key == Spacebar (`0x20`) && isDown: Call `voiceManager.strumNotes(noteBuffer, speed, downstroke)`.
        *   If Key == Shift (`0x10`) && isDown: Call `voiceManager.strumNotes(noteBuffer, speed, upstroke)`.
    *   **Zone Logic:**
        *   Get Zone. Calculate Chord (via `ChordUtilities`).
        *   **If `PlayMode::Direct`:** Call `voiceManager.noteOn` (Existing logic).
        *   **If `PlayMode::Strum`:**
            *   On Key Down: **Replace** `noteBuffer` with the new chord notes.
            *   On Key Up: Clear `noteBuffer` (or keep it latching? For now, clear it to require holding keys).
            *   **Crucial:** Do NOT call `voiceManager.noteOn`.

3.  **Getter:** `std::vector<int> getBufferedNotes();` (For Visualizer).

### Step 3: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Add a specific method for Strumming from a list.

**Requirements:**
1.  **Method:** `void strumNotes(const std::vector<int>& notes, int speedMs, bool downstroke);`
    *   **Logic:**
        *   Send `NoteOff` for any currently playing notes (Auto-choke previous strum).
        *   Call `strumEngine.triggerStrum(notes, velocity, channel, speedMs);` (Reverse vector if `!downstroke`).
        *   Track these notes in `activeNoteNumbers` so `panic()` still works.

### Step 4: Update `VisualizerComponent` (`Source/VisualizerComponent.cpp`)
Visualize the waiting state.

**Refactor `paint()`:**
1.  Get buffered notes: `auto buffered = inputProcessor.getBufferedNotes();` (Thread safe).
2.  **Layer Logic:**
    *   Inside the loop, check: `bool isBuffered = std::find(buffered.begin(), buffered.end(), noteNumber) != buffered.end();`
    *   **Layer 2 (Body):**
        *   If `active` (Playing): Yellow.
        *   If `isBuffered` (Waiting): **Colours::lightblue**.
        *   Else: Dark Grey.

---

**Generate code for: Updated `Zone.h`, Updated `InputProcessor.h/cpp`, Updated `VoiceManager.h/cpp`, and Updated `VisualizerComponent.cpp`.**