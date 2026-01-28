# 🤖 Cursor Prompt: Phase 41 (Stable) - Static Layer UI & Logic

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Phase 40 Complete (Backend supports layers). Previous attempts at Phase 41 caused infinite recursion/hangs on startup due to dynamic layer creation.
*   **Goal:** Implement the Layer UI using a **Static 9-Layer System** (Fixed Banks).
    *   Layer 0 = Base.
    *   Layers 1-8 = Overlay Banks.
    *   No Add/Remove allowed. Renaming allowed.

**Strict Constraints:**
1.  **PresetManager:** In the constructor (and after loading XML), strictly enforce that Layers 0-8 exist.
2.  **InputProcessor:** Remove any logic that tries to create layers. It should expect a vector of size 9.
3.  **LayerListPanel:** Remove Add/Remove buttons. Display exactly 9 rows.

---

### Step 1: Update `Source/PresetManager.h` & `.cpp`
**1. Header:**
*   Remove `addLayer`, `removeLayer`.
*   Add `void ensureStaticLayers();`

**2. Implementation:**
*   **`ensureStaticLayers`:**
    *   Get (or create) the main "Layers" list node.
    *   Loop `i` from 0 to 8.
    *   Check if a child with `id == i` exists.
    *   **If missing:** Create `ValueTree("Layer")`. Set `id` to `i`. Set `name` ("Base" or "Layer X"). Add to parent.
*   **Constructor:** Call `ensureStaticLayers()` immediately.
*   **`loadFromFile`:** Call `ensureStaticLayers()` *after* parsing the XML (to fill in any missing banks from older presets).
*   **`getLayerNode(int index)`:** Simple iteration to find the child. Return `juce::ValueTree()` (invalid) if out of bounds, do **not** create.

### Step 2: Update `Source/LayerListPanel.h` & `.cpp`
**1. Header:**
*   Remove `addButton`, `removeButton`.

**2. Implementation:**
*   **Constructor:** Remove button setup. Just setup `listBox`.
*   **`getNumRows`:** Return 9.
*   **`paintListBoxItem`:**
    *   Get Layer Node `i`.
    *   Draw Name (e.g., "0: Base", "1: Shift").
    *   Highlight row 0 slightly differently (Bold).
*   **`listBoxItemDoubleClicked`:**
    *   Launch AlertWindow to rename the layer property `name`.

### Step 3: Update `Source/InputProcessor.cpp`
**1. Constructor:**
*   `layers.resize(9);`

**2. `rebuildMapFromTree`:**
*   `layers.assign(9, Layer());` (Reset to 9 empty layers).
*   `layers[0].isActive = true;`
*   Iterate `presetManager.getLayersList()` children.
    *   Read `id`. If valid (0-8), populate `layers[id]`.

### Step 4: Update `Source/MappingEditorComponent.cpp`
**1. Layout:**
*   Use `juce::Grid` (1 row, 2 columns).
    *   Col 1 (20%): `layerListPanel`.
    *   Col 2 (80%): `table`.
*   Remove any "Add Layer" button logic.

**2. Interaction:**
*   When `layerListPanel` selection changes:
    *   Update `currentLayerId`.
    *   `table.updateContent()`.

---

**Generate code for: `PresetManager.h/cpp`, `LayerListPanel.h/cpp`, `InputProcessor.cpp`, and `MappingEditorComponent.cpp`.**