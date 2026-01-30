Here is the Cursor prompt for **Phase 55.3**. This is the **GUI Switchover**, replacing the hardcoded Inspector with the dynamic Schema system.

### 📋 Cursor Prompt: Phase 55.3 - Dynamic Inspector GUI

**Target Files:**
1.  `Source/MappingInspector.h`
2.  `Source/MappingInspector.cpp`

**Task:**
Refactor `MappingInspector` to dynamically generate UI controls based on the `InspectorSchema` provided by `MappingDefinition`.

**Specific Instructions:**

1.  **Update `Source/MappingInspector.h`:**
    *   **Remove:** All hardcoded `juce::Slider`, `juce::ComboBox`, `juce::Label`, `juce::TextButton` members (channelSlider, data1Slider, etc.).
    *   **Add:** Storage for dynamic widgets.
        ```cpp
        // Container for the generated UI rows
        struct InspectorRow {
            std::unique_ptr<juce::Label> label;
            std::unique_ptr<juce::Component> editor; // Slider, ComboBox, or ToggleButton
            bool isSeparator; // If true, draw a line above
        };
        std::vector<InspectorRow> uiRows;
        
        void rebuildUI();
        void createControl(const InspectorControl& def);
        ```
    *   Include `"MappingDefinition.h"`.

2.  **Update `Source/MappingInspector.cpp`:**

    *   **Constructor:** Clear the body. Just `addChangeListener` to deviceManager.
    *   **`setSelection`:** Call `rebuildUI()` after updating `selectedTrees`.
    *   **`valueTreePropertyChanged`:** If the property changed is "type" or "data1" (Command ID), call `rebuildUI()` (because the schema structure changes). For other properties, just `repaint()` or update values (optimization: for now, full rebuild is safer).

    *   **Implement `rebuildUI`:**
        1.  `uiRows.clear();` (This removes old components from parent).
        2.  If `selectedTrees` is empty, return.
        3.  Get Schema: `auto schema = MappingDefinition::getSchema(selectedTrees[0]);`
        4.  Iterate Schema: call `createControl(def)`.
        5.  `resized();`

    *   **Implement `createControl(const InspectorControl& def)`:**
        *   Create `Label` with `def.label`.
        *   **Switch `def.controlType`:**
            *   **Slider:** Create `juce::Slider`. Set Range/Suffix. Bind `onValueChange` to iterate `selectedTrees` and set `def.propertyId`. Set initial value from `selectedTrees[0]`.
            *   **ComboBox:** Create `juce::ComboBox`. Add items from `def.options`. Bind `onChange`. Set initial ID.
            *   **Toggle:** Create `juce::ToggleButton`. Bind `onClick`. Set state.
        *   **Smart Formatting:** If `def.valueFormat == NoteName`, attach the `textFromValueFunction` to the slider using `MidiNoteUtilities`.
        *   Add components to `uiRows` and `addAndMakeVisible`.

    *   **Implement `resized`:**
        *   Iterate `uiRows`.
        *   Stack them vertically.
        *   Standard layout: Label (left 80px) -> Editor (right fill).
        *   If `def.showSeparatorBefore`, add gap.

**Optimization Note:**
For `ActionType`, ensure the ComboBox callback calls `undoManager.beginNewTransaction()` before setting the property, just like the old code.

**Goal:**
When you run the app and select a "Note" mapping, you should see the new dynamic UI (including the new "Follow Transpose" toggle defined in Phase 55.1). If you select a "Command", the UI should morph to show the Command-specific controls.