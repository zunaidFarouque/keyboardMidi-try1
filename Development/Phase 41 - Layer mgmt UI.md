# 🤖 Cursor Prompt: Phase 41 - Layer Management UI

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 40 Complete. The Backend (`InputProcessor`) supports multiple Layers (Banks), but the UI (`MappingEditorComponent`) still views a flat list of mappings and assumes everything is in Layer 0.
*   **Phase Goal:** Implement a `LayerListPanel` to create/select layers, and refactor `MappingEditorComponent` to display mappings for the **Selected Layer** only.

**Strict Constraints:**
1.  **Tree Structure:** The `ValueTree` schema has changed. Mappings are now children of a `<Layer>` node, not the root.
    *   *Old:* `<Mappings><Mapping.../></Mappings>`
    *   *New:* `<Layers><Layer id="0" name="Base"><Mapping.../></Layer>...</Layers>`
2.  **Safety:** Layer 0 ("Base") is permanent. The "Remove" button must be disabled for Layer 0.
3.  **Command Binding:** The `MappingInspector` must now allow selecting `LayerMomentary`, `LayerToggle`, and `LayerSolo` commands.

---

### Step 1: Update `PresetManager` (`Source/PresetManager.h/cpp`)
Handle the new Tree hierarchy.

**Methods:**
1.  `juce::ValueTree getLayersNode();` (Creates it if missing).
2.  `juce::ValueTree getLayerNode(int layerIndex);` (Returns the specific child tree).
3.  `void addLayer(String name);`
4.  `void removeLayer(int layerIndex);`
5.  **Refactor `getMappingsNode`**: This method is now ambiguous. Rename/Overload it to `getMappingsNode(int layerId)` so the Table knows which list to show.

### Step 2: `LayerListPanel` (`Source/LayerListPanel.h/cpp`)
Manage the Banks.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::ListBoxModel`, `juce::ValueTree::Listener` (listen to `Layers` node).
2.  **Members:** `juce::ListBox list;`, `juce::TextButton addButton, removeButton;`.
3.  **API:** `std::function<void(int layerId)> onLayerSelected;`
4.  **Logic:**
    *   **Draw:** Show Layer Name and ID. Highlight selected.
    *   **Add:** Create new Layer Node.
    *   **Remove:** Delete selected Layer Node (Ignore if ID 0).
    *   **Selection:** Trigger callback when user clicks a row.

### Step 3: Update `MappingEditorComponent`
Integrate the Layer List.

**1. Layout:**
*   Use `juce::Grid` or `FlexBox`.
*   **Left (20%):** `LayerListPanel`.
*   **Right (80%):** `TableListBox` (The existing table).

**2. Logic:**
*   Member: `int currentLayerId = 0;`
*   **Callback:** Bind `layerList.onLayerSelected = [this](int id) { currentLayerId = id; table.updateContent(); };`
*   **Data Source:** Update all table methods (`getNumRows`, `paintCell`) to use `presetManager.getMappingsNode(currentLayerId)`.
*   **Add Button:** When adding a mapping, ensure it is added to `getMappingsNode(currentLayerId)`.

### Step 4: Update `MappingInspector`
Add the new commands.

**Update `Constructor`:**
*   Add items to `commandSelector`:
    *   "Layer Hold (Momentary)"
    *   "Layer Toggle"
    *   "Layer Solo"
*   **Dynamic UI:** If one of these is selected, show `data1Slider` but rename label to **"Target Layer ID"**.

---

**Generate code for: Updated `PresetManager`, `LayerListPanel`, Updated `MappingEditorComponent`, and Updated `MappingInspector`.**