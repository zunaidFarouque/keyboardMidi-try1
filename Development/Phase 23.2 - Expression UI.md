# 🤖 Cursor Prompt: Phase 23.2 - Expression UI & Serialization

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 23.1 Complete. `ExpressionEngine` logic exists, but there is no way to configure an Envelope in the UI, and the settings are not saved to disk.
*   **Phase Goal:** Update `MappingInspector` to expose ADSR controls and `InputProcessor` to serialize them.

**Strict Constraints:**
1.  **Dynamic UI:** The ADSR sliders must **only** appear when the Action Type is set to "Envelope".
2.  **Data Model:** We need to extend `MidiAction` to store the envelope parameters.
3.  **Serialization:** Ensure these new fields are read from XML in `InputProcessor`.

---

### Step 1: Update `MappingTypes.h`
Expand the data structure.

**Updates:**
1.  **New Struct:** `struct AdsrSettings { int attack = 50; int decay = 0; int sustain = 127; int release = 50; bool isPitchBend = false; int targetCC = 1; };`
2.  **Update `MidiAction`:** Add a member `AdsrSettings adsr;`.

### Step 2: Update `InputProcessor.cpp`
Handle the data persistence.

**1. `addMappingFromTree` (XML Load):**
*   Read properties: `adsrAttack`, `adsrDecay`, `adsrSustain`, `adsrRelease`.
*   Read `adsrTarget` (String "CC" or "PitchBend").
*   Read `peakValue` (Store in `data2` or a new field, let's use `data2` for the Peak Level).
*   Populate `action.adsr` struct.

**2. `processEvent`:**
*   Ensure the `expressionEngine.triggerEnvelope` call passes the correctly populated `action.adsr` data.

### Step 3: Update `MappingInspector` (`Source/MappingInspector.h/cpp`)
Build the control panel.

**1. Header:**
*   Add 4 Sliders: `attackSlider`, `decaySlider`, `sustainSlider`, `releaseSlider`.
*   Add 4 Labels: "A", "D", "S", "R".
*   Add `juce::ComboBox envTargetSelector` ("CC", "Pitch Bend").
*   *Note:* Reuse existing `data1Slider` for CC Number and `data2Slider` for Peak Value.

**2. Constructor:**
*   Configure ranges: A/D/R (0-5000ms), S (0-127).
*   Add listener callbacks to write to `ValueTree` properties (e.g., `setProperty("adsrAttack", val, ...)`).

**3. `setSelection` (Visibility Logic):**
*   **If Type == Envelope:**
    *   Show ADSR sliders.
    *   Show `envTargetSelector`.
    *   If Target == CC: Show `data1Slider` (Label "CC Num").
    *   If Target == PitchBend: Hide `data1Slider`.
    *   Show `data2Slider` (Label "Peak Val").
*   **Else:** Hide ADSR sliders and Target selector.

**4. `resized`:**
*   Layout the ADSR sliders in a row (or 2x2 grid) below the main controls.

---

**Generate code for: Updated `MappingTypes.h`, Updated `InputProcessor.cpp`, Updated `MappingInspector.h`, and Updated `MappingInspector.cpp`.**