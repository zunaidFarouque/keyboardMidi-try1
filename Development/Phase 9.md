# 🤖 Cursor Prompt: Phase 9 - Device Aliases (The Optimization Strategy)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
*   **Current State:** Phase 8 Complete.
*   **Phase Goal:** Implement Device Aliases ("Main Keys" -> Hardware ID), but **optimize for zero-latency lookups**.

**Strict Constraints:**
1.  **Zero Overhead:** `processEvent` must NOT loop through aliases. It must use a direct `unordered_map` lookup (O(1)).
2.  **The "Compiler":** `InputProcessor` must rebuild its flat map whenever `DeviceManager` or `PresetManager` changes.

---

### Step 1: `DeviceManager` (`Source/DeviceManager.h/cpp`)
Manage the configuration.

**Requirements:**
1.  **Storage:** `juce::ValueTree globalConfig { "OmniKeyConfig" };`
    *   XML Structure: `<Alias name="Main Keys"><Hardware id="12345"/><Hardware id="67890"/></Alias>`
2.  **API:**
    *   `void createAlias(String name);`
    *   `void assignHardware(String alias, uintptr_t hardwareId);`
    *   `Array<uintptr_t> getHardwareForAlias(String aliasName);`
    *   `String getAliasForHardware(uintptr_t hardwareId);` (For UI display).
3.  **Persistence:** Save/Load `OmniKeyConfig.xml` to `juce::File::getSpecialLocation(userApplicationDataDirectory)`.
4.  **Broadcasting:** Inherit `juce::ChangeBroadcaster`. Broadcast when config changes.

### Step 2: `InputProcessor` (The Compiler)
Update the `rebuildMapFromTree` function to "flatten" the abstract aliases into concrete hardware IDs.

**Logic Update (`rebuildMapFromTree`):**
1.  Clear `keyMapping`.
2.  Iterate through all Mappings in `PresetManager`.
3.  For each Mapping:
    *   Get `aliasName` (String) from the mapping.
    *   Call `deviceManager.getHardwareForAlias(aliasName)` -> Returns list of IDs (e.g., `[ID_A, ID_B]`).
    *   **The Loop:** For each `hardwareID` in that list:
        *   Create `InputID { hardwareID, key }`.
        *   Insert `Action` into `keyMapping`.
4.  *Result:* The map now contains direct links for every connected device. `processEvent` remains O(1).

**Listener Updates:**
*   Inherit `juce::ChangeListener`.
*   Register with `DeviceManager`.
*   In `changeListenerCallback`: Call `rebuildMapFromTree()` (because if hardware assignment changes, the compiled map is stale).

### Step 3: `MappingEditorComponent`
Update the UI to work with Aliases.

1.  **Learn Mode:**
    *   When key pressed: Call `deviceManager.getAliasForHardware(id)`.
    *   Save the **Alias Name** to the ValueTree (property `inputAlias`), NOT the Hardware ID.
    *   If hardware has no alias, show alert or default to "Unassigned".
2.  **Display:** Show the Alias Name in the "Device" column.

### Step 4: `DeviceSetupComponent`
The UI to manage the Rig.

**UI Elements:**
1.  **Left Panel:** List of Aliases ("Main Keys", "Pad", "Global").
2.  **Right Panel:** List of Connected Hardware IDs assigned to selected Alias.
3.  **"Scan/Add" Button:** Press a key on a controller -> Adds its ID to the selected Alias.
    *   *Note:* Trigger `deviceManager.sendChangeMessage()` to force `InputProcessor` to recompile.

### Step 5: Integration
1.  Update `MainComponent` to hold `DeviceManager`.
2.  Pass `DeviceManager` to `InputProcessor` and Editors.
3.  Load `DeviceManager` config on startup **before** loading the Preset.

---

**Generate code for: `DeviceManager`, `DeviceSetupComponent`, updated `InputProcessor`, updated `MappingEditorComponent`, and `MainComponent`.**