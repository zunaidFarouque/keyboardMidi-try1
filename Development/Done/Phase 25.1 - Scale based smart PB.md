# 🤖 Cursor Prompt: Phase 25.1 - Smart Scale Pitch Bend (Pre-Compiled)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 25 Complete. We have `SettingsManager` for Global Pitch Bend Range.
*   **Phase Goal:** Implement **"Smart Scale Bend"**.
    *   User maps a key to "Bend +1 Scale Step".
    *   If last note was E4 (in C Major), +1 Step = F4. Delta = 1 semitone.
    *   If last note was B4 (in C Major), +1 Step = C5. Delta = 1 semitone.
    *   If last note was D4 (in C Major), +1 Step = E4. Delta = 2 semitones.
    *   **Optimization:** This specific Pitch Bend value for every note must be **Pre-Compiled** into a lookup table attached to the Mapping.

**Strict Constraints:**
1.  **Architecture:** Add `AdsrTarget::SmartScaleBend`.
2.  **The Compiler:** In `InputProcessor::addMappingFromTree`, if the target is SmartBend, calculate an array `std::vector<int> lookuptable(128)` where index `i` is the Pitch Bend value (0-16383) required if the last note played was `i`.
3.  **Dependencies:** Use `SettingsManager` (Global PB Range) and `ZoneManager` (Global Scale Intervals) for the math.
4.  **Rebuilds:** Ensure `InputProcessor` listens to `ZoneManager` and `SettingsManager`. If Scale or Range changes, trigger `rebuildMapFromTree`.

---

### Step 1: Update `MappingTypes.h`
1.  Add `AdsrTarget::SmartScaleBend`.
2.  Update `MidiAction`:
    *   Add `std::vector<int> smartBendLookup;` (Empty by default).

### Step 2: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Implement the Compiler logic.

**1. Track Last Note:**
*   Add member `int lastTriggeredNote = 60;`.
*   Update `processEvent`: When a Note On is sent (Direct or Strum), update `lastTriggeredNote`.

**2. Update `addMappingFromTree` (The Compiler):**
*   Check if `adsrTarget == SmartScaleBend`.
*   Read `smartStepShift` (int) from XML (e.g., +1, -1).
*   Get `globalRange` (Settings) and `globalIntervals` (ZoneManager).
*   **Loop `note = 0` to `127`**:
    *   Calculate `currentDegree` of `note` in `globalIntervals`.
    *   Target Degree = `currentDegree + smartStepShift`.
    *   `targetNote` = `ScaleUtilities::calculateMidiNote(..., targetDegree)`.
    *   `semitoneDelta` = `targetNote - note`.
    *   `pbValue` = `8192 + (semitoneDelta * (8192.0 / globalRange))`.
    *   Clamp to 0-16383.
    *   Store in `action.smartBendLookup[note]`.

**3. Update `processEvent` (Runtime):**
*   If `target == SmartScaleBend`:
    *   `int peak = action.smartBendLookup[lastTriggeredNote];`
    *   Pass `peak` to `expressionEngine.triggerEnvelope(...)` instead of `action.data2`.

### Step 3: Update `MappingInspector`
UI Controls for the new mode.

**Update `setSelection`:**
*   If `SmartScaleBend`:
    *   Hide CC/PB-Raw sliders.
    *   Show `smartStepSlider` (Range -7 to +7 steps).
    *   Show Label: "Scale Steps".

---

**Generate code for: Updated `MappingTypes.h`, Updated `InputProcessor.h/cpp` (Compiler & State Tracking), and Updated `MappingInspector.cpp`.**