# 🤖 Cursor Prompt: Phase 45.5 - Learn Device Alias (Studio Mode Aware)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 45.3 Complete.
*   **The Bug:** When using "Learn" in the Mapping Editor, it updates the Key Code (`inputKey`) but ignores the source device. The mapping defaults to whatever device was there before (usually "Global"), even if the user pressed a specific keyboard (e.g., "Laptop").
*   **Phase Goal:** Update the Learn logic to capture the **Device Alias** corresponding to the hardware that triggered the event.

**Strict Constraints:**
1.  **Studio Mode Logic:**
    *   If `SettingsManager::isStudioMode()` is **OFF**: Always map to **Global** (Hash 0).
    *   If `SettingsManager::isStudioMode()` is **ON**: Query `DeviceManager` for the Alias associated with the hardware ID.
2.  **Alias Priority:** If a hardware ID belongs to an Alias, use that Alias Hash. If not found (unassigned), fallback to Global (0).
3.  **Data Persistence:** Update the `deviceHash` property in the `ValueTree` using the Hex String format (`String::toHexString(hash)`).

---

### Step 1: Update `Source/MappingEditorComponent.cpp`

**Refactor `handleRawKeyEvent`:**

Currently, it only updates `inputKey`. We need to calculate and update `deviceHash` too.

```cpp
void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle, int key, bool isDown)
{
    // 1. Checks (Toggle state, Global Toggle Key protection)
    if (!learnButton.getToggleState()) return;
    if (key == settingsManager.getToggleKey()) return;
    if (!isDown) return;

    // 2. Thread Safety
    juce::MessageManager::callAsync([this, deviceHandle, key]()
    {
        if (!learnButton.getToggleState()) return;

        int selectedRow = table.getSelectedRow();
        if (selectedRow == -1) return;

        auto mappingsNode = presetManager.getMappingsNode(currentLayerId);
        
        if (selectedRow < mappingsNode.getNumChildren())
        {
            auto node = mappingsNode.getChild(selectedRow);
            
            // --- NEW LOGIC: Determine Alias ---
            uintptr_t targetAlias = 0; // Default Global

            if (settingsManager.isStudioMode())
            {
                // Ask DeviceManager: "Who is this hardware?"
                // getAliasesForHardware returns a vector. We take the first one if available.
                // Note: We need access to DeviceManager. 
                // Ensure 'deviceManager' member exists (it should from Phase 33/34).
                
                auto aliases = deviceManager.getAliasesForHardware(deviceHandle);
                if (!aliases.empty())
                {
                    targetAlias = aliases[0];
                }
            }
            
            // 3. Update Properties
            // Update Key
            node.setProperty("inputKey", key, nullptr);
            
            // Update Device Alias
            node.setProperty("deviceHash", juce::String::toHexString((juce::int64)targetAlias), nullptr);
            
            // 4. Cleanup
            learnButton.setToggleState(false, juce::sendNotification);
        }
    });
}
```

---

**Generate code for: Updated `MappingEditorComponent.cpp`.**