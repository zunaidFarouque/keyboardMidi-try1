# 🤖 Cursor Prompt: Phase 46.3 - Fix Alias Reverse Lookup

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Phase 46.2 Complete.
*   **The Bug:**
    1.  Mapping Table shows "Unknown" in the Device column for aliases.
    2.  Mapping Inspector defaults to "Global" because it can't match the alias hash.
*   **Diagnosis:** `DeviceManager::getAliasName(hash)` is failing to resolve the hash back to the string name. Likely due to integer casting issues or lack of a robust lookup map.

**Phase Goal:** Implement a `aliasIdToName` cache in `DeviceManager` to ensure 100% reliable reverse lookups.

**Strict Constraints:**
1.  **Cache:** `std::map<uintptr_t, juce::String> aliasNameCache;`
2.  **Sync:** Rebuild this cache whenever aliases are created, renamed, or loaded.
3.  **Lookup:** `getAliasName` must look in this map. If not found, return "Unknown".

---

### Step 1: Update `Source/DeviceManager.h`
Add the cache member.

```cpp
private:
    std::map<uintptr_t, juce::String> aliasNameCache;
    void rebuildAliasCache(); // Helper
```

### Step 2: Update `Source/DeviceManager.cpp`
Implement the cache logic.

**1. Implement `rebuildAliasCache()`:**
```cpp
void DeviceManager::rebuildAliasCache()
{
    aliasNameCache.clear();
    
    // Always add Global
    aliasNameCache[0] = "Global (All Devices)";

    // Iterate Root Node Children (Aliases)
    for (const auto& child : rootNode)
    {
        if (child.hasType("Alias"))
        {
            juce::String name = child.getProperty("name");
            uintptr_t hash = MappingTypes::getAliasHash(name);
            aliasNameCache[hash] = name;
        }
    }
}
```

**2. Update Triggers:**
*   Call `rebuildAliasCache()` in:
    *   `Constructor` (at the end).
    *   `loadGlobalConfig`.
    *   `createAlias`.
    *   `renameAlias`.
    *   `deleteAlias` (if implemented).

**3. Refactor `getAliasName(uintptr_t hash)`:**
```cpp
juce::String DeviceManager::getAliasName(uintptr_t hash)
{
    auto it = aliasNameCache.find(hash);
    if (it != aliasNameCache.end())
        return it->second;
        
    return "Unknown";
}
```

### Step 3: Verify `MappingInspector.cpp`
Ensure the dropdown selection uses the same math.

**Update `setSelection`:**
*   Get `deviceHash` string from XML.
*   Convert to `uintptr_t` using `.getHexValue64()`.
*   Iterate `aliasSelector` items.
*   The ID of the items in `aliasSelector` usually stores the index, not the hash.
*   **Fix:** Match by **Text**.
    *   Call `deviceManager.getAliasName(hash)`.
    *   Set Loop: `if (box.getItemText(i) == name) box.setSelectedItemIndex(i);`

---

**Generate code for: Updated `DeviceManager.h`, Updated `DeviceManager.cpp`, and Updated `MappingInspector.cpp`.**