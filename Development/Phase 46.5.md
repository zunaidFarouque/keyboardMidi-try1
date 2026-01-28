The issue is a logic conflict in `MappingInspector.cpp`.

**The Diagnosis:**
1.  **Reading:** The Inspector prioritizes reading a property called `inputAlias`. Since your "Learn" function (in `MappingEditorComponent`) only updates `deviceHash`, `inputAlias` is empty. The Inspector sees empty -> defaults to "Global". It ignores the valid `deviceHash`.
2.  **Writing:** When you change the dropdown in the Inspector, the current code writes the name to `inputAlias` but **writes "0" to `deviceHash`**. This effectively deletes the mapping from the Audio Engine (which listens to `deviceHash`).

**The Solution:**
We must make `deviceHash` the **Single Source of Truth** for both the Engine and the UI.
1.  **Reading:** Always read `deviceHash`, convert it to a Name using `DeviceManager`.
2.  **Writing:** Calculate the Hash from the selected Name, and write that Hash to `deviceHash`.

Here is the prompt for **Phase 46.5**.

***

# 🤖 Cursor Prompt: Phase 46.5 - Fix Mapping Inspector Alias Logic

**Role:** Expert C++ Audio Developer (JUCE UI Debugging).

**Context:**
*   **Current State:** Phase 46.4 Complete.
*   **The Bug:**
    1.  `MappingInspector` defaults to "Global" because it prioritizes an empty `inputAlias` property over the valid `deviceHash`.
    2.  Changing the Alias in `MappingInspector` breaks the mapping because it writes `deviceHash = 0` instead of the actual alias hash.
*   **Phase Goal:** Make `deviceHash` the authoritative property. Remove reliance on `inputAlias` for logic.

**Strict Constraints:**
1.  **Read Logic (`updateControlsFromSelection`):**
    *   Read `deviceHash` (hex string).
    *   Convert to `uintptr_t`.
    *   Call `deviceManager.getAliasName(hash)` to get the text.
    *   Select that text in `aliasSelector`.
2.  **Write Logic (`aliasSelector.onChange`):**
    *   Get text from Selector.
    *   Calculate Hash using `MappingTypes::getAliasHash(text)`. (Or `0` if "Global").
    *   Write Hash to `deviceHash` property (as hex string).
    *   (Optional) You can write `inputAlias` string too for readability, but it must not be the primary read source.

---

### Step 1: Update `Source/MappingInspector.cpp`

**Refactor `updateControlsFromSelection` (Reading):**
Replace the alias section with:

```cpp
    // Update Alias Selector
    {
        juce::String aliasName = "Global (All Devices)"; // Default
        
        // Source of Truth: deviceHash
        if (allTreesHaveSameValue("deviceHash"))
        {
            juce::String hashStr = getCommonValue("deviceHash").toString();
            uintptr_t hash = (uintptr_t)hashStr.getHexValue64();
            
            if (hash != 0)
                aliasName = deviceManager.getAliasName(hash);
        }
        else
        {
             // Mixed selection logic
             aliasSelector.setSelectedId(-1, juce::dontSendNotification);
             return; // Or skip setting selection
        }

        // Set by Text
        bool found = false;
        for (int i = 0; i < aliasSelector.getNumItems(); ++i) {
            if (aliasSelector.getItemText(i) == aliasName) {
                aliasSelector.setSelectedItemIndex(i, juce::dontSendNotification);
                found = true;
                break;
            }
        }
        if (!found) aliasSelector.setSelectedItemIndex(0, juce::dontSendNotification); // Default to Global
    }
```

**Refactor `aliasSelector.onChange` (Writing):**

```cpp
    aliasSelector.onChange = [this] {
        if (selectedTrees.empty()) return;
        if (aliasSelector.getSelectedId() == -1) return;

        juce::String aliasName = aliasSelector.getText();
        
        // Calculate Hash
        uintptr_t newHash = 0;
        if (aliasName != "Global (All Devices)")
        {
            newHash = MappingTypes::getAliasHash(aliasName);
        }

        undoManager.beginNewTransaction("Change Alias");
        for (auto& tree : selectedTrees) {
            if (tree.isValid()) {
                // Write the Hash (Engine needs this)
                tree.setProperty("deviceHash", juce::String::toHexString((int64)newHash), &undoManager);
                
                // Write the Name (Optional, for debugging XML)
                tree.setProperty("inputAlias", aliasName, &undoManager);
            }
        }
        undoManager.beginNewTransaction(); // Close/Commit
    };
```

---

**Generate code for: Updated `Source/MappingInspector.cpp`.**