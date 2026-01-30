### 📋 Cursor Prompt: Phase 55.9 - Enhanced Separators & Modular Layouts

**Target Files:**
1.  `Source/MappingDefinition.h`
2.  `Source/MappingDefinition.cpp`
3.  `Source/MappingInspector.h`
4.  `Source/MappingInspector.cpp`

**Task:**
1.  **Explicit Separators:** Add `Type::Separator` to `InspectorControl` with support for labels and alignment (Left/Center/Right).
2.  **Migration:** Replace the `showSeparatorBefore` boolean with explicit Separator items in the schema.
3.  **Visualization:** Implement a `SeparatorComponent` that draws the lines and text correctly.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.h`:**
    *   Update `InspectorControl` struct:
        *   Add `Separator` to the `Type` enum.
        *   Add `juce::Justification separatorAlign = juce::Justification::centred;`
        *   **Remove** `bool showSeparatorBefore;` (Cleanup).

2.  **Update `Source/MappingDefinition.cpp`:**
    *   **Helper:** Create a helper to generate a separator easily:
        ```cpp
        static InspectorControl createSeparator(const juce::String& label = "", juce::Justification align = juce::Justification::centred) {
            InspectorControl c;
            c.controlType = InspectorControl::Type::Separator;
            c.label = label;
            c.separatorAlign = align;
            return c;
        }
        ```
    *   **Refactor `getSchema`:**
        *   Where you previously set `showSeparatorBefore = true`, insert a `createSeparator()` item into the vector instead.
        *   **Example (Note):**
            *   Insert `createSeparator("Note Settings", juce::Justification::centredLeft)` before "Transpose".
        *   **Example (CC):**
            *   Insert `createSeparator()` (Line only) before "Release Mode".

3.  **Update `Source/MappingInspector.h`:**
    *   Add a private inner class `SeparatorComponent : public juce::Component`.

4.  **Update `Source/MappingInspector.cpp`:**

    *   **Implement `SeparatorComponent`:**
        *   **Paint:**
            *   Get bounds. Calculate vertical centre.
            *   If label is empty: Draw line across full width (`g.setColour(juce::Colours::grey); g.fillRect(...)`).
            *   If label exists:
                *   Measure text width.
                *   Determine text position based on `justification`.
                *   Draw text.
                *   Draw lines on remaining space (left/right of text), leaving a small padding (e.g., 5px) around the text.
                *   *Style:* Use `Colours::lightgrey` for text, `Colours::grey` for lines. Font height ~14px bold?

    *   **Refactor `rebuildUI`:**
        *   Inside the loop iterating the schema:
        *   If `def.controlType == Type::Separator`:
            *   **Force New Row:** Even if `sameLine` was somehow set, Separators should probably claim a full row for aesthetics. Push current `uiRow` if not empty.
            *   Create `SeparatorComponent`.
            *   Push a new `UiRow` containing just this separator (Weight 1.0).
            *   Continue loop.

    *   **Refactor `createControl`:**
        *   Remove any logic related to `showSeparatorBefore`.

**Outcome:**
*   You will see distinct sections in the inspector (e.g., "---- Note Settings ----").
*   The system handles "Line Only" or "Labelled Line".
*   The "Label - Tick - Slider" layout confirmed to work via the existing `LabeledControl` + `sameLine` logic (verified in thought process: `[Label][Tick]` comes from Item 1, `[Slider]` comes from Item 2).