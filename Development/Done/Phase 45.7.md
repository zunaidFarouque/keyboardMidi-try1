# 🤖 Cursor Prompt: Phase 45.7 - Smart Alias Selection & Debugging

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 45.6 Complete.
*   **The Bug:** When "Learning" with a device mapped to a specific Alias (e.g., "Laptop"), the system sometimes defaults to "Global" or fails to switch from an existing Alias.
*   **Hypothesis:** The hardware might be mapped to multiple aliases (including Global), and the logic picks the first one (Global). Or `getAliasesForHardware` is failing silently.

**Phase Goal:**
1.  Update `MappingEditorComponent` to prioritize **Non-Zero (Specific)** aliases over Global ones found in the list.
2.  Add verbose `DBG` logging to the Learn callback to trace the lookup process.

**Strict Constraints:**
1.  **Priority:** If `aliases` contains `[0, 999]`, pick `999`.
2.  **Non-Destructive:** If `aliases` is empty, do NOT change the device hash (preserve existing).

---

### Step 1: Update `Source/MappingEditorComponent.cpp`

**Refactor `handleRawKeyEvent`:**

```cpp
void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle, int key, bool isDown)
{
    // ... (Checks for Toggle Key, Learn State, isDown) ...

    juce::MessageManager::callAsync([this, deviceHandle, key]()
    {
        // Re-check state
        if (!learnButton.getToggleState()) return;
        int selectedRow = table.getSelectedRow();
        if (selectedRow == -1) return;

        // DBG LOGGING
        DBG("Learn: Key " + String(key) + " on Device " + String::toHexString((int64)deviceHandle));

        auto mappingsNode = presetManager.getMappingsNode(currentLayerId);
        if (selectedRow < mappingsNode.getNumChildren())
        {
            auto node = mappingsNode.getChild(selectedRow);
            
            // 1. Always Update Key
            node.setProperty("inputKey", key, nullptr);
            
            // 2. Resolve Device Alias
            if (settingsManager.isStudioMode())
            {
                auto aliases = deviceManager.getAliasesForHardware(deviceHandle);
                
                DBG("Learn: Found " + String(aliases.size()) + " aliases for this device.");

                uintptr_t bestAlias = 0;
                bool foundValid = false;

                // Priority Logic: Find first NON-ZERO alias
                for (auto alias : aliases)
                {
                    if (alias != 0)
                    {
                        bestAlias = alias;
                        foundValid = true;
                        break; // Found specific, stop looking
                    }
                }
                
                // Fallback: If no specific found, but list has Global(0), use 0.
                if (!foundValid && !aliases.empty())
                {
                    bestAlias = aliases[0]; // Likely 0
                    foundValid = true;
                }

                if (foundValid)
                {
                    DBG("Learn: Switching to Alias " + String::toHexString((int64)bestAlias));
                    node.setProperty("deviceHash", juce::String::toHexString((int64)bestAlias), nullptr);
                }
                else
                {
                    DBG("Learn: No alias found. Keeping existing.");
                }
            }
            else
            {
                // Studio Mode OFF -> Force Global
                node.setProperty("deviceHash", "0", nullptr);
            }
            
            learnButton.setToggleState(false, juce::sendNotification);
        }
    });
}
```

---

**Generate code for: Updated `Source/MappingEditorComponent.cpp`.**