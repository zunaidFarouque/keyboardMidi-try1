### 📋 Cursor Prompt: Phase 55.5 - Dynamic UI Refinement (Command & Envelope)

**Target Files:**
1.  `Source/MappingDefinition.cpp`
2.  `Source/MappingInspector.cpp`

**Task:**
Refine the Schema definitions to handle context-sensitivity perfectly, remove the "Macro" type, and ensure the UI rebuilds immediately when context changes.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.cpp` (`getSchema`):**

    *   **Type Selector:** Remove "Macro" from the options map.

    *   **Command Logic (Data1 Dependency):**
        *   Retrieve `data1` (Command ID).
        *   **If Layer Command** (`LayerMomentary`, `LayerToggle`, `LayerSolo`):
            *   `Data2`: Type `ComboBox`, Options `getLayerOptions()`, Label "Target Layer".
        *   **If Global Pitch/Mode:**
            *   `Data2`: Type `LabelOnly` (Value "N/A"), or simply **omit** the control from the vector.
        *   **If Panic:**
            *   `Data2`: **Omit**.
        *   **Default:**
            *   `Data2`: Type `Slider` (0-127).

    *   **Envelope Logic (Target Dependency):**
        *   Retrieve `adsrTarget`.
        *   **If PitchBend / SmartScaleBend:**
            *   **Omit** Data1 (Note/CC Number) - Pitch Bend doesn't need a number.
            *   `Data2`: Label "Peak Value", Max 16383.
        *   **If CC:**
            *   Include Data1 (Slider, Label "CC Number").
            *   `Data2`: Label "Peak Value", Max 127.

2.  **Update `Source/MappingInspector.cpp`:**

    *   **`valueTreePropertyChanged`:**
        *   We must detect changes that require a schema rebuild.
        *   Current check: `type`, `data1`.
        *   **Add:** `adsrTarget`. (Changing Envelope Target changes the UI).
        *   **Code:**
            ```cpp
            if (property == juce::Identifier("type") || 
                property == juce::Identifier("data1") || 
                property == juce::Identifier("adsrTarget")) 
            {
                // Must use callAsync to avoid deleting components while they are calling this callback
                juce::MessageManager::callAsync([this]() { rebuildUI(); });
            }
            else {
                // Just repaint or update values for simple sliders
                // (Optimization for later, for now repaint is fine)
                repaint(); 
            }
            ```

**Verification:**
*   Select "Command". Change Dropdown from "Sustain" to "Layer Momentary". The "Data2" slider should instantly vanish and become a "Target Layer" dropdown.
*   Select "Envelope". Change Target from "CC" to "Pitch Bend". The "CC Number" slider should vanish.