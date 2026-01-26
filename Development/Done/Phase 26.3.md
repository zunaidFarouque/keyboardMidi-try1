# 🤖 Cursor Prompt: Phase 26.3 - Fix Legato Anchor Release

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 26.2.1 Complete.
*   **The Bug:** In Legato Mode, holding A, pressing B (Glide), then releasing A causes the sound to stop or retrigger.
*   **The Cause:** Releasing A triggers `NoteOff A`. But in our "Fake Legato" implementation, Note A is the "Anchor" (Carrier) for the sound of B (via Pitch Bend). Killing A kills B.
*   **Phase Goal:** Update `handleKeyUp` to respect the Legato Stack. If keys are still in the stack, the Anchor Voice must remain alive.

**Strict Constraints:**
1.  **Legato Mode Only:** This logic applies only if the channel is in `PolyphonyMode::Legato`.
2.  **Voice Cleanup:** When the stack finally empties, we must find and kill the active voice on that channel, regardless of which `InputID` originally triggered it.

---

### Step 1: Update `VoiceManager.cpp`

Refactor `handleKeyUp`.

**New Logic Flow:**

1.  **Iterate Mono Stacks:**
    *   Loop through `monoStacks` map.
    *   Check if `source` exists in the stack vector.
    *   **If found:**
        *   Get `channel` and `polyphonyMode` (you might need to store mode in the map or look it up via the active voice on that channel). *Actually, since `VoiceManager` doesn't store Zone/Mode directly per channel, we assume we find the mode via the Active Voice on that channel.*
        *   **Refined Lookup:** Iterate `voices` first to find which `midiChannel` this `source` *might* belong to, OR search `monoStacks` directly. *Search `monoStacks` is safer for "Ghost Anchors".*

2.  **Implement the Legato Branch:**
    *   Remove `source` from the stack.
    *   **If Stack is Empty:**
        *   **Kill:** Find the active voice on this `channel` (Note: In Mono/Legato, there is only one).
        *   Send `NoteOff`.
        *   Remove from `voices`.
        *   Reset Pitch Bend to 8192.
    *   **If Stack is NOT Empty:**
        *   **Sustain:** Do NOT send NoteOff.
        *   **Glide:** Calculate Pitch Bend for the new Stack Top (relative to the Anchor Voice's note number).
        *   Call `portamentoEngine.startGlide(...)`.

3.  **Implement Poly/Mono Branch (Fallback):**
    *   If not found in a Legato stack, proceed with existing logic (Standard NoteOff).

**Helper Implementation Detail:**
Since `monoStacks` is `std::map<int, std::vector<std::pair<int, InputID>>>` (Channel -> Stack), you can iterate it.
*   If you find the `source`, you know the `channel`.
*   You need to know if this channel is **Legato**.
    *   *Solution:* Check `voices` for this channel. `voices[i].zonePtr->polyphonyMode`?
    *   *Alternative:* Just assume if it's in `monoStacks` it needs Mono/Legato handling.
    *   *Critical Check:* We need to distinguish **Mono (Retrigger)** from **Legato (Glide)**.
    *   *Update:* Use `activeVoice.allowSustain` or similar? No.
    *   *Best way:* Look up the active voice for this channel. It stores the `InputID`. If that voice exists, we can access the Zone/Mode via the `InputProcessor`... wait, `VoiceManager` doesn't know about Zones.
    *   **Fix:** We need to store `PolyphonyMode` in `ActiveVoice` struct.

### Step 2: Update `VoiceManager.h`
*   Update `struct ActiveVoice`: Add `PolyphonyMode polyphonyMode;`.
*   Update `noteOn`: Accept `PolyphonyMode` argument.

### Step 3: Update `InputProcessor.cpp`
*   Pass `zone->polyphonyMode` to `voiceManager.noteOn`.

### Step 4: Apply Logic in `VoiceManager.cpp`
*   In `handleKeyUp`:
    *   Search `monoStacks`.
    *   If found:
        *   Find the `ActiveVoice` for this channel to get the `PolyphonyMode`.
        *   If `Legato`: Apply the "Ghost Anchor" logic (Only kill if stack empty).
        *   If `Mono`: Apply standard Retrigger logic (If stack not empty, Retrigger. If empty, Kill).

---

**Generate code for: Updated `VoiceManager.h`, Updated `VoiceManager.cpp`, and Updated `InputProcessor.cpp`.**