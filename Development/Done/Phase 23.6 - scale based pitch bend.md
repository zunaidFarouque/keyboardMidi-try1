# 🤖 Cursor Prompt: Phase 23.6 - Scale-Aware Smart Pitch Bends

**Role:** Expert C++ Audio Developer (Music Theory Specialist).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 23.5 Complete. Pitch Bend works but uses raw semitones.
*   **Phase Goal:** Implement **Smart Pitch Bend**.
    *   User selects "Shift Scale Steps" (e.g., +1).
    *   If playing E (in C Major), +1 Step = F (+1 semitone).
    *   If playing F (in C Major), +1 Step = G (+2 semitones).
    *   **Optimization:** Use pre-compiled lookup tables in `Zone` to avoid scale math during playback.

**Strict Constraints:**
1.  **Compiler:** `Zone::rebuildCache` must populate `std::vector<int> noteToDegree` (size 128) and `std::vector<int> degreeToNote` (sufficient size).
2.  **Runtime:** `ExpressionEngine` must calculate the target Pitch Bend value at the moment of trigger by looking up the `lastPlayedNote` and using the Zone's lookup tables.
3.  **UI:** `MappingInspector` shows a "Scale Steps" slider when `SmartPitch` is selected.

---

### Step 1: Update `MappingTypes.h`
Add the new target.
*   Update `enum class AdsrTarget`: Add `SmartPitch`.

### Step 2: Update `Zone` (`Source/Zone.h/cpp`)
Implement the Lookup Tables.

**1. Header:**
*   `std::vector<int> noteToDegree;` (Index 0-127 -> Scale Degree).
*   `std::vector<int> degreeToNote;` (Scale Degree -> MIDI Note).
*   **Method:** `int getSmartSeimtoneDelta(int startNote, int stepShift);`

**2. `rebuildCache`:**
*   Clear/Resize vectors.
*   Iterate octaves (-2 to 8) and Scale Intervals.
*   Populate `degreeToNote` sequentially.
*   Populate `noteToDegree`: For each valid note in the scale, map MIDI->Degree. For notes *outside* the scale, map to the nearest lower degree (or -1).

**3. `getSmartSeimtoneDelta` (O(1) Lookup):**
*   `startDegree = noteToDegree[startNote];`
*   `targetDegree = startDegree + stepShift;`
*   `targetNote = degreeToNote[targetDegree];`
*   Return `targetNote - startNote`.

### Step 3: Update `VoiceManager`
We need context.
*   Add method: `int getLastPlayedNote(int channel) const;`
*   Logic: Track the most recent `NoteOn` per channel in a map `lastNotes`.

### Step 4: Update `ExpressionEngine.cpp`
Calculate the dynamic target.

**Refactor `triggerEnvelope`:**
*   Check if `settings.target == AdsrTarget::SmartPitch`.
*   **Get Context:** `int rootNote = voiceManager.getLastPlayedNote(channel);`
*   **Get Zone:** Need access to the Zone to call `getSmartSeimtoneDelta`.
    *   *Note:* `ExpressionEngine` might need to look up the Zone via `InputProcessor` or `ZoneManager`. If simpler, pass the `Zone*` or the calculated `semitoneDelta` into `triggerEnvelope` from `InputProcessor`.
    *   *Decision:* Let's pass the calculated `peakValue` (Raw MIDI PB) into `triggerEnvelope` from `InputProcessor`, moving the lookup logic there.

### Step 5: Update `InputProcessor.cpp` (`processEvent`)
*   If `Action == SmartPitch`:
    *   `int startNote = voiceManager.getLastPlayedNote(channel);`
    *   `int steps = action.adsr.smartSteps;` (We need to add this field to `AdsrSettings` or reuse `data1`).
    *   `int semitones = zone->getSmartSeimtoneDelta(startNote, steps);`
    *   **Math:** `targetPB = 8192 + (semitones * (8192.0 / pbRange))`.
    *   Call `expressionEngine.triggerEnvelope(..., targetPB)`.

### Step 6: Update `MappingInspector`
*   If `SmartPitch`: Show "Scale Steps" slider (-12 to +12). Reuse `data1` or `pbShift`.

---

**Generate code for: `Zone.h/cpp` (Compiler), `VoiceManager.h/cpp` (Context), `InputProcessor.cpp` (Logic), and `MappingInspector.cpp`.**