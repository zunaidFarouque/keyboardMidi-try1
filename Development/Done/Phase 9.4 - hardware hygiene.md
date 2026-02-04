# 🤖 Cursor Prompt: Phase 9.4 - Hardware Hygiene & Global Renaming

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 39 Complete.
*   **Issue 1 (Identity):** The UI uses "Any / Master", but sometimes creates a specific alias named "Master". We need to enforce **"Global"** as the name and **0** as the Hash.
*   **Issue 2 (Dead Hardware):** If a USB device is replugged, it gets a new Windows Handle. The old Handle remains in `DeviceManager` as a "Zombie", confusing the logic (it looks like the Alias is assigned, but it won't work).

**Phase Goal:**
1.  Rename UI labels to "Global".
2.  Implement **Hardware Hygiene**: When devices change, query the OS and remove dead IDs from `DeviceManager`.

**Strict Constraints:**
1.  **Global ID:** The string "Global" must effectively map to `uintptr_t 0`.
2.  **Win32 API:** Use `GetRawInputDeviceList` to validate handles.
3.  **Event Loop:** `RawInputManager` must intercept `WM_INPUT_DEVICE_CHANGE` (`0x00FE`) to trigger the cleanup.

---

### Step 1: Update `RawInputManager` (`Source/RawInputManager.cpp`)
Listen for plug events.

**Refactor `rawInputWndProc`:**
*   Add case `WM_INPUT_DEVICE_CHANGE`:
    *   Call `DeviceManager::getInstance()->validateConnectedDevices();` (You may need to pass the reference or use a callback/broadcaster approach if `DeviceManager` isn't global. *Recommendation: Add `std::function<void()> onDeviceChange` callback to RawInputManager, set it from MainComponent.*)
    *   Return `0` (or `DefWindowProc`).

### Step 2: Update `DeviceManager` (`Source/DeviceManager.h/cpp`)
Implement the cleanup logic.

**Method:** `void validateConnectedDevices();`
*   **Logic:**
    1.  Call `GetRawInputDeviceList` to get a vector of currently live `RAWINPUTDEVICELIST` structs.
    2.  Extract valid `hDevice` handles into a `std::set<uintptr_t> liveHandles`.
    3.  Iterate through all Aliases in `hardwareToAliasMap`.
    4.  For each Alias, iterate its Hardware IDs.
    5.  **Check:** If `id != 0` AND `id` is NOT in `liveHandles`:
        *   **Remove it.**
        *   Log: "Removed dead device [HexID] from Alias [Name]".
    6.  If changes were made, `sendChangeMessage()`.

### Step 3: UI Renaming
Update strings to "Global".

**1. `VisualizerComponent.cpp`:**
*   Update `updateViewSelector`: Change "Any / Master" to **"Global (All Devices)"**. Ensure ID pushes `0`.

**2. `MappingInspector.cpp`:**
*   Update `aliasSelector`: Add "Global (All Devices)" as item 1 (ID 1).
*   **Logic:** When "Global" is selected, ensure we write `deviceHash` property as **"0"** (String "0"), NOT the hash of the word "Global".

**3. `DeviceSetupComponent.cpp`:**
*   Prevent the user from creating/renaming an alias to "Global".

### Step 4: Wire Up `MainComponent`
*   In Constructor: `rawInputManager->setOnDeviceChangeCallback([this] { deviceManager.validateConnectedDevices(); });`

---

**Generate code for: `RawInputManager`, `DeviceManager`, `VisualizerComponent`, `MappingInspector`, `DeviceSetupComponent`, and `MainComponent`.**