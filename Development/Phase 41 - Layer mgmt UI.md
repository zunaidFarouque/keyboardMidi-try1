# 🤖 Cursor Prompt: Phase 41 - Layer Management UI & Data Structure

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 40 Complete. The Backend (`InputProcessor`) supports Layers, but the UI (`MappingEditorComponent`) is broken because it expects a flat list of mappings. It doesn't know how to read/write the new `<Layer>` XML structure.
*   **Phase Goal:** Implement the UI to Manage Layers (Add, Remove, Rename) and update the Mapping Editor to show only the selected Layer.

**Strict Constraints:**
1.  **XML Structure:** We are moving from a Flat List to a Hierarchy:
    *   **Root** -> `Layers` (List) -> `Layer` (ValueTree) -> `Mappings` (List) -> `Mapping` (ValueTree).
    *   *Note:* Ensure `PresetManager` handles migration/creation of this structure if missing.
2.  **Layer 0:** The Base Layer (ID 0) cannot be removed.
3.  **Renaming:** Users must be able to rename layers via the UI.

---

### Step 1: Update `PresetManager` (`Source/PresetManager.h/cpp`)
Refactor for Hierarchy.

**Methods:**
1.  `juce::ValueTree getLayersList();` (Returns the parent node for layers).
2.  `juce::ValueTree getLayerNode(int layerIndex);`
    *   Iterate children of `getLayersList()`. Find one with property `id == layerIndex`.
    *   If not found (and index is valid), create it.
3.  `void addLayer(String name);`
    *   Find highest ID + 1. Create new child.
4.  `void removeLayer(int layerIndex);` (Ignore if 0).
5.  **Helper:** `juce::ValueTree getMappingsListForLayer(int layerIndex);`
    *   Get Layer Node -> Get Child "Mappings" (create if missing).

### Step 2: `LayerListPanel` (`Source/LayerListPanel.h/cpp`)
The Sidebar.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::ListBoxModel`.
2.  **Logic:**
    *   Display list of Layers (Name + ID).
    *   **Highlight** the currently selected row.
    *   **Double Click:** Rename Layer (AlertWindow).
3.  **Callback:** `std::function<void(int layerId)> onLayerSelected;`

### Step 3: Update `MappingEditorComponent`
Connect the List to the Table.

**1. Layout:**
    *   Grid: Left 20% (`LayerListPanel`), Right 80% (`TableListBox`).
**2. Logic:**
    *   Member `int selectedLayerId = 0;`
    *   **Data Source:** Update `getNumRows` and `paintCell` to call `presetManager.getMappingsListForLayer(selectedLayerId)`.
    *   **Add Button:** When adding a row, insert it into the *currently selected layer's* mapping list.

### Step 4: Update `MappingInspector`
Add Layer Commands.

**1. Update `commandSelector`:**
    *   Add items: "Layer Momentary (Hold)", "Layer Toggle", "Layer Solo".
**2. UI Logic:**
    *   If a Layer Command is selected, show `data1Slider` but label it **"Target Layer ID"**.
    *   Range: 0 to 8.

### Step 5: `InputProcessor` (XML Update)
Ensure `rebuildMapFromTree` handles the new hierarchy.
*   Instead of `presetManager.getMappingsNode()`, it should:
    *   Iterate `presetManager.getLayersList()` children.
    *   For each Layer, iterate its "Mappings" child.
    *   Populate `layers[id].compiledMap` / `configMap`.

---

**Generate code for: `PresetManager`, `LayerListPanel`, Updated `MappingEditorComponent`, Updated `MappingInspector`, and Updated `InputProcessor` (XML Parsing logic).**