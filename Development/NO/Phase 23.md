# 🤖 Cursor Prompt: Phase 23 - Note Release & Delayed Offs

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** The Zone Editor has a "Release Duration" slider, but the audio engine doesn't use it. Notes stop instantly.
*   **Phase Goal:** Implement the backend logic to support delayed Note Off events (Timed Release).

**Strict Constraints:**
1.  **Timing:** Use the existing `StrumEngine` (HighResTimer) to handle the timing. Do not create a second timer.
2.  **State Safety:** If a note is waiting to be released, and the user presses the key again *before* the release happens, the pending release MUST be cancelled to avoid cutting off the new note.

---

### Step 1: Update `StrumEngine` (`Source/StrumEngine.h/cpp`)
It needs to handle stopping notes, not just starting them.

**1. Update `PendingNote` Struct:**
*   Add `bool isNoteOff;` (Discriminator).

**2. Add Method:**
*   `void scheduleNoteOff(int note, int channel, double delayMs);`
    *   Adds a `PendingNote` with `isNoteOff = true` and `targetTime = now + delay`.

**3. Update `hiResTimerCallback`:**
*   Check queue.
*   If `targetTime` passed:
    *   If `!isNoteOff`: Send Note On (Existing).
    *   If `isNoteOff`: Send Note Off (New).

**4. Add Method:** `cancelPendingNoteOff(int note, int channel);`
*   Search queue. Remove any `PendingNote` where `isNoteOff == true` AND note/channel match.
*   *Usage:* Called when a new note starts to prevent the old "tail" from killing the new attack.

### Step 2: Update `VoiceManager` (`Source/VoiceManager.cpp`)
Intercept the Key Up event.

**Refactor `handleKeyUp`:**
1.  Identify the notes associated with the Input.
2.  **Get Zone Settings:** Access `zone->releaseMode` and `zone->releaseDurationMs`.
3.  **Logic:**
    *   If `Sustain` or `Latch` is active: (Keep existing logic).
    *   Else If `ReleaseMode == Time` AND `duration > 0`:
        *   **Do NOT send NoteOff immediately.**
        *   Call `strumEngine.scheduleNoteOff(note, channel, duration)`.
        *   Remove from `activeVoices` (logically it's released, even if sound is tailing).
    *   Else (`Instant`):
        *   Send NoteOff immediately.

**Refactor `noteOn`:**
*   Before starting a new note, call `strumEngine.cancelPendingNoteOff(note, channel)`. This handles the re-trigger case.

---

**Generate code for: Updated `StrumEngine.h/cpp` and Updated `VoiceManager.cpp`.**