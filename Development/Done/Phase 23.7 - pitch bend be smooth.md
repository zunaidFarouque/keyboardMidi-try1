# 🤖 Cursor Prompt: Phase 23.7 - Pitch Bend Priority Stack (LIFO)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 23.6 Complete. Pitch Bend envelopes calculate values, but overlapping presses fight each other or reset incorrectly.
*   **Phase Goal:** Implement a **Priority Stack** for Pitch Bend.
    *   **Behavior:** The most recently pressed key controls the pitch.
    *   **Handoff:** When switching control (A->B or B->A), the pitch glides smoothly from the *Current Value* to the *New Target*, using the *New Owner's* Attack time.
    *   **Background Release:** Releasing a key that is *not* currently controlling the pitch (i.e., held in the background) should remove it from the stack silently without affecting the output.

**Strict Constraints:**
1.  **Per-Channel Stack:** Pitch Bend is per-channel. Maintain a `std::map<int, std::vector<InputID>>` (Channel -> Stack).
2.  **Dynamic Start:** Update the envelope math. Instead of assuming a start of 8192, every envelope needs a `startValue` member that is set at the moment of triggering.
3.  **Re-Triggering:** When falling back (B released, A takes over), A must be "Re-Attacked" (Stage set to Attack, Level 0.0, StartValue = Current physical pitch).

---

### Step 1: Update `ExpressionEngine.h`
Add stack management and dynamic start capability.

**Updates:**
1.  **Struct `ActiveEnvelope`:**
    *   Add `int dynamicStartValue = -1;` (The value at `Level 0.0`).
    *   Add `bool isDormant = false;` (If true, this envelope exists but does not send MIDI).
2.  **Class Members:**
    *   `std::map<int, std::vector<InputID>> pitchBendStacks;`
    *   `int currentPitchBendValues[17];` (Cache current output per channel 1-16, default 8192).

### Step 2: Update `ExpressionEngine.cpp`
Implement the Priority Logic.

**1. Update `hiResTimerCallback` (The Calculator):**
*   **Logic:** `output = dynamicStart + (level * (peak - dynamicStart))`.
*   **Cache:** Before calling `midiEngine.sendPitchBend`, store the value in `currentPitchBendValues[channel]`.
*   **Dormancy:** If `envelope.isDormant`, **skip** calculation and sending. Only update state logic if needed, or just pause it.

**2. Update `triggerEnvelope` (Press):**
*   If Target is PitchBend:
    *   Get the Stack for this channel.
    *   If Stack not empty, mark the current Top (Back) envelope as `isDormant = true`.
    *   Push new `InputID` to Stack.
    *   **Handoff:** Set new envelope's `dynamicStartValue` = `currentPitchBendValues[channel]`.
*   If Target is CC:
    *   `dynamicStartValue` = 0. (Standard behavior).

**3. Update `releaseEnvelope` (Release):**
*   If Target is PitchBend:
    *   Get Stack.
    *   **Case 1: Releasing the Active Driver (Top of Stack)**
        *   Remove from Stack.
        *   If Stack is NOT empty:
            *   Get new Top (previous key).
            *   **Re-Activate:** Find that envelope in `activeEnvelopes`.
            *   Set `isDormant = false`.
            *   **Smooth Transition:**
                *   Set `dynamicStartValue` = `currentPitchBendValues[channel]`.
                *   Reset `currentLevel = 0.0` (Start Attack phase).
                *   Recalculate step size based on *that* envelope's Attack settings.
        *   If Stack IS empty:
            *   Let the current envelope finish its Release phase (Standard ADSR release to 8192).
    *   **Case 2: Releasing a Background Key (Middle of Stack)**
        *   Remove from Stack.
        *   Find the envelope and mark it `Finished` (kill it).
        *   **Do NOT** output MIDI. (The Top envelope is still driving).

---

**Generate code for: Updated `ExpressionEngine.h` and `ExpressionEngine.cpp`.**