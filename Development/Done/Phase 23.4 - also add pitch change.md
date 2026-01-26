# 🤖 Cursor Prompt: Phase 23.4 - Extended Envelope Targets (PB & Aftertouch)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 23.3 Complete. The ADSR engine works for CCs, but Pitch Bend support is partial/boolean, and Aftertouch is missing.
*   **Phase Goal:** Expand the ADSR capability to support **Pitch Bend** and **Channel Pressure (Aftertouch)** as selectable targets.

**Strict Constraints:**
1.  **Architecture:** Use `enum class AdsrTarget` instead of booleans to allow future expansion.
2.  **Ranges:**
    *   **CC:** 0 - 127.
    *   **Aftertouch:** 0 - 127.
    *   **Pitch Bend:** 0 - 16383. (Ensure the "Peak Value" slider in Inspector adjusts its range or we interpret 0-127 as 0-16383 scaling internally. *Decision: Let's scale 0-127 input to 0-16383 for Pitch Bend to keep the UI consistent, or just allow the full range. Let's scale it: Output = NormalizedLevel * Peak * (Target == PB ? 128 : 1).*
3.  **UI:** Hide the "CC Number" slider when it's not needed.

---

### Step 1: Update `MappingTypes.h`
Refactor the settings struct.

**Updates:**
1.  Define `enum class AdsrTarget { CC, PitchBend, Aftertouch };`.
2.  Update `struct AdsrSettings`:
    *   Remove `bool isPitchBend`.
    *   Add `AdsrTarget target = AdsrTarget::CC;`.

### Step 2: Update `InputProcessor.cpp`
Handle Persistence.

**Updates in `addMappingFromTree` / `processEvent`:**
1.  **Load:** Read string property `"adsrTarget"`.
    *   "CC" -> `AdsrTarget::CC`
    *   "PitchBend" -> `AdsrTarget::PitchBend`
    *   "Aftertouch" -> `AdsrTarget::Aftertouch`
2.  **Save:** Ensure `MappingInspector` writes these strings correctly (handled in Step 4).

### Step 3: Update `ExpressionEngine.cpp`
Handle the Output.

**Refactor `hiResTimerCallback`:**
Inside the sending block:
*   Calculate `scaledPeak`.
    *   If `target == PitchBend`: `scaledPeak = 16383`. (Assume full range for now, or read `data2` scaled up). *Let's stick to using `data2` (Peak) from the mapping.*
    *   **Logic:**
        *   `int max = (envelope.settings.target == AdsrTarget::PitchBend) ? 16383 : 127;`
        *   `int peak = envelope.peakValue;` (If user set 127 in UI, we might need to multiply by 128 for PB, or just trust the user set 16383. **Better UX:** Scale 0-127 UI value to 0-16383 for PB automatically).
        *   `int output = level * peak * (envelope.settings.target == AdsrTarget::PitchBend ? 128 : 1);`
*   **Switch `envelope.settings.target`:**
    *   **Case CC:** `midiEngine.sendCC(ch, num, output);`
    *   **Case PitchBend:** `midiEngine.sendPitchBend(ch, output);`
    *   **Case Aftertouch:** `midiEngine.sendChannelPressure(ch, output);`

### Step 4: Update `MappingInspector.cpp`
Update the UI Logic.

**Updates:**
1.  **Constructor:** Update `envTargetSelector` items: "CC" (1), "Pitch Bend" (2), "Aftertouch" (3).
2.  **`setSelection`:**
    *   Read `adsrTarget`. Set Selector ID.
    *   **Visibility Logic:**
        *   If `CC`: Show `data1Slider` (Label "CC Num").
        *   If `PitchBend` or `Aftertouch`: Hide `data1Slider` (Label "-").
3.  **Callbacks:** Ensure changing the selector writes the correct String ("CC", "PitchBend", "Aftertouch") to the ValueTree.

---

**Generate code for: `MappingTypes.h`, `InputProcessor.cpp`, `ExpressionEngine.cpp`, and `MappingInspector.cpp`.**