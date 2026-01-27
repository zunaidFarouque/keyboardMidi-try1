# 🤖 Cursor Prompt: Phase 41.1 - Layer Management UI & Logic

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 41 Complete. The `LayerListPanel` shows a raw list like "0: Base", but lacks controls to Add or Remove layers. The UI is confusing.
*   **Phase Goal:** Upgrade `LayerListPanel` to be a full manager. Add Header, Add/Remove buttons, and implement the backend logic in `PresetManager`.

**Strict Constraints:**
1.  **Layer 0 Protection:** The "Base" layer (ID 0) can **never** be removed. Disable the remove button if Layer 0 is selected.
2.  **ID Management:** When adding a layer, find the first available ID between 1 and 7 (assuming 8 layers max). If full, disable the Add button.
3.  **UI Polish:** Add a header label "Active Layers".

---

### Step 1: Update `PresetManager` (`Source/PresetManager.cpp`)
Implement the Logic.

**1. `addLayer`:**
*   Get `getLayersList()`.
*   Iterate children to find used IDs.
*   Find lowest available ID (1 to 7).
*   If found: Create new Layer ValueTree with `id` and `name="Layer [ID]"`. Add to list.
*   If full: Return/Do nothing.

**2. `removeLayer`:**
*   **Check:** If `layerIndex == 0`, return (Safety).
*   Find child with `id == layerIndex`.
*   `layersList.removeChild(child, nullptr)`.

### Step 2: Update `LayerListPanel` (`Source/LayerListPanel.h/cpp`)
Build the UI.

**1. UI Elements:**
*   `juce::Label headerLabel;` ("Mapping Layers").
*   `juce::TextButton addButton;` ("+").
*   `juce::TextButton removeButton;` ("-").

**2. Layout (`resized`):**
*   **Top Bar (30px):** Header Label (Left), Remove Button (Right), Add Button (Right).
*   **Rest:** ListBox.

**3. Logic:**
*   **Constructor:**
    *   `addButton.onClick`: Call `presetManager.addLayer()`.
    *   `removeButton.onClick`: Call `presetManager.removeLayer(selectedRow)`.
*   **Selection Change:**
    *   If `selectedRow == 0`: `removeButton.setEnabled(false)`.
    *   Else: `removeButton.setEnabled(true)`.
*   **Refresh:** Ensure `valueTreeChildAdded/Removed` listeners call `updateContent()` and `repaint()`.

### Step 3: Visual Polish (`paintListBoxItem`)
Make "Base" look special.
*   If `rowNumber == 0`: Draw text in **Bold** or a slightly different color to indicate it is the master layer.

---

**Generate code for: Updated `PresetManager.cpp` and Updated `LayerListPanel.h/cpp`.**