# 🤖 Cursor Prompt: Phase 21 - Global Scale & Root Inheritance

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 20 Complete. Zones operate independently with their own Root and Scale.
*   **Phase Goal:** Implement "Global Scale" and "Global Root" concepts in `ZoneManager`. Update `Zone` to optionally inherit these settings instead of using local values.

**Strict Constraints:**
1.  **Architecture:** `Zone` should remain a logic unit. It should not query `ZoneManager` directly. Instead, `ZoneManager` should push global state into `Zone` during the cache rebuild process.
2.  **Serialization:** Save/Load the new Global settings (in `ZoneManager`) and the "Use Global" flags (in `Zone`).
3.  **Compiling:** When Global Settings change, **only** Zones flagged to use them should rebuild their cache.

---

### Step 1: Update `ZoneManager` (`Source/ZoneManager.h/cpp`)
Store the Global Context.

**Requirements:**
1.  **Members:**
    *   `juce::String globalScaleName = "Major";`
    *   `int globalRootNote = 60;` (Middle C).
2.  **Methods:**
    *   `void setGlobalScale(String name);`
    *   `void setGlobalRoot(int root);`
    *   Getters for both.
3.  **Logic Update (`setGlobalScale/Root`):**
    *   Update the member.
    *   Iterate `zones`.
    *   **If** `zone->usesGlobalScale()` (or Root), call `zone->rebuildCache(...)`.
    *   Call `rebuildLookupTable()` at the end.
4.  **Serialization:** Update `toValueTree` / `restoreFromValueTree` to persist these two global values.

### Step 2: Update `Zone` (`Source/Zone.h/cpp`)
Add inheritance flags.

**Requirements:**
1.  **Members:**
    *   `bool useGlobalScale = false;`
    *   `bool useGlobalRoot = false;`
2.  **Serialization:** Save/Load these bools.
3.  **Refactor `rebuildCache`:**
    *   **New Signature:** `void rebuildCache(const std::vector<int>& scaleIntervals, int effectiveRoot);`
    *   *Note:* The caller (`ZoneManager` or `PropertiesPanel`) is now responsible for deciding *which* intervals/root to pass.
    *   **Logic:**
        *   Remove internal lookups. Use the `scaleIntervals` passed in.
        *   Use the `effectiveRoot` passed in.
        *   Run existing Chord/Note generation logic.

### Step 3: Update Call Sites (Fixing the Refactor)
Now that `rebuildCache` requires arguments, we must fix the callers.

**1. `ZoneManager::addZone / rebuildLookupTable`:**
*   When calling rebuild on a zone, check its flags.
*   If `useGlobalScale`: Pass `scaleLibrary.getIntervals(globalScaleName)`.
*   Else: Pass `scaleLibrary.getIntervals(zone->scaleName)`.
*   (Same logic for Root).

**2. `ZonePropertiesPanel.cpp`:**
*   This panel calls `currentZone->rebuildCache()` when sliders move.
*   Update it to query `zoneManager` for the global context if the checkboxes (which we will add in Phase 22) would be checked.
*   *For this Phase:* Just pass `scaleLibrary.getIntervals(currentZone->scaleName)` and `currentZone->rootNote` to maintain current behavior until the UI is updated.

---

**Generate code for: Updated `ZoneManager`, Updated `Zone`, and fixed call sites in `ZonePropertiesPanel`.**