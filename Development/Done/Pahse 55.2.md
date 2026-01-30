### 📋 Cursor Prompt: Phase 55.2 - Complete Mapping Schemas

**Target Files:**
1.  `Source/MappingDefinition.h`
2.  `Source/MappingDefinition.cpp`
3.  `Source/Tests/MappingDefinitionTests.cpp`

**Task:**
Expand `MappingDefinition::getSchema` to handle `CC`, `Command`, and `Envelope` types. This ensures the Core can fully describe the UI for any mapping.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.h`:**
    *   Add a helper `static std::map<int, juce::String> getCommandOptions();` to return the map of ID -> Name (e.g., "Sustain", "Layer Momentary").
    *   Add `static std::map<int, juce::String> getLayerOptions();` (0 -> "Base", 1 -> "Layer 1"...).

2.  **Update `Source/MappingDefinition.cpp`:**
    *   Implement the helper methods (populate the maps).
    *   **Expand `getSchema` switch statement:**

    *   **Case: `ActionType::CC`**
        *   **Channel:** Slider (1-16).
        *   **Data1 (Controller):** Slider (0-127), Label "CC Number".
        *   **Data2 (Value):** Slider (0-127), Label "Press Value".
        *   **Separator**
        *   **Release Behavior:** Toggle (`sendReleaseValue`), Label "Send Value on Release".
        *   **Release Value:** Slider (`releaseValue`, 0-127). *Condition:* Only enable/show if `sendReleaseValue` is true? (For now, just add it, UI will handle enable state later).

    *   **Case: `ActionType::Command`**
        *   **Data1 (Command):** ComboBox. Options = `getCommandOptions()`.
        *   **Data2 (Target):**
            *   *Logic:* Read `data1` from the `mapping` ValueTree.
            *   If `data1` is `LayerMomentary`, `LayerToggle`, or `LayerSolo`:
                *   Return a **ComboBox** using `getLayerOptions()`. Label "Target Layer".
            *   Else if `data1` is `GlobalPitch...` or `Panic`:
                *   Return **LabelOnly** (or nothing) for Data2 (not used).
            *   Else:
                *   Return **Slider** (0-127).

    *   **Case: `ActionType::Envelope`**
        *   **Channel:** Slider.
        *   **Target:** ComboBox (`adsrTarget`). Options: "CC", "PitchBend", "SmartScaleBend".
        *   *Logic:* Read `adsrTarget` from ValueTree.
        *   If "CC": **Data1** Slider ("CC Number").
        *   **Data2 (Peak):** Slider. If Target == Pitch/Smart, Max = 16383. Else Max = 127.
        *   **Separator**
        *   **ADSR Sliders:** Attack, Decay, Sustain, Release.

3.  **Update `Source/Tests/MappingDefinitionTests.cpp`:**
    *   **Test 2: Command Context.**
        *   Create Mapping with Type=Command, Data1=LayerMomentary.
        *   Get Schema.
        *   Assert: Control for "data2" is `ComboBox` (Layer Selector).
    *   **Test 3: Envelope Context.**
        *   Create Mapping with Type=Envelope, Target=PitchBend.
        *   Get Schema.
        *   Assert: Control for "data2" has Max=16383.

**Goal:**
Verify that the `MappingDefinition` engine correctly adapts the schema based on the properties of the mapping (e.g. changing Data2 from a Slider to a Dropdown depending on Data1).