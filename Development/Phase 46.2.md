# 🤖 Cursor Prompt: Phase 46.2 - Fix Unassigned List & Remove Logic

**Role:** Expert C++ Audio Developer (JUCE UI & Logic).

**Context:**
*   **Current State:** Phase 46.1 Complete. "Master Input" is gone (Good).
*   **Bug 1:** The `[ Unassigned Devices ]` list (Row 0) is always empty, even when new devices are plugged in.
*   **Bug 2:** The "Remove" button in `DeviceSetupComponent` never enables, preventing users from un-mapping a device.

**Phase Goal:**
1.  Fix `DeviceManager::validateConnectedDevices` to correctly populate the `unassignedDevices` vector.
2.  Implement `DeviceManager::removeHardwareFromAlias`.
3.  Fix `DeviceSetupComponent` selection logic to enable the Remove button and refresh the lists correctly.

**Strict Constraints:**
1.  **Unassigned Logic:** If a device handle from `GetRawInputDeviceList` is NOT found in `hardwareToAliasMap`, it MUST be added to `unassignedDevices`.
2.  **Remove Logic:** Removing a device from an alias effectively makes it "Unassigned". The system must reflect this immediately.
3.  **UI Feedback:** Ensure `hardwareListBox.updateContent()` is called when switching aliases.

---

### Step 1: Update `Source/DeviceManager.cpp`

**Refactor `validateConnectedDevices`:**
Ensure the logic flow is watertight.

```cpp
void DeviceManager::validateConnectedDevices()
{
    // 1. Clear Unassigned
    unassignedDevices.clear();

    // 2. Get Live OS Devices
    UINT nDevices = 0;
    GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST));
    if (nDevices == 0) return;

    std::vector<RAWINPUTDEVICELIST> rawDevices(nDevices);
    GetRawInputDeviceList(rawDevices.data(), &nDevices, sizeof(RAWINPUTDEVICELIST));

    // 3. Scan & Categorize
    for (const auto& device : rawDevices)
    {
        if (device.dwType != RIM_TYPEKEYBOARD) continue;

        uintptr_t handle = (uintptr_t)device.hDevice;
        bool isAssigned = false;

        // Check if assigned to ANY alias
        // hardwareToAliasMap maps HardwareID -> Vector of AliasHashes (or similar structure)
        // Check your implementation of hardwareToAliasMap carefully here.
        // Assuming: std::map<uintptr_t, std::vector<uintptr_t>> hardwareToAliasMap;
        if (hardwareToAliasMap.find(handle) != hardwareToAliasMap.end())
        {
            isAssigned = true;
        }

        if (!isAssigned)
        {
            unassignedDevices.push_back(handle);
        }
    }

    // 4. Notify UI
    sendChangeMessage();
}
```

**Implement `removeHardwareFromAlias`:**
```cpp
void DeviceManager::removeHardwareFromAlias(const juce::String& aliasName, uintptr_t hardwareId)
{
    // 1. Find the Alias Node
    juce::ValueTree aliasNode;
    for (auto child : rootNode) // Assuming rootNode holds aliases
        if (child.getProperty("name").toString() == aliasName)
            aliasNode = child;

    if (!aliasNode.isValid()) return;

    // 2. Find and Remove the Hardware Child
    for (int i = 0; i < aliasNode.getNumChildren(); ++i)
    {
        auto hwNode = aliasNode.getChild(i);
        // Assuming hardware ID is stored as hex string
        uintptr_t storedId = (uintptr_t)hwNode.getProperty("id").toString().getHexValue64();
        
        if (storedId == hardwareId)
        {
            aliasNode.removeChild(i, nullptr);
            // Don't break, in case of duplicates? No, break is fine.
            break; 
        }
    }

    // 3. Update Internal Maps
    // (Re-running validate is the laziest/safest way to sync maps & unassigned list)
    // But we should update maps locally first if validate relies on OS query that might be slow?
    // Let's rely on rebuildMaps() -> validateConnectedDevices() chain.
    
    rebuildHardwareMap(); // You likely have a method to rebuild hardwareToAliasMap from ValueTree
    validateConnectedDevices(); // This will put the removed device into 'unassigned'
}
```

### Step 2: Update `Source/DeviceSetupComponent.cpp`

**Fix `HardwareListModel` (Data Source):**
```cpp
    // In getNumRows / paintListBoxItem
    if (owner.isUnassignedSelected()) // Helper check: selectedRow == 0
    {
        // Return unassignedDevices info
    }
    else
    {
        // Return alias hardware info
    }
```

**Fix Button State Logic (`updateButtonStates`):**
```cpp
void DeviceSetupComponent::updateButtonStates()
{
    int aliasRow = aliasListBox.getSelectedRow();
    int hwRow = hardwareListBox.getSelectedRow();

    bool isUnassigned = (aliasRow == 0);
    bool hasAlias = (aliasRow > 0);
    bool hasHwSelection = (hwRow != -1);

    // Remove Button: Only enabled if looking at a Real Alias AND a device is selected
    removeButton.setEnabled(hasAlias && hasHwSelection);
    
    // Rename/Delete Alias: Only if Real Alias
    renameButton.setEnabled(hasAlias);
    deleteAliasButton.setEnabled(hasAlias);
    
    // Add/Scan: Can we add to Unassigned? No.
    scanAddButton.setEnabled(hasAlias);
}
```

**Wire up Listeners:**
*   Ensure `hardwareListBox.onChange` calls `updateButtonStates`.
*   Ensure `aliasListBox.onChange` calls `updateButtonStates` AND `hardwareListBox.updateContent()`.

---

**Generate code for: Updated `DeviceManager.cpp` and `DeviceSetupComponent.cpp`.**