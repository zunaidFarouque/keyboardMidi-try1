# 🤖 Cursor Prompt: Phase 23.3 - Expression Engine Optimization (Delta Check)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 23.2 Complete. The ADSR Envelope works, but it transmits data on every single timer tick (200Hz).
*   **Problem:** During the **Sustain** phase (or slow ramps), the calculated value often stays the same (e.g., 127), but the engine keeps sending `CC 1 = 127` redundantly. This floods the MIDI bus.
*   **Phase Goal:** Implement a **Delta Check** to only transmit when the value changes.

**Strict Constraints:**
1.  **Logic:** Calculate the new `outputVal`. Compare it to `lastSentValue`. If equal, **skip** the `midiEngine` call.
2.  **Initialization:** Initialize `lastSentValue` to `-1` so the first value (0) is always sent.

---

### Step 1: Update `ExpressionEngine.h`
Add state tracking to the envelope struct.

**Update `struct ActiveEnvelope`:**
*   Add member: `int lastSentValue = -1;`

### Step 2: Update `ExpressionEngine.cpp`
Implement the check loop.

**Update `hiResTimerCallback`:**
1.  Inside the loop, after calculating `int outputVal` (based on level * peak):
2.  **Add Check:**
    ```cpp
    if (outputVal != envelope.lastSentValue)
    {
        // Send MIDI
        if (envelope.settings.isPitchBend)
            midiEngine.sendPitchBend(...);
        else
            midiEngine.sendCC(...);

        // Update State
        envelope.lastSentValue = outputVal;
    }
    ```

---

**Generate code for: Updated `ExpressionEngine.h` and `ExpressionEngine.cpp`.**