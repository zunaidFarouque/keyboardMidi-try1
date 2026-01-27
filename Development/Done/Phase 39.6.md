# 🤖 Cursor Prompt: Phase 39.6 - Fix Visualizer Color Lookup Consistency

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Visualizer correctly shows Note Names for "Any / Master" zones (Hash 0), but fails to show their Colors.
*   **Diagnosis:** `simulateInput` uses `ZoneManager::zoneLookupTable` (Optimized Map), while `getZoneColorForKey` likely iterates the vector. They are out of sync regarding how they handle the "0" Device Hash.
*   **Goal:** Refactor `getZoneColorForKey` to use the `zoneLookupTable` for 100% consistency.

**Strict Constraints:**
1.  **Logic:**
    *   Construct `InputID { aliasHash, keyCode }`.
    *   Acquire `ScopedReadLock` on `zoneLock`.
    *   Look up in `zoneLookupTable`.
    *   If found: Return `zone->zoneColor`.
    *   If not found: Return `std::nullopt`.

---

### Step 1: Update `Source/ZoneManager.cpp`

**Refactor `getZoneColorForKey`:**

```cpp
std::optional<juce::Colour> ZoneManager::getZoneColorForKey(int keyCode, uintptr_t aliasHash)
{
    // Use the exact same lookup logic as handleInput()
    // This ensures that if it plays, it paints.
    
    juce::ScopedReadLock sl(zoneLock);
    
    InputID id { aliasHash, keyCode };
    
    auto it = zoneLookupTable.find(id);
    if (it != zoneLookupTable.end())
    {
        // Found the active zone for this key/device
        if (it->second)
            return it->second->zoneColor;
    }

    return std::nullopt;
}
```

---

**Generate code for: Updated `Source/ZoneManager.cpp`.**