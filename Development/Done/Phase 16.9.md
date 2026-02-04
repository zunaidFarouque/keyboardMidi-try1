# 🤖 Cursor Prompt: Phase 16.9 - The Master Zone Compiler

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 16.8 Complete. `Zone` calculates notes efficiently, but `ZoneManager` still loops through every single zone to check if an input matches.
*   **Phase Goal:** Implement a "Master Lookup Table" in `ZoneManager`. This compiles all active Zones into a single `std::unordered_map` for instant access.

**Strict Constraints:**
1.  **Performance:** `handleInput` must NOT iterate. It must use `map.find()`.
2.  **Thread Safety:** The map is built on the Message Thread (Editing) and read on the Audio Thread. Use `juce::ReadWriteLock`.
3.  **Conflict Resolution:** If two Zones share the same Key on the same Alias, the most recently added Zone takes priority.

---

### Step 1: Update `ZoneManager.h`
Add the lookup structure.

**Requirements:**
1.  **New Member:** `std::unordered_map<InputID, Zone*> zoneLookupTable;`
2.  **New Method:** `void rebuildLookupTable();`
3.  **Update:** `addZone`, `removeZone` must call this rebuild method.

### Step 2: Implement `ZoneManager.cpp`
Switch the logic from Iteration to Lookup.

**1. `rebuildLookupTable()`:**
*   Acquire `ScopedWriteLock`.
*   Clear `zoneLookupTable`.
*   Iterate through `zones` vector.
*   For each `zone`:
    *   Get `zone->inputKeyCodes` (You may need to ensure `Zone` exposes this getter).
    *   For each `keyCode`:
        *   Construct `InputID id { zone->targetAliasHash, keyCode };`
        *   `zoneLookupTable[id] = zone.get();` (Overwrites previous entries, effectively giving priority to later zones).

**2. `handleInput(InputID input)`:**
*   Acquire `ScopedReadLock`.
*   `auto it = zoneLookupTable.find(input);`
*   If `it == zoneLookupTable.end()`, return `nullopt`.
*   Else: `return it->second->processKey(input, globalChromTrans, globalDegTrans);`

**3. `simulateInput(...)`:**
*   Update this to use the map lookup as well, keeping logic consistent with `handleInput`.

### Step 3: Update `Zone` (`Source/Zone.h`)
Ensure data is accessible.
*   Make sure `getInputKeyCodes()` returns `const std::vector<int>&`.

### Step 4: Triggers (UI Integration)
When we edit a Zone (add keys, change alias), the Manager needs to know.

*   **Update `ZonePropertiesPanel`**:
    *   In the "Assign Keys" logic (when capture finishes) or when "Alias" changes:
    *   Call `zoneManager.rebuildLookupTable()` (you might need to expose this public, or add a `refresh()` wrapper).

---

**Generate code for: Updated `ZoneManager.h`, Updated `ZoneManager.cpp`, Updated `Zone.h`, and the trigger update in `ZonePropertiesPanel.cpp`.**