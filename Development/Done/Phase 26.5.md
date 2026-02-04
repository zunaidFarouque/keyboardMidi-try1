# 🤖 Cursor Prompt: Phase 26.5 - Mono Engine Hardening (State Integrity)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26.4 Complete.
*   **Problem:** Rapid input ("spamming") causes race conditions in the Mono/Legato engine.
    1.  **Stuck Notes:** The stack empties, but the audio engine thinks a note is still playing.
    2.  **Unresponsive:** The engine gets into a state where it ignores new notes because it thinks a previous glide is still active on a dead voice.
*   **Phase Goal:** Implement **Strict State Enforcement** using Critical Sections and "Zombie Voice" cleanup.

**Strict Constraints:**
1.  **Thread Safety:** You must add a `juce::CriticalSection monoCriticalSection` to `VoiceManager`. **ALL** access to `monoStacks`, `activeVoices`, and `portamentoEngine` during NoteOn/NoteOff events must be scoped by this lock.
2.  **Zombie Check:** In `noteOn` (Mono branch), if the stack is currently empty but a voice is still active on that channel, immediately kill the old voice before starting the new one.
3.  **Hard Stop:** In `handleKeyUp`, if the stack becomes empty, force-kill **ANY** voice on that channel (don't trust the ID) and force-stop the `portamentoEngine`.

---

### Step 1: Update `VoiceManager.h`
Add the lock.

**Member:**
*   `juce::CriticalSection monoCriticalSection;`

### Step 2: Update `VoiceManager.cpp`
Implement the "Garbage Collection" logic.

**1. Update `noteOn`:**
*   `const juce::ScopedLock sl(monoCriticalSection);` (Start of function).
*   **Zombie Check (Mono Mode):**
    *   Before pushing to stack:
    *   `if (monoStack.empty())`
        *   Check `voices` for any entry with `midiChannel == channel`.
        *   If found: `midiEngine.sendNoteOff(...)` and remove it. (Self-healing).

**2. Update `handleKeyUp`:**
*   `const juce::ScopedLock sl(monoCriticalSection);`
*   **Hard Stop Logic (Stack Empty):**
    *   If `stack.empty()`:
        *   Iterate `voices`. If `v.midiChannel == channel`:
            *   `midiEngine.sendNoteOff(...)`
            *   Remove from `voices`.
        *   **Reset Engine:**
            *   `midiEngine.sendPitchBend(channel, 8192);`
            *   `portamentoEngine.stop();` (Ensure timer stops).

**3. Update `panic`:**
*   `const juce::ScopedLock sl(monoCriticalSection);`
*   Ensure `portamentoEngine.stop()` is called here too.

---

**Generate code for: Updated `VoiceManager.h` and `VoiceManager.cpp`.**