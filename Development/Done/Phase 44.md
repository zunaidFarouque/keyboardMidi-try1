This phase addresses the missing controls in the Inspector and the visual polish of the Layer List.

We need to make the **Mapping Inspector** context-aware for Commands.
*   **Current Logic:** If Type == Command, show Command Selector. Hide everything else.
*   **New Logic:** If Command == "Layer Switch", **Show Target Layer Selector**.

We also need to make the **Layer List** look like a professional sidebar (padding, better highlighting) rather than a raw list.

Here is the prompt for **Phase 44**.

***

# 🤖 Cursor Prompt: Phase 44 - Layer Command Targets & UI Polish

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
We are building "MIDIQy".
*   **Current State:**
    1.  **Inspector:** When selecting "Layer Momentary" or "Layer Toggle" commands, there is **no input** to select *which* layer to trigger. The `data1` field (Target Layer) is hidden.
    2.  **Layer List:** The `LayerListPanel` looks raw and unstyled ("Bad UI").
*   **Phase Goal:**
    1.  Add a "Target Layer" ComboBox to `MappingInspector` that appears only for Layer commands.
    2.  Style the `LayerListPanel` with custom drawing (padding, rounded selection).

**Strict Constraints:**
1.  **Data Binding:** The new "Target Layer" selector must write to the `data1` property of the mapping.
2.  **Dynamic Visibility:**
    *   If Command is `Panic` or `Sustain`: Hide Target Layer selector.
    *   If Command is `Layer...`: Show Target Layer selector.
3.  **UI Polish:** Use `lookAndFeel` or custom `paintListBoxItem` logic to make the Layer List look like a side navigation bar (darker background, lighter text, 4px rounded selection).

---

### Step 1: Update `MappingInspector` (`Source/MappingInspector.h/cpp`)
Add the missing control.

**1. Header:**
*   Add `juce::ComboBox targetLayerSelector;`
*   Add `juce::Label targetLayerLabel;`

**2. Constructor:**
*   Populate `targetLayerSelector` with items "0: Base", "1: Layer 1", ... "8: Layer 8".
*   **Callback:** On change, update `data1` property of selected trees.

**3. Update `setSelection` & `commandSelector.onChange`:**
*   We need a helper `updateVisibility()`.
*   **Logic:**
    *   Get selected Command ID.
    *   `bool isLayerCmd = (cmd >= LayerMomentary && cmd <= LayerSolo);` (Check your Enum values in `MappingTypes.h`).
    *   `targetLayerSelector.setVisible(isLayerCmd);`
    *   `targetLayerLabel.setVisible(isLayerCmd);`

**4. `resized`:**
*   Position the new selector below the Command selector.

### Step 2: Update `LayerListPanel` (`Source/LayerListPanel.cpp`)
Polish the visuals.

**1. Refactor `paintListBoxItem`:**
*   **Background:**
    *   If `rowIsSelected`: Draw a `roundedRectangle` with `Colours::lightblue.withAlpha(0.2f)` and 4px corners. Insert 2px padding on sides.
    *   Else: Transparent.
*   **Text:**
    *   Font: 15.0f.
    *   Color: `rowIsSelected ? Colours::white : Colours::grey`.
    *   Indentation: Left-justify with 10px margin.
*   **Row 0 (Base):** Maybe draw a small "Lock" icon or text `(Default)` to show it's special.

**2. Component Paint:**
*   Set the background of the `ListBox` itself to a darker shade (e.g., `0xff1a1a1a`) to separate it from the table.

---

**Generate code for: Updated `MappingInspector.h`, Updated `MappingInspector.cpp`, and Updated `LayerListPanel.cpp`.**