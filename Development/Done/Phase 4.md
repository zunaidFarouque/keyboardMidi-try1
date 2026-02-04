# 🤖 Cursor Prompt: Phase 4 - State Management (ValueTree)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 3 Complete. We have `InputProcessor` using a `std::unordered_map` for lookups, but it is currently populated via `loadDebugMappings()`.
*   **Phase Goal:** Replace hardcoded mappings with a dynamic `juce::ValueTree` system, implement XML Save/Load, and create a GUI Table to view mappings.

**Strict Constraints:**
1.  **Thread Safety:** The `InputProcessor` map is read by the Input Thread and written by the UI Thread. You **must** use `juce::ReadWriteLock`.
2.  **CMake:** You must provide the snippet to update `target_sources`.

---

### Step 1: `PresetManager` (`Source/PresetManager.h/cpp`)
Create a class to own the data.

**Requirements:**
1.  **Member:** `juce::ValueTree rootNode;` (Identifier: `"MIDIQyPreset"`).
2.  **Constructor:** Check if `rootNode` has a `"Mappings"` child. If not, create it.
3.  **Persistence:**
    *   `void saveToFile(juce::File file)`: Writes `rootNode` to XML.
    *   `void loadFromFile(juce::File file)`: Loads XML and replaces `rootNode`.
4.  **Access:** `juce::ValueTree getMappingsNode()`: Returns the child node containing mappings.

**Data Schema (XML Concept):**
```xml
<Mapping inputKey="81" deviceHash="123456" type="Note" channel="1" data1="60" data2="127"/>
```

### Step 2: Refactor `InputProcessor` (The Observer)
Modify `InputProcessor` to listen to the `ValueTree` instead of being hardcoded.

**Requirements:**
1.  **Inheritance:** Inherit from `juce::ValueTree::Listener`.
2.  **Members:**
    *   `juce::ReadWriteLock mapLock;` (Crucial for thread safety).
    *   Reference to `PresetManager` (passed in constructor).
3.  **Logic:**
    *   In Constructor: `rootNode.addListener(this)`. Iterate existing tree children and populate `keyMapping`.
    *   `valueTreeChildAdded` / `valueTreeChildRemoved` / `valueTreePropertyChanged`:
        *   **Acquire Write Lock.**
        *   Update the `keyMapping` `std::unordered_map` to match the Tree.
        *   **Release Lock.**
4.  **Process Event:**
    *   **Acquire Read Lock.**
    *   Perform the lookup.
    *   **Release Lock.**

### Step 3: `MappingEditorComponent` (`Source/MappingEditorComponent.h/cpp`)
A simple table to view the data.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::TableListBoxModel`.
2.  **Members:** `juce::TableListBox table;`, Reference to `PresetManager`.
3.  **Constructor:** Setup table headers ("Key", "Type", "Note", "Vel").
4.  **Data Source:** Read directly from `presetManager.getMappingsNode()`.
    *   *Note:* In `paintRowBackground` and `paintCell`, just read attributes from the `ValueTree` child at `rowNumber`.
5.  **Interaction:**
    *   Add a "+" Button.
    *   `onClick`: Adds a new child `ValueTree` with default values (e.g., Key Q -> Note 60) to the Mappings node.

### Step 4: Update `CMakeLists.txt`
Add `Source/PresetManager.cpp` and `Source/MappingEditorComponent.cpp` to `target_sources`.

### Step 5: Integration (`MainComponent`)
1.  **Members:** Add `PresetManager` and `MappingEditorComponent`.
    *   *Order:* `PresetManager` must be declared *before* `InputProcessor` and `Editor`.
2.  **Constructor:**
    *   Pass `presetManager` to `InputProcessor`.
    *   Pass `presetManager` to `MappingEditorComponent`.
    *   Add "Save" and "Load" buttons that call `presetManager.saveToFile(...)` (use `juce::FileChooser`).
    *   Remove `loadDebugMappings()`.

---

**Generate code for: `PresetManager`, `MappingEditorComponent`, updated `InputProcessor`, updated `MainComponent`, and the `CMakeLists.txt` snippet.**