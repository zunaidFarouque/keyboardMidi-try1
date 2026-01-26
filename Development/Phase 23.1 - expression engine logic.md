# 🤖 Cursor Prompt: Phase 23.1 - Expression Engine Logic

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 23 Complete. `AdsrSettings` struct exists, and `ExpressionEngine` is linked but empty.
*   **Phase Goal:** Implement the ADSR State Machine inside `ExpressionEngine`.

**Strict Constraints:**
1.  **Timing:** Use `juce::HighResolutionTimer` running at **5ms intervals** (200Hz). This is sufficient for control signals (MIDI is serial and slow) without choking the CPU.
2.  **Logic:**
    *   **Attack:** Linear interpolation from 0.0 to 1.0 (Peak).
    *   **Decay:** Linear interpolation from 1.0 to `sustainLevel`.
    *   **Sustain:** Hold `sustainLevel`.
    *   **Release:** Linear interpolation from Current Level to 0.0.
3.  **Targets:**
    *   If `isPitchBend`: Output range is **0 to 16383** (mapped to MIDI Pitch Bend message). *Note: Center is 8192. For an envelope, usually we modulate 0->Max or Center->Max. Let's assume Unipolar modulation 0->Max (0->127 for CC, 0->16383 for PB) for now.*
    *   If `CC`: Output range is **0 to 127**.

---

### Step 1: Update `ExpressionEngine.h`
Define the state machine structures.

**Requirements:**
1.  **Enum:** `enum class Stage { Attack, Decay, Sustain, Release, Finished };`
2.  **Inner Struct `ActiveEnvelope`:**
    *   `InputID source;`
    *   `int channel;`
    *   `AdsrSettings settings;`
    *   `int peakValue;` (The target value defined in the mapping, e.g. 127).
    *   **State Tracking:** `Stage stage`, `double currentLevel` (0.0-1.0), `double stageProgress` (0.0-1.0), `double stepSize`.
3.  **Members:**
    *   `MidiEngine& midiEngine;` (Reference).
    *   `std::vector<ActiveEnvelope> activeEnvelopes;`
    *   `juce::CriticalSection lock;`

### Step 2: Implement `ExpressionEngine.cpp`
The math core.

**1. `triggerEnvelope`:**
*   Create new `ActiveEnvelope`.
*   Set Stage to `Attack`.
*   Calculate `stepSize = 1.0 / (attackMs / timerInterval)`.
*   Add to vector.
*   Start Timer if needed.

**2. `releaseEnvelope`:**
*   Find envelope by `InputID`.
*   Set Stage to `Release`.
*   Calculate `stepSize = currentLevel / (releaseMs / timerInterval)`.

**3. `hiResTimerCallback`:**
*   Iterate all active envelopes.
*   **Update Level:**
    *   **Attack:** `level += stepSize`. If `level >= 1.0`, switch to Decay.
    *   **Decay:** `level -= stepSize`. If `level <= sustain`, clamp to sustain, switch to Sustain.
    *   **Sustain:** Keep level.
    *   **Release:** `level -= stepSize`. If `level <= 0`, mark `Finished`.
*   **Send MIDI:**
    *   `int outputVal = level * peakValue;`
    *   If `settings.isPitchBend`: `midiEngine.sendPitchBend(channel, outputVal)`.
    *   Else: `midiEngine.sendCC(channel, settings.ccNumber, outputVal)`.
*   **Cleanup:** Remove `Finished` envelopes. Stop timer if empty.

### Step 3: Integration (`MainComponent` & `InputProcessor`)
1.  **MainComponent:** Pass `midiEngine` to `inputProcessor` constructor (so it can pass it to `expressionEngine`).
2.  **InputProcessor:**
    *   Initialize `ExpressionEngine` (passing midiEngine).
    *   In `processEvent`:
        *   If `ActionType::Envelope`:
        *   If `isDown`: `expressionEngine.triggerEnvelope(...)`.
        *   If `!isDown`: `expressionEngine.releaseEnvelope(...)`.

---

**Generate code for: `ExpressionEngine.h`, `ExpressionEngine.cpp`, `InputProcessor.h`, `InputProcessor.cpp`, and updated `MainComponent.cpp`.**