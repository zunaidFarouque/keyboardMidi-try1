# 🤖 Cursor Prompt: Phase 45 - Layer Selection Persistence & Visualizer HUD

**Role:** Expert C++ Audio Developer (JUCE UI & State Management).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 44 Complete.
*   **Issue 1:** Switching Layers in `MappingEditor` clears the table selection, disabling the Inspector. The user has to re-select a row every time they switch tabs.
*   **Issue 2:** The Visualizer shows keys, but doesn't show which **Layers** (Banks) are currently active in the engine (e.g., did I toggle "Shift" on?).

**Phase Goal:**
1.  Implement selection memory per layer in `MappingEditorComponent`.
2.  Add an "Active Layers" status display to `VisualizerComponent`.

**Strict Constraints:**
1.  **Selection Logic:** When switching layers, if the saved row index is out of bounds for the new layer (e.g., Layer 1 has 5 rows, Layer 2 has 0), deselect cleanly.
2.  **Thread Safety:** `Visualizer` reads from `InputProcessor` (Audio Thread state). Use `ScopedReadLock` or atomic copies when getting the list of active layer names.

---

### Step 1: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Expose the state.

**Method:** `juce::StringArray getActiveLayerNames();`
*   **Logic:**
    *   Acquire `ScopedReadLock` (mapLock).
    *   Iterate `layers`.
    *   If `layer.isActive`: Add `layer.name` to list.
    *   Return list.

### Step 2: Update `VisualizerComponent` (`Source/VisualizerComponent.cpp`)
Draw the HUD.

**Refactor `paint`:**
*   In the Header Bar area (next to Sustain/Transpose info):
*   Call `inputProcessor.getActiveLayerNames()`.
*   Join them with `" | "`.
*   Draw text: `"LAYERS: " + joinedString`.
*   *Color:* Use a distinct color (e.g., `Colours::cyan`) to show these are active modes.

### Step 3: Update `MappingEditorComponent` (`Source/MappingEditorComponent.h/cpp`)
Implement Selection Persistence.

**1. Header:**
*   Add member: `std::map<int, int> layerSelectionHistory;`

**2. Implementation (Constructor/Lambda):**
*   **Update `layerList.onLayerSelected`:**
    ```cpp
    layerList.onLayerSelected = [this](int newLayerId) {
        // 1. Save Current Selection
        int currentRow = table.getSelectedRow();
        layerSelectionHistory[currentLayerId] = currentRow;

        // 2. Switch Data
        currentLayerId = newLayerId;
        table.updateContent();

        // 3. Restore Selection
        int savedRow = -1;
        if (layerSelectionHistory.count(newLayerId))
            savedRow = layerSelectionHistory[newLayerId];

        // 4. Validate & Apply
        if (savedRow >= 0 && savedRow < getNumRows())
        {
            table.selectRow(savedRow);
        }
        else
        {
            table.deselectAllRows();
        }
        
        // 5. Force Inspector Update (if deselecting, this clears inspector)
        // TableListBox::selectRow usually triggers selectedRowsChanged, but explicit is safer if needed.
    };
    ```

---

**Generate code for: `InputProcessor.h/cpp` (New getter), `VisualizerComponent.cpp` (HUD Update), and `MappingEditorComponent.h/cpp` (Persistence Logic).**