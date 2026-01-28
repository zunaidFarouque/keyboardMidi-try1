# 🤖 Cursor Prompt: Phase 46 - Device Manager Hygiene & Unassigned View

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 45 Complete.
*   **The Bug:** `DeviceManager` automatically assigns any new/unknown USB device to the "Master Input" alias. This fills the Global list with "Zombie" devices (phantom IDs like `FE869`) that reappear even after removal.
*   **Phase Goal:**
    1.  **Stop Auto-Add:** `DeviceManager` must NOT assign new hardware to any alias automatically.
    2.  **Track Unassigned:** Keep a list of connected but unassigned devices.
    3.  **UI:** Update `DeviceSetupComponent` to show a special "**[ Unassigned Devices ]**" entry at the top of the Alias list.

**Strict Constraints:**
1.  **Logic:** In `validateConnectedDevices`, if a live device ID is not found in any existing Alias, add it to a `std::vector<uintptr_t> unassignedDevices`. Do NOT add it to the ValueTree.
2.  **UI:**
    *   The Alias List should always show `[ Unassigned Devices ]` as **Row 0**.
    *   Real Aliases start at **Row 1**.
    *   "Delete Alias" and "Rename Alias" must be **Disabled** for Row 0.
3.  **Master Input:** If "Master Input" exists in the XML, treat it as a normal user-defined alias. Do not give it special auto-add privileges.

---

### Step 1: Update `DeviceManager` (`Source/DeviceManager.h/cpp`)
Refactor the validation logic.

**1. Header:**
*   Add `std::vector<uintptr_t> unassignedDevices;`
*   Add `const std::vector<uintptr_t>& getUnassignedDevices() const;`

**2. Update `validateConnectedDevices`:**
*   Clear `unassignedDevices`.
*   Get Live Device List from OS.
*   Loop through Live Devices:
    *   Check if ID exists in `hardwareToAliasMap`.
    *   **If Yes:** Keep it (and ensure it's in the Map).
    *   **If No:** Add to `unassignedDevices`. (Do NOT write to ValueTree).
*   *Cleanup:* Loop through `hardwareToAliasMap`. If an ID in the map is NOT in the Live List, remove it from the ValueTree (Dead device cleanup).

### Step 2: Update `DeviceSetupComponent.cpp`
Implement the Special Row.

**1. `AliasListModel`:**
*   **`getNumRows`:** Return `deviceManager.getNumAliases() + 1`. (Plus 1 for Unassigned).
*   **`paintListBoxItem`:**
    *   If `rowNumber == 0`: Draw text `"[ Unassigned Devices ]"` (Maybe in Orange/Italic).
    *   If `rowNumber > 0`: Draw alias name for index `rowNumber - 1`.

**2. `HardwareListModel`:**
*   **Update:**
    *   If `selectedAliasIndex == 0`: Use `deviceManager.getUnassignedDevices()`.
    *   If `selectedAliasIndex > 0`: Use `deviceManager.getHardwareForAlias(aliasName)`.

**3. `DeviceSetupComponent` Interactions:**
*   **Selection Change:** Handle the `row - 1` offset logic.
*   **Buttons:**
    *   Disable `renameButton` and `deleteAliasButton` if `aliasList.getSelectedRow() == 0`.
    *   `scanAddButton` (Assign): If used on Row 0, it doesn't make sense (you can't assign TO unassigned). Maybe hide it, or change behavior to "Create New Alias from Selection"? *Decision: Disable Scan/Add on Row 0.*
    *   **New Feature:** Allow dragging/moving? For now, just disabling the modification buttons on Row 0 is sufficient. The user creates a *New* alias, then scans to add to *that*.

---

**Generate code for: Updated `DeviceManager.h`, Updated `DeviceManager.cpp`, and Updated `DeviceSetupComponent.cpp`.**