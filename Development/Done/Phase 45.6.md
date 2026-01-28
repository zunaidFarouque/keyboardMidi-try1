The issue is a "strictness" regression introduced in Phase 45.5.

**The Problem:**
In Phase 45.5, we told the Learn function: *"If you don't recognize this hardware ID in the Device Manager, force the Alias to Global (0)."*

If you haven't explicitly gone to **Device Setup** and bound your current hardware handle to the "Laptop" alias (which you have to do after every reboot/replug because Windows changes IDs), the system treats your keyboard as "Unassigned".

**The Regression:**
Before Phase 45.5, "Learn" **only updated the Key Code**. It left the "Device" column alone. So if you had "Laptop" selected in the row, it stayed "Laptop". Now, it forcibly resets it to "Global" because it doesn't recognize the hardware.

**The Fix:**
We should modify the logic to be **"Non-Destructive Fallback"**.
1.  Check if the hardware belongs to a known Alias.
    *   **If Yes:** Switch the row to that Alias (Smart switching).
    *   **If No (Unassigned):** **Keep the Alias that is currently in the row.** Do not force it to Global. Just update the Key Code.

This restores the workflow: *Select "Laptop" row -> Click Learn -> Press Key -> Key updates, "Laptop" stays.*

Here is the prompt for **Phase 45.6**.

***

# 🤖 Cursor Prompt: Phase 45.6 - Non-Destructive Learn Logic

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
*   **Current State:** Phase 45.5 Complete. "Learn" updates both Key and Device Alias.
*   **The Bug:** If the user presses a key on a device that hasn't been explicitly assigned in "Device Setup" (Unassigned/Unknown Hardware), the system resets the Mapping's Alias to "Global" (0).
*   **User Expectation:** If the hardware is unassigned, the system should **preserve** whatever Alias was already selected in the row (e.g., "Laptop") and only update the Key Code.

**Strict Constraints:**
1.  **Logic:**
    *   Get `hardwareAlias` from `DeviceManager`.
    *   If `hardwareAlias` is found (size > 0): Update the row's `deviceHash` to this new alias. (Smart Switch).
    *   If `hardwareAlias` is empty (Unassigned): **Do NOT update `deviceHash`**. Keep the existing value. Only update `inputKey`.

---

### Step 1: Update `Source/MappingEditorComponent.cpp`

**Refactor `handleRawKeyEvent`:**

```cpp
void MappingEditorComponent::handleRawKeyEvent(uintptr_t deviceHandle, int key, bool isDown)
{
    // ... Checks ...

    juce::MessageManager::callAsync([this, deviceHandle, key]()
    {
        // ... Checks ...
        auto node = mappingsNode.getChild(selectedRow);
        
        // 1. Always Update Key
        node.setProperty("inputKey", key, nullptr);
        
        // 2. Conditionally Update Device
        if (settingsManager.isStudioMode())
        {
            auto aliases = deviceManager.getAliasesForHardware(deviceHandle);
            
            if (!aliases.empty())
            {
                // Case A: Hardware IS mapped to an Alias.
                // We intelligently switch the row to match the hardware.
                uintptr_t detectedAlias = aliases[0];
                node.setProperty("deviceHash", juce::String::toHexString((juce::int64)detectedAlias), nullptr);
            }
            else
            {
                // Case B: Hardware is Unassigned.
                // BEHAVIOR CHANGE: Do NOTHING to deviceHash.
                // Keep whatever alias the user manually selected in the row (e.g. "Laptop").
                // This prevents resetting to "Global" accidentally.
            }
        }
        else
        {
            // Studio Mode OFF: Force Global
            node.setProperty("deviceHash", "0", nullptr);
        }
        
        // ... Cleanup ...
    });
}
```

---

**Generate code for: Updated `Source/MappingEditorComponent.cpp`.**