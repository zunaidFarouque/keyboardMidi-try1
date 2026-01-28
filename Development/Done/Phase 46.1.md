The "Master Input" persisting and eating your devices means we failed to fully kill the "Auto-Add" logic in the constructor or initialization phase.

Since you deleted AppData and it *still* happened, it means the code is explicitly creating "Master Input" and assigning devices to it on a fresh start.

Here is the plan to **Kill "Master Input"** for good and ensure Unassigned devices actually stay Unassigned.

### The Fix
1.  **Constructor Cleanup:** Remove the lines in `DeviceManager::DeviceManager()` or `init` that create "Master Input".
2.  **Validation Logic:** Ensure `validateConnectedDevices` puts *new* devices into `unassignedDevices`, not into the first available alias.

Here is the prompt for **Phase 46.1**.

***

# 🤖 Cursor Prompt: Phase 46.1 - Remove Default "Master Input" Alias

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** Phase 46 Complete.
*   **The Bug:** On a fresh install (no AppData), the app still creates an alias named "Master Input" and automatically assigns detected hardware to it.
*   **Goal:**
    1.  **Stop Default Creation:** `DeviceManager` should start with **Zero Aliases**.
    2.  **Stop Auto-Assign:** Freshly detected hardware must go to `unassignedDevices`, never to an alias.
    3.  **UI:** The "Master Input" item should disappear from the "Rig Configuration" window (unless the user manually creates it).

**Strict Constraints:**
1.  **Constructor:** Remove any `createAlias("Master Input")` calls.
2.  **Sanitization:** In `validateConnectedDevices`, if a device is found in the OS list but NOT in `hardwareToAliasMap`, it **MUST** go to `unassignedDevices`.

---

### Step 1: Update `Source/DeviceManager.cpp`

**1. Refactor Constructor:**
*   **Remove** any logic that checks `if (rootNode.getNumChildren() == 0)` and adds "Master Input".
*   Leave the `rootNode` empty on fresh start.

**2. Refactor `validateConnectedDevices`:**
*   Ensure the logic is strictly:
    ```cpp
    // ... inside loop over Live OS Devices ...
    bool isAssigned = false;
    
    // Check if this hardwareID exists in ANY alias
    // (You might need a reverse lookup cache or iterate all aliases)
    for (auto& entry : hardwareToAliasMap) {
        // Entry value is vector<uintptr_t>
        for (auto id : entry.second) {
            if (id == deviceHandle) {
                isAssigned = true; 
                break; 
            }
        }
        if (isAssigned) break;
    }

    if (!isAssigned)
    {
        // PURE UNASSIGNED. Do not create alias.
        unassignedDevices.push_back(deviceHandle);
    }
    ```

**3. Check `loadGlobalConfig` / `restoreFromValueTree`:**
*   Ensure there is no fallback logic adding defaults there either.

---

**Generate code for: Updated `DeviceManager.cpp` (Constructor & Validate logic).**