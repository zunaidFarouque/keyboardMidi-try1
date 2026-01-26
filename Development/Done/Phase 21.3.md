# 🤖 Cursor Prompt: Phase 21.3 - Note Release Envelopes

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 21.2 Complete.
*   **Problem:** When a key is released, the `NoteOff` event is sent instantly. This sounds synthetic and abrupt. Real instruments have a decay tail.
*   **Phase Goal:** Implement a "Release Duration" parameter per Zone. When a key is lifted, the system should wait $N$ milliseconds before sending the MIDI NoteOff.

**Strict Constraints:**
1.  **Timing:** Use `juce::HighResolutionTimer` in `VoiceManager` to handle the delayed events accurately.
2.  **Voice Stealing:** If a key is pressed *while* it is currently fading out (in the release queue), the pending NoteOff must be cancelled/flushed immediately to prevent stuck notes or logic errors.
3.  **State Tracking:** The `ActiveVoice` struct must store the `releaseDuration` when the note starts, because the Zone settings might change while the note is playing.

---

### Step 1: Update `Zone` (`Source/Zone.h/cpp`)
Add the property.

**Requirements:**
1.  **Member:** `int releaseDurationMs = 0;` (Default 0 = Instant).
2.  **Serialization:** Save/Load this property.

### Step 2: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Implement the Scheduler.

**1. Architecture Changes:**
*   Inherit `juce::HighResolutionTimer`.
*   **Update `ActiveVoice` struct:** Add `int releaseMs`.
*   **New Struct:** `struct PendingNoteOff { int note; int channel; double targetTimeMs; };`
*   **New Queue:** `std::vector<PendingNoteOff> releaseQueue;`
*   **Members:** `double currentTimeMs = 0;` (Incremented in timer or queried from `Time::getMillisecondCounterHiRes()`).

**2. Update `noteOn`:**
*   Accept `int releaseMs` as an argument.
*   **Safety Check:** Check `releaseQueue`. If this Note/Channel is waiting to die, remove it from the queue (effectively stealing the voice back).
*   Store `releaseMs` in the `ActiveVoice` entry.

**3. Update `handleKeyUp`:**
*   Find the `ActiveVoice`.
*   If `voice.releaseMs > 0`:
    *   **Don't** send MIDI NoteOff yet.
    *   Calculate `targetTime = Time::getMillisecondCounterHiRes() + voice.releaseMs`.
    *   Move data to `releaseQueue`.
    *   Start Timer if not running.
*   Else (Instant):
    *   Send MIDI NoteOff immediately.

**4. Implement `hiResTimerCallback`:**
*   Iterate `releaseQueue`.
*   If `targetTime <= Time::getMillisecondCounterHiRes()`:
    *   Send MIDI NoteOff.
    *   Remove from queue.
*   If queue empty, `stopTimer()`.

### Step 3: Update `InputProcessor.cpp`
Pass the data.

**Update `processEvent`:**
*   Read `zone->releaseDurationMs`.
*   Pass it to `voiceManager.noteOn(...)`.
*   *Note:* Update the `voiceManager.noteOn` signature in the Header to accept this new int.

### Step 4: Update `ZonePropertiesPanel`
UI Control.

**Requirements:**
1.  **UI:** Add `juce::Slider releaseSlider` ("Release").
2.  **Range:** 0ms to 5000ms (5 seconds). Suffix " ms".
3.  **Callback:** Update `currentZone->releaseDurationMs`.

---

**Generate code for: Updated `Zone.h/cpp`, Updated `VoiceManager.h/cpp`, Updated `InputProcessor.cpp`, and Updated `ZonePropertiesPanel.cpp`.**