# 🤖 Cursor Prompt: Phase 7 - Output Inspector & Batch Editing

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 6 Complete. Mappings can be added/loaded but not easily edited.
*   **Phase Goal:** Implement a `MappingInspector` panel that binds to the Table's selection for powerful batch editing.

**Strict Constraints:**
1.  **Undo/Redo:** All edits made via the Inspector **must** be wrapped in a `juce::UndoManager` transaction.
2.  **Thread Safety:** The `InputProcessor` already uses a `ReadWriteLock`. No changes are needed there, but be mindful that UI edits will trigger its listeners.
3.  **CMake:** Provide the snippet to add the new `MappingInspector.cpp`.

---

### Step 1: `MappingInspector` (`Source/MappingInspector.h/cpp`)
Create a class that displays and edits properties of selected mappings.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::ValueTree::Listener`.
2.  **Members:**
    *   `juce::ComboBox typeSelector;`
    *   `juce::Slider channelSlider, data1Slider, data2Slider;`
    *   `juce::Label channelLabel, data1Label, data2Label;`
    *   `juce::UndoManager* undoManager;` (Passed from parent).
    *   `std::vector<juce::ValueTree> selectedTrees;` (Cache of current selection).
3.  **API:**
    *   `void setSelection(const std::vector<juce::ValueTree>& selection)`: This is the main entry point.
        *   **Logic:**
            *   Cache the `selection`.
            *   If selection is empty, disable all controls.
            *   If 1 item selected: Fill controls with its values.
            *   If >1 item selected: For each control, check if all trees have the same value. If yes, show that value. If they differ, show a "Mixed Value" state (e.g., set slider value to -1 and display "---" as text).

### Step 2: Batch Update Logic (Inside `MappingInspector.cpp`)
When a UI control's value changes, update all selected items.

**Implementation (`channelSlider.onValueChange`, etc.):**
1.  Check if `selectedTrees` is empty. If so, return.
2.  `undoManager->beginNewTransaction(...)`.
3.  Iterate through `selectedTrees`.
4.  For each `tree`, call `tree.setProperty("channel", newSliderValue, undoManager)`.
5.  `undoManager->endCurrentTransaction()`.
    *   *Result:* This automatically triggers `valueTreePropertyChanged` in both `InputProcessor` and `MappingEditorComponent`, ensuring the audio engine and table view update instantly.

### Step 3: `MappingEditorComponent` (Layout & Selection)
Refactor this component to own and manage the Inspector.

**Requirements:**
1.  **Members:**
    *   `MappingInspector inspector;`
    *   `juce::UndoManager undoManager;`
2.  **Constructor:**
    *   Pass `&undoManager` to the `inspector` constructor.
    *   Call `table.setMultipleSelectionEnabled(true)`.
3.  **Layout:** Use `FlexBox` or `resized()` to create a side-by-side layout (e.g., Table on left, Inspector on right).
4.  **Selection Logic:**
    *   Override `TableListBoxModel::selectedRowsChanged(int lastRowSelected)`.
    *   Inside:
        *   Get `table.getSelectedRows()`.
        *   Create a `std::vector<juce::ValueTree>` by getting the children from `PresetManager`.
        *   Call `inspector.setSelection(...)` with the new vector.

### Step 4: Update `CMakeLists.txt`
Add `Source/MappingInspector.cpp` to `target_sources`.

### Step 5: `MainComponent` (Undo/Redo Wiring)
The top-level component needs to handle the Undo/Redo key commands.

**Requirements:**
1.  **Inheritance:** Inherit from `juce::ApplicationCommandTarget`.
2.  **Command Manager:** `juce::ApplicationCommandManager commandManager;`
3.  **Constructor:**
    *   Register basic commands (`undo`, `redo`).
    *   Set `MainComponent` as the `nextTarget` for the `commandManager`.
4.  **Overrides:**
    *   Implement `getAllCommands`, `getCommandInfo`, `perform`.
    *   In `perform`, check for the Undo/Redo command IDs and call the `UndoManager` owned by `MappingEditorComponent`.

---

**Generate code for: `MappingInspector`, updated `MappingEditorComponent`, updated `MainComponent`, and the `CMakeLists.txt` snippet.**