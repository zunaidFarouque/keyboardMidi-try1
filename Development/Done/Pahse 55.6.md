### 📋 Cursor Prompt: Phase 55.6 - Smart Layouts & interaction Fixes

**Target Files:**
1.  `Source/MappingDefinition.h`
2.  `Source/MappingDefinition.cpp`
3.  `Source/MappingInspector.h`
4.  `Source/MappingInspector.cpp`

**Task:**
1.  **Fix Layout:** Allow multiple controls on one row (e.g., side-by-side toggles).
2.  **Fix Interaction:** Prevent `rebuildUI` from firing during slider drags unless strictly necessary.

**Specific Instructions:**

1.  **Update `Source/MappingDefinition.h`:**
    *   Add `bool sameLine = false;` to `InspectorControl`.
    *   Add `float widthWeight = 1.0f;` to `InspectorControl` (to control how much space it takes, e.g., 0.5 for half width).

2.  **Update `Source/MappingDefinition.cpp`:**
    *   **In "Note" Schema:**
        *   "Ignore Global Transpose": Set `widthWeight = 0.5f`.
        *   "Send Note Off": Set `sameLine = true`, `widthWeight = 0.5f`.
    *   **In "CC" Schema:**
        *   "Send Value on Release": Set `widthWeight = 0.5f`.
        *   "Release Value": Set `sameLine = true`, `widthWeight = 0.5f`.

3.  **Update `Source/MappingInspector.h`:**
    *   Refactor the storage structure to support multiple items per row.
    *   **Replace:** `struct InspectorRow { ... };` and `std::vector<InspectorRow> uiRows;`
    *   **With:**
        ```cpp
        struct UiItem {
            std::unique_ptr<juce::Component> component; // Label, Slider, or Toggle
            float weight;
        };
        struct UiRow {
            std::vector<UiItem> items;
            bool isSeparator;
        };
        std::vector<UiRow> uiRows;
        ```

4.  **Update `Source/MappingInspector.cpp`:**

    *   **Refactor `rebuildUI`:**
        *   Iterate the Schema.
        *   If `!def.sameLine` or `uiRows` is empty, push a new `UiRow`.
        *   **Create Components:**
            *   If `Slider/ComboBox`: Create a `GroupComponent` (invisible) holding the Label + Editor, treating them as one unit. Push this unit to `UiRow`.
            *   If `Toggle`: Just push the ToggleButton (it has its own text).
        *   Store `def.widthWeight` in the `UiItem`.

    *   **Refactor `resized`:**
        *   Use `juce::FlexBox` logic manually or built-in.
        *   Iterate `uiRows`. For each row:
            *   Calculate total width.
            *   Distribute width to `items` based on `weight`.
            *   Set bounds.

    *   **Refactor `valueTreePropertyChanged` (THE SLIDER FIX):**
        *   **Logic:**
            ```cpp
            bool needsRebuild = false;
            
            if (property == juce::Identifier("type") || 
                property == juce::Identifier("adsrTarget")) {
                needsRebuild = true;
            }
            else if (property == juce::Identifier("data1")) {
                // Only rebuild for Command type (where data1 changes the UI structure)
                // For Note/CC, data1 is just a value change.
                if (allTreesHaveSameValue("type") && 
                    getCommonValue("type").toString() == "Command") {
                    needsRebuild = true;
                }
            }

            if (needsRebuild) {
                juce::MessageManager::callAsync([this]() { rebuildUI(); });
            } else {
                // If not rebuilding, we rely on the Sliders' internal ValueTree 
                // listeners to update themselves (which they do automatically 
                // if we bind them correctly).
                // Or call checkVisibility() if we had dynamic hiding.
                repaint();
            }
            ```

**Important Detail for Sliders:**
When you create the `juce::Slider` in `createControl`, ensure it is utilizing `juce::ValueTree::PropertyAsValue` or explicitly listening to the tree, OR simply accept that `repaint()` won't move the slider handle if you dragged it, but the internal data is updating.
*Actually, standard JUCE Sliders don't listen to ValueTrees automatically.*
**Correction:** In `createControl`, when setting up the Slider `onValueChange`, that handles User -> Data.
For Data -> User (External updates or undo):
Since we are NOT rebuilding, the slider needs to update its text/position if the data changes from *elsewhere* (like Undo).
**For now:** The primary fix is preventing `rebuildUI` while dragging. The slider will update itself visually because *you* are dragging it. The `repaint()` is sufficient for the rest of the UI.

**Goal:**
1.  Checkboxes appear side-by-side (saving vertical space).
2.  Dragging the "Note" slider is smooth and doesn't interrupt the interaction.