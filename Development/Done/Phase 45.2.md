# 🤖 Cursor Prompt: Phase 45.2 - Fix Mapping Learn (Layers & Safety)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 45.1 Complete. Layer switching works visually.
*   **Bug 1:** "Learn" mode always updates Layer 0 (Base), even if Layer 1 is selected in the UI.
*   **Bug 2:** "Learn" mode captures the Global Toggle Key (e.g., Scroll Lock). This is dangerous because pressing Scroll Lock later would toggle the app *and* trigger a note simultaneously.

**Phase Goal:**
1.  Update `MappingEditorComponent` to use `currentLayerId` when applying learned keys.
2.  Filter out the Global Toggle Key inside the Learn callback.

**Strict Constraints:**
1.  **Dependency:** `MappingEditorComponent` needs access to `SettingsManager` to know what the current Toggle Key is. Add this dependency.
2.  **Targeting:** Ensure `presetManager.getMappingsNode(currentLayerId)` is used, NOT `getMappingsNode()` (which defaults to 0).

---

### Step 1: Update `MappingEditorComponent.h`
Add the dependency.

**Requirements:**
1.  Add member: `SettingsManager& settingsManager;`
2.  Update Constructor signature to accept `SettingsManager&`.

### Step 2: Update `MappingEditorComponent.cpp`
Fix the logic.

**1. Constructor:**
*   Initialize `settingsManager`.

**2. Refactor `handleRawKeyEvent`:**
```cpp
void MappingEditorComponent::handleRawKeyEvent(uintptr_t device, int key, bool isDown)
{
    // 1. Check if Learn is Active
    if (!learnButton.getToggleState()) return;
    
    // 2. SAFETY: Ignore the Global Toggle Key (e.g. Scroll Lock)
    if (key == settingsManager.getToggleKey()) return;

    if (!isDown) return;

    // 3. Thread Safety
    juce::MessageManager::callAsync([this, device, key]()
    {
        // Re-check state on Message Thread
        if (!learnButton.getToggleState()) return;

        int selectedRow = table.getSelectedRow();
        if (selectedRow == -1) return;

        // 4. CRITICAL FIX: Get the node for the CURRENT LAYER
        auto mappingsNode = presetManager.getMappingsNode(currentLayerId); 
        
        if (selectedRow < mappingsNode.getNumChildren())
        {
            auto node = mappingsNode.getChild(selectedRow);
            
            // 5. Update ValueTree (Undoable transaction ideally, or direct)
            // Use KeyNameUtilities for display name generation if stored, 
            // but we switched to dynamic display in Phase 36, so just store RAW values.
            
            node.setProperty("inputKey", key, nullptr);
            
            // Check if we should bind specific device or keep as "Any"
            // For now, let's bind the specific device found
            // Convert device handle to Alias Hash using DeviceManager logic?
            // Actually, MappingEditor doesn't know DeviceManager logic easily unless we ask it.
            // Let's assume we map to the "Global" alias (0) OR the specific alias if found.
            
            // Better UX: Just update the KEY. Don't change the Device Alias unless the user asks.
            // Keeping the existing Device Alias is safer to avoid jumping context.
            // If the user wants to change device, they use the dropdown.
            
            // node.setProperty("deviceHash", ...); // Skip this for now to be safe/simple
            
            // 6. Finish
            learnButton.setToggleState(false, juce::sendNotification);
            // Table updates automatically via ValueTree listener
        }
    });
}
```

### Step 3: Update `MainComponent.cpp`
Pass the reference.

**Constructor:**
*   Update `mappingEditor` initialization:
    `mappingEditor = std::make_unique<MappingEditorComponent>(presetManager, deviceManager, settingsManager);`

---

**Generate code for: Updated `MappingEditorComponent.h`, Updated `MappingEditorComponent.cpp`, and Updated `MainComponent.cpp`.**