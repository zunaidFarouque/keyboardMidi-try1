# 🤖 Cursor Prompt: Phase 23.5 - Musical Pitch Bend Math

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
*   **Current State:** Phase 23.4 Complete. Envelope can target Pitch Bend, but requires manual raw value entry (0-16383), which is non-musical.
*   **Phase Goal:** Allow users to define Pitch Bend in **Semitones**.
    *   User sets "Synth Range" (what the synth is configured to) and "Target Shift".
    *   System calculates the MIDI value relative to center (8192).
    *   Envelope modulates from Center -> Target -> Center.

**Strict Constraints:**
1.  **Zero Point:** Pitch Bend envelopes must start and end at **8192**, not 0.
2.  **Compilation:** The complex math (Semitones -> MIDI 0-16383) happens in the **Inspector (UI)** or **InputProcessor (Load)**. The `ExpressionEngine` simply receives a `peakValue` integer.
3.  **Math:**
    *   `StepsPerSemitone = 8192.0 / SynthRange`.
    *   `TargetMIDI = 8192 + (Shift * StepsPerSemitone)`.

---

### Step 1: Update `ExpressionEngine.cpp`
Implement the Offset Logic.

**Refactor `hiResTimerCallback`:**
1.  Determine `zeroPoint`:
    *   If `settings.target == PitchBend`: `zeroPoint = 8192`.
    *   Else: `zeroPoint = 0`.
2.  **Calculate Output:**
    *   `int peak = envelope.peakValue;`
    *   `double range = peak - zeroPoint;` (This can be negative for downward bends).
    *   `int output = zeroPoint + (envelope.currentLevel * range);`
3.  **Clamp:** Ensure `output` stays within 0-16383 (PB) or 0-127 (CC).

### Step 2: Update `InputProcessor.cpp`
Handle the persistence of the "Musical" settings.

**1. `addMappingFromTree` (XML Load):**
*   Check if `adsrTarget == PitchBend`.
*   Read optional properties: `pbRange` (Default 12), `pbShift` (Default 0).
*   **Compiler Logic:**
    *   If `pbRange` and `pbShift` exist, calculate the target MIDI value immediately.
    *   `steps = 8192.0 / pbRange`.
    *   `peak = 8192 + (pbShift * steps)`.
    *   Store `peak` in `action.data2`. (This overwrites any raw value to ensure consistency).

### Step 3: Update `MappingInspector` (`Source/MappingInspector.h/cpp`)
Create the Musical UI.

**1. Members:**
*   `juce::Slider pbRangeSlider;` (1-48).
*   `juce::Slider pbShiftSlider;` (-24 to +24).

**2. Constructor:**
*   Setup sliders.
*   **Callbacks:**
    *   Save values to `pbRange` / `pbShift` properties in ValueTree.
    *   **Crucial:** Also calculate the resulting `data2` (Peak Value) and save that too, so the InputProcessor doesn't have to guess.

**3. `setSelection`:**
*   **If Target == PitchBend:**
    *   Hide `data1Slider` (CC Num) and `data2Slider` (Raw Peak).
    *   Show `pbRangeSlider` and `pbShiftSlider`.
    *   Read values from Tree (default to 12 and 0).
*   **Else:**
    *   Hide PB sliders.
    *   Show CC/Peak sliders.

**4. `resized`:**
*   Place the new sliders in the layout dynamically.

---

**Generate code for: Updated `ExpressionEngine.cpp`, `InputProcessor.cpp`, `MappingInspector.h`, and `MappingInspector.cpp`.**