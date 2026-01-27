# 🤖 Cursor Prompt: Phase 39.4 - Fix InputID Hashing & Logic Verification

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Phase 39.3 Complete. Dual Maps (`compiledMap`, `configMap`) are implemented.
*   **The Bug:** Visualizer lookups are failing. "Master" view is empty even when mappings exist. "Specific" view shows inherited mappings but not its own.
*   **Diagnosis:** The `std::hash<InputID>` implementation is likely weak, causing collisions between `{0, Key}` and `{Alias, Key}`. This causes map entries to overwrite each other.

**Phase Goal:**
1.  Implement a robust **Hash Combine** function for `InputID`.
2.  Review and harden `InputProcessor::simulateInput` to ensure it correctly prioritizes Specific -> Global.

**Strict Constraints:**
1.  **Hashing:** Use a proven hash combination algorithm (like `boost::hash_combine` logic: `seed ^ (hash + 0x9e3779b9 + (seed<<6) + (seed>>2))`).
2.  **Equality:** Ensure `operator==` for `InputID` checks both `deviceHandle` and `keyCode`.
3.  **Simulation:** Ensure `simulateInput` checks the maps in the correct order.

---

### Step 1: Update `MappingTypes.h`
Fix the struct and hash.

```cpp
struct InputID
{
    uintptr_t deviceHandle;
    int keyCode;

    bool operator==(const InputID& other) const
    {
        return deviceHandle == other.deviceHandle && keyCode == other.keyCode;
    }
};

// Inject Custom Hash into std namespace
namespace std
{
    template <>
    struct hash<InputID>
    {
        std::size_t operator()(const InputID& k) const
        {
            // Robust Hash Combine
            std::size_t h1 = std::hash<uintptr_t>{}(k.deviceHandle);
            std::size_t h2 = std::hash<int>{}(k.keyCode);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };
}
```

### Step 2: Verify `InputProcessor.cpp`
Check the `simulateInput` logic.

**Refactor `simulateInput`:**
*   Ensure it uses `configMap`.
*   **Sequence:**
    1.  **Specific Manual:** `configMap.find({viewDeviceHash, key})`. (Only if `viewDeviceHash != 0`).
    2.  **Global Manual:** `configMap.find({0, key})`.
    3.  **Specific Zone:** `zoneManager.handleInput({viewDeviceHash, key})`.
    4.  **Global Zone:** `zoneManager.handleInput({0, key})`.
*   **State Assignment:**
    *   If `Specific Manual`: State = `Active` (or `Override` if Global exists).
    *   If `Global Manual`: State = `Inherited` (if View != 0), else `Active`.
    *   Same logic for Zones.

---

**Generate code for: Updated `MappingTypes.h` and Updated `InputProcessor.cpp`.**