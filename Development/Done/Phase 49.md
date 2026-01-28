Here is the prompt for **Phase 49**.

This is a significant architectural upgrade. Previously, Zones were a "Global Fallback" that ran only if no Manual Mapping existed. Now, we are integrating Zones **into the Layer Stack**.

This means you can have a "C Major Zone" on **Layer 0**, and a "Drum Pad Zone" on **Layer 1**. When Layer 1 is active, the Drum Pad overrides the Piano.

***

# 🤖 Cursor Prompt: Phase 49 - Layered Zones Architecture

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
We are building "OmniKey".
*   **Current State:**
    *   Manual Mappings belong to Layers (0-8).
    *   Zones are "Global" and only trigger if *no* manual mapping is found in *any* active layer.
*   **Phase Goal:** Integrate Zones into the Layer System.
    *   Zones can be assigned to a specific Layer (0-8).
    *   `InputProcessor` checks layers Top-Down. For each layer, it checks (Manual Mapping) -> (Zone).

**Strict Constraints:**
1.  **Data Structure:** `ZoneManager` must replace the single `zoneLookupTable` with a `std::vector` of lookup tables (one per layer) to support overlapping zones on different layers.
2.  **Processing Logic:** `InputProcessor` loop must be refactored to check `ZoneManager` *inside* the layer loop, not after it.
3.  **UI:** `ZonePropertiesPanel` needs a Layer Selector.

---

### Step 1: Update `Zone` (`Source/Zone.h/cpp`)
Add the property.

**Requirements:**
1.  **Member:** `int layerID = 0;` (Default Base).
2.  **Serialization:** Save/Load `layerID` in `toValueTree`/`fromValueTree`.

### Step 2: Update `ZoneManager` (`Source/ZoneManager.h/cpp`)
Refactor the Lookup Engine.

**1. Header:**
*   **Old:** `std::unordered_map<InputID, Zone*> zoneLookupTable;`
*   **New:** `std::vector<std::unordered_map<InputID, Zone*>> layerLookupTables;` (Size 9).
*   **Update:** `handleInput(InputID input)` -> `handleInput(InputID input, int layerIndex)`.
*   **Update:** `simulateInput` needs to accept `layerIndex` too.

**2. Implementation:**
*   **Constructor:** `layerLookupTables.resize(9);`
*   **`rebuildLookupTable`:**
    *   Clear all maps.
    *   Iterate zones.
    *   Read `zone->layerID`. Clamp 0-8.
    *   Insert into `layerLookupTables[zone->layerID]`.
*   **`handleInput(input, layerIndex)`:**
    *   Check bounds.
    *   Look up in `layerLookupTables[layerIndex]`.
    *   Return action if found.

### Step 3: Update `InputProcessor.cpp`
Interleave the Logic.

**Refactor `processEvent` (and `simulateInput`):**
*   **Current Loop:** Checks `layers[i].compiledMap`.
*   **New Loop:**
    1.  Iterate `i` from 8 down to 0.
    2.  If `!layers[i].isActive` continue.
    3.  **Check Manual:** `layers[i].compiledMap.find(...)`. If match, execute & return.
    4.  **Check Zone:** `zoneManager.handleInput(input, i)`. If match, execute & return.
*   **Remove:** The "Fallback" code at the bottom of the function (since Zones are now checked inside the loop).
    *   *Note:* Ensure the "Global Device ID" fallback logic works inside the loop too (Check Specific, then Check Global for *that layer*).

### Step 4: Update `ZonePropertiesPanel`
Add the UI.

**1. Header:**
*   `juce::Label layerLabel;`
*   `juce::ComboBox layerSelector;`

**2. Constructor:**
*   Populate `layerSelector` ("0: Base", "1: Layer 1"...).
*   **Callback:** Update `currentZone->layerID`, call `rebuildLookupTable`, `sendChangeMessage`.

**3. `setZone`:**
*   Update selector from `zone->layerID`.

**4. `resized`:**
*   Place it near the top (e.g., under Alias/Name).

---

**Generate code for: Updated `Zone.h/cpp`, Updated `ZoneManager.h/cpp`, Updated `InputProcessor.cpp` (Critical Logic Change), and Updated `ZonePropertiesPanel.h/cpp`.**