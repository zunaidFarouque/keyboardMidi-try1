# 🤖 Cursor Prompt: Phase 39.3 - Fix Visualizer Lookup (Dual Map Architecture)

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 39.2 Complete.
*   **The Bug:** The Visualizer fails to show mappings for specific Device Aliases.
*   **The Cause:** `InputProcessor` compiles mappings into `HardwareID` keys for performance. The Visualizer queries using `AliasHash` keys. The map lookup fails because an Alias Hash != a Hardware ID.
*   **Phase Goal:** Implement a secondary `configMap` in `InputProcessor` that stores the raw Alias-based mappings specifically for the Visualizer/UI to query.

**Strict Constraints:**
1.  **Separation:** `processEvent` (Audio) MUST continue using the Hardware-based map (renamed to `compiledMap` for clarity).
2.  **Visualization:** `simulateInput` (UI) MUST use the new `configMap` (Alias-based).
3.  **Synchronization:** `rebuildMapFromTree` MUST populate both maps.

---

### Step 1: Update `InputProcessor.h`
Refactor member variables.

**Members:**
*   **Rename** `keyMapping` to `compiledMap`. (Key: HardwareID).
*   **Add** `std::unordered_map<InputID, MidiAction> configMap;` (Key: AliasHash).

### Step 2: Update `InputProcessor.cpp`
Populate both maps.

**1. Refactor `rebuildMapFromTree`:**
```cpp
void InputProcessor::rebuildMapFromTree()
{
    juce::ScopedWriteLock lock(mapLock);
    compiledMap.clear();
    configMap.clear(); // Clear the UI map too

    auto mappingsNode = presetManager.getMappingsNode();
    for (int i = 0; i < mappingsNode.getNumChildren(); ++i)
    {
        addMappingToMaps(mappingsNode.getChild(i));
    }
}
```

**2. Update `addMappingFromTree` (Helper logic):**
*   Parse `inputKey`, `aliasHash` (from "deviceHash" property), etc.
*   Create `Action`.
*   **Populate Config Map (One Entry per Mapping):**
    *   `configMap[{aliasHash, inputKey}] = action;`
*   **Populate Compiled Map (Many Entries per Mapping):**
    *   Get Hardware IDs for this Alias from `deviceManager`.
    *   For each `hwID`: `compiledMap[{hwID, inputKey}] = action;`
    *   *Note:* If Alias is "Master/Any" (0), you might handle it by not adding to `compiledMap` here, relying on the Fallback logic in `processEvent`, OR add it to a specific `0` entry if `processEvent` checks `0`. (Phase 15.1 logic checks {0, key} explicitly, so we just need to ensure `compiledMap` or `configMap` handles it. Actually, `processEvent` checks Specific then Global. Global check usually looks up `{0, key}`. So `compiledMap` should contain `{0, key}` if the mapping is global).
    *   *Correction:* `configMap` definitely gets `{0, key}`. `compiledMap` gets `{0, key}` too if it's global.

**3. Refactor `processEvent` (Audio Thread):**
*   Use `compiledMap`.
*   Logic: Check `{deviceHW, key}`. If fail, check `{0, key}` (Global fallback in `compiledMap`).

**4. Refactor `simulateInput` (Visualizer Thread):**
*   Use `configMap`.
*   **Logic:**
    *   Argument `viewDeviceHash` is an **Alias Hash**.
    *   Check `configMap.find({viewDeviceHash, key})`.
    *   Check `configMap.find({0, key})` (Global).
    *   (Zone logic remains the same).

---

**Generate code for: Updated `InputProcessor.h` and Updated `InputProcessor.cpp` (Focus on `rebuildMap` and `simulateInput`).**