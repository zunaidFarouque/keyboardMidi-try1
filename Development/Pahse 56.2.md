### 📋 Cursor Prompt: Phase 56.2 - Expression UI Schema

**Target Files:**
1.  `Source/MappingDefinition.cpp`
2.  `Source/MappingInspector.cpp`

**Task:**
Implement the `ActionType::Expression` schema in `MappingDefinition`. Use the `ValueTree` state to conditionally show/hide controls (Progressive Disclosure).

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.cpp` (`getSchema`):**

    *   **Update Type Selector:** Ensure "Expression" is in the options (ID 5, or whatever you mapped it to in Phase 56.1). Remove "CC" and "Envelope".

    *   **Implement `ActionType::Expression` Case:**
        *   **1. Setup:**
            *   Read `adsrTarget` (string, default "CC").
            *   Read `useCustomEnvelope` (bool, default false).
        
        *   **2. Basic Controls:**
            *   **Channel:** Slider (1-16).
            *   **Target:** ComboBox ("adsrTarget"). Options: "CC", "PitchBend", "SmartScaleBend".
            *   **If Target == "CC":**
                *   **CC Number:** Slider (`data1`, 0-127). Label "Controller".
            *   **If Target == "SmartScaleBend":**
                *   **Steps:** Slider (`smartStepShift`, -12 to 12). Label "Scale Steps".
            *   **Peak Value:** Slider (`data2`). Label "Peak Value".
                *   If Target == "CC", Max = 127.
                *   If Target == "PitchBend" or "SmartScaleBend", Max = 16383.

        *   **3. Mode Selection:**
            *   **Separator:** Label "Dynamics".
            *   **Mode Toggle:** Toggle (`useCustomEnvelope`). Label "Use Custom ADSR Curve".

        *   **4. Conditional Branch (The "Magic"):**
            *   **IF `useCustomEnvelope` IS TRUE:**
                *   **Attack:** Slider (`adsrAttack`, 0-5000ms).
                *   **Decay:** Slider (`adsrDecay`, 0-5000ms).
                *   **Sustain:** Slider (`adsrSustain`, 0.0-1.0).
                *   **Release:** Slider (`adsrRelease`, 0-5000ms).
                *   *(Note: Hide "Release Value" controls here, as ADSR handles release)*
            *   **ELSE (`useCustomEnvelope` IS FALSE - "Simple Mode"):**
                *   **Release Toggle:** Toggle (`sendReleaseValue`). Label "Send Value on Release". (`widthWeight = 0.45`).
                *   **Release Val:** Slider (`releaseValue`). Label "". (`widthWeight = 0.55`, `sameLine = true`, `dependent = sendReleaseValue`).

2.  **Update `Source/MappingInspector.cpp`:**
    *   **Triggers:** Ensure `useCustomEnvelope` is added to the list of properties that trigger a **Rebuild** in `valueTreePropertyChanged`.
        ```cpp
        if (property == juce::Identifier("useCustomEnvelope")) {
             juce::MessageManager::callAsync([this]() { rebuildUI(); });
        }
        ```

**Outcome:**
*   When you select "Expression", you initially see a simple interface (Channel, CC Num, Peak).
*   If you click "Use Custom ADSR Curve", the UI instantly expands to show the envelopes.
*   If you uncheck it, it shrinks back to the simple view (Legacy CC behavior).