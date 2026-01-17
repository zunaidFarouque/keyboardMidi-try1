# 🤖 Cursor Prompt: Phase 9.2 - Alias Display & Editing

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 9.1 Complete. The `DeviceManager` exists and maps Hardware to Aliases.
*   **Problem 1 (Display):** The `MappingEditorComponent` table still shows raw numbers (e.g., "0" or hashes) in the "Device" column instead of the readable Alias Name (e.g., "Main Keys").
*   **Problem 2 (Editing):** The `MappingInspector` allows editing Note/Channel, but offers **no way** to change which Device Alias a mapping belongs to.

**Phase Goal:**
1.  Update the Table to look up and display Alias Names.
2.  Add a ComboBox to the Inspector to allow re-assigning a mapping to a different Alias.

---

### Step 1: Update `DeviceManager`
We need helper methods to support the UI.

**Requirements:**
1.  `juce::String getAliasName(uintptr_t aliasHash)`:
    *   Look up the hash in the internal config. Return the Name.
    *   *Edge Case:* If hash is `0`, return "Any / Master".
    *   *Edge Case:* If hash not found, return "Unknown".
2.  `juce::StringArray getAllAliasNames()`:
    *   Return a list of all defined aliases (for populating a ComboBox).

### Step 2: Fix `MappingEditorComponent` (The Display)
Update `paintCell` to show names, not numbers.

**Requirements:**
1.  **Dependency:** Use the existing `DeviceManager` reference.
2.  **Logic (`paintCell` - Column 2):**
    *   Get the `deviceHash` property (which is now an Alias Hash).
    *   Call `deviceManager.getAliasName(hash)`.
    *   Draw that string.

### Step 3: Upgrade `MappingInspector` (The Editor)
Add a control to change the assigned Alias.

**Requirements:**
1.  **UI:** Add `juce::ComboBox aliasSelector;` and `juce::Label aliasLabel;`.
2.  **Constructor:**
    *   Populate `aliasSelector` using `deviceManager.getAllAliasNames()`.
    *   Add an item ID for "Any / Master" (Hash 0).
3.  **Selection Logic (`setSelection`):**
    *   Read `deviceHash` from the selected Tree.
    *   Convert Hash -> Name using `DeviceManager`.
    *   Select that name in the `aliasSelector`.
4.  **Edit Logic (`aliasSelector.onChange`):**
    *   Get the selected Name.
    *   Convert Name -> Hash (using `MappingTypes::getAliasHash`).
    *   Write to `ValueTree` property "deviceHash".
    *   **Constraint:** Use `undoManager` for the transaction.
5.  **Synchronization:**
    *   The `MappingInspector` needs to know when Aliases are created/deleted (to update its dropdown).
    *   Inherit `juce::ChangeListener`.
    *   Register with `DeviceManager`.
    *   In `changeListenerCallback`: Refresh the ComboBox items.

### Step 4: Integration
*   Ensure `MainComponent` passes the `DeviceManager` reference into `MappingInspector` (via `MappingEditorComponent`).

---

**Generate code for: Updated `DeviceManager`, Updated `MappingEditorComponent.cpp` (paintCell), Updated `MappingInspector.h/cpp`.**