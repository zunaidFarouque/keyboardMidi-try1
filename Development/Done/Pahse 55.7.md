Here is the prompt for **Phase 55.7**, which implements the Compact Dependent Layout logic.

### 📋 Cursor Prompt: Phase 55.7 - Compact Dependent Layouts

**Target Files:**
1.  `Source/MappingDefinition.h`
2.  `Source/MappingDefinition.cpp`
3.  `Source/MappingInspector.cpp`

**Task:**
Implement a compact "Checkbox + Slider" row layout for CC Release settings.
1.  Allow defining dependencies (Slider disabled if Checkbox unchecked).
2.  Allow Sliders to have no separate label (saving space).
3.  Configure CC Schema to use this layout.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.h`:**
    *   Add to `InspectorControl` struct:
        ```cpp
        // If set, this control is enabled only when the target property is true
        juce::String enabledConditionProperty; 
        ```

2.  **Update `Source/MappingDefinition.cpp` (`getSchema`):**
    *   **In `ActionType::CC` case:**
        *   **Toggle "Send Value on Release":**
            *   `widthWeight = 0.45f;`
            *   `sameLine = false;` (Start of row)
        *   **Slider "Release Value":**
            *   `label = "";` (Empty label prevents the "Label:" text widget)
            *   `sameLine = true;`
            *   `widthWeight = 0.55f;`
            *   `enabledConditionProperty = "sendReleaseValue";`

3.  **Update `Source/MappingInspector.cpp`:**

    *   **Refactor `createControl`:**
        *   **Handling Empty Labels:**
            *   If `def.label` is empty **AND** control is not a Toggle (Toggles have built-in text):
            *   Do NOT create the container `GroupComponent`.
            *   Just use the `editor` component directly as the `UiItem`.
            *   (This allows the Slider to expand into the full width of its allocation).
        *   **Handling Dependency:**
            *   If `def.enabledConditionProperty` is not empty:
                *   Check the value in `selectedTrees[0]`.
                *   `bool isEnabled = selectedTrees[0].getProperty(def.enabledConditionProperty, false);`
                *   Set the component's enabled state: `component->setEnabled(isEnabled);`

    *   **Refactor `valueTreePropertyChanged`:**
        *   Add `sendReleaseValue` to the list of properties that trigger `needsRebuild = true`.
        *   (This ensures that clicking the checkbox immediately rebuilds the row to enable/disable the slider).

**Outcome:**
When you select a CC mapping:
*   You will see "Send Value on Release" [Checkbox] followed immediately by the [Slider].
*   The Slider will have no text label on the left (compact).
*   Unchecking the box will disable the slider.