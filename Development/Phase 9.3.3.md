# 🤖 Cursor Prompt: Phase 9.3.3 - Safe Rename Implementation

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Renaming an Alias causes a Heap Corruption crash (`_CrtIsValidHeapPointer`).
*   **Cause:** Re-entrancy issues. `DeviceManager` iterates the ValueTree -> `setProperty` -> Synchronous Listener (`InputProcessor`) -> Reads ValueTree. Modifying a tree while iterating it (even via index) can be risky if listeners interact with the structure.
*   **Phase Goal:** Fix the crash by isolating the update logic.

**Strict Constraints:**
1.  **UI Safety:** In `DeviceSetupComponent`, the `renameAlias` call MUST be inside `juce::MessageManager::callAsync`.
2.  **Data Safety:** In `DeviceManager`, do NOT iterate `mappings.getNumChildren()` and call `setProperty` directly in the loop.
    *   First loop: Find matching children and add them to a `std::vector<juce::ValueTree>`.
    *   Second loop: Iterate the vector and call `setProperty`.

---

### Step 1: Update `Source/DeviceManager.cpp`
Refactor `renameAlias` to use the "Collect then Update" pattern.

```cpp
void DeviceManager::renameAlias(juce::String oldNameIn, juce::String newNameIn, PresetManager* presetManager)
{
    // Deep copies
    const juce::String oldName = oldNameIn;
    const juce::String newName = newNameIn;

    if (oldName == newName || newName.isEmpty()) return;

    // 1. Update the Alias Definition (Global Config)
    juce::ValueTree aliasNode;
    for (auto child : globalConfig)
    {
        if (child.getProperty("name").toString() == oldName)
        {
            aliasNode = child;
            break;
        }
    }

    if (!aliasNode.isValid()) return;

    // Calculate Hashes
    uintptr_t oldHash = MappingTypes::getAliasHash(oldName);
    uintptr_t newHash = MappingTypes::getAliasHash(newName);

    // 2. Update Mappings (Safely)
    if (presetManager)
    {
        auto mappings = presetManager->getMappingsNode();
        std::vector<juce::ValueTree> mappingsToUpdate;

        // Pass 1: Collect
        for (int i = 0; i < mappings.getNumChildren(); ++i)
        {
            auto mapping = mappings.getChild(i);
            juce::String hashStr = mapping.getProperty("deviceHash").toString();
            uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();

            if (currentHash == oldHash)
            {
                mappingsToUpdate.push_back(mapping);
            }
        }

        // Pass 2: Update (Listeners fire here, but we are no longer iterating the parent tree)
        for (auto& mapping : mappingsToUpdate)
        {
            mapping.setProperty("deviceHash", juce::String::toHexString((juce::int64)newHash), nullptr);
        }
    }

    // 3. Apply Name Change
    aliasNode.setProperty("name", newName, nullptr);
    
    // 4. Notify & Save
    sendChangeMessage();
    saveGlobalConfig();
}
```

### Step 2: Update `Source/DeviceSetupComponent.cpp`
Use Async execution to unwind the stack.

**Refactor `renameButton.onClick`:**

```cpp
    renameButton.onClick = [this] {
        auto selectedRow = aliasListBox.getSelectedRow();
        if (selectedRow < 0) return;

        // Get Name from Model (assuming model has a getter or we assume selectedRow corresponds to alias list)
        // Ideally: juce::String oldName = aliasListModel.getAliasName(selectedRow); 
        // Or if not available, grab from listbox component (less reliable).
        // Let's assume you have a way to get the name.
        juce::String oldName = deviceManager.getAliasNameAtIndex(selectedRow); // Ensure this method exists or use equivalent

        juce::AlertWindow::showAsyncMessageBox(juce::AlertWindow::QuestionIcon,
            "Rename Alias",
            "Enter new name for '" + oldName + "':",
            "Rename", "Cancel",
            [this, oldName](int result) {
                if (result == 1) {
                    // We need a TextEntry box, showAsyncMessageBox doesn't have it.
                    // You likely used showAsync with an added text editor. 
                    // Let's assume you fix the UI part.
                    // The CRITICAL part is the callback logic:
                }
            });
            
        // ... Wait, showAsyncMessageBox doesn't have text input. 
        // You probably used `AlertWindow w(...)` then `w.addTextEditor(...)`.
        // Let's stick to the architecture correction:
        
        auto w = std::make_shared<juce::AlertWindow>("Rename Alias", "Enter new name:", juce::AlertWindow::QuestionIcon);
        w->addTextEditor("name", oldName);
        w->addButton("Rename", 1);
        w->addButton("Cancel", 0);
        
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, w, oldName](int result) {
            if (result == 1)
            {
                juce::String newName = w->getTextEditorContents("name");
                
                // ASYNC CALL to prevent stack corruption
                juce::MessageManager::callAsync([this, oldName, newName]() {
                    // Safe to call now
                    deviceManager.renameAlias(oldName, newName, &presetManager);
                    
                    // Refresh UI
                    aliasListBox.updateContent();
                    aliasListBox.repaint();
                });
            }
        }));
    };
```

*(Self-Correction: Ensure `DeviceManager` has `getAliasNameAtIndex`. If not, add `getAllAliasNames()[index]` logic).*

---

**Generate code for: Updated `Source/DeviceManager.cpp` and Updated `Source/DeviceSetupComponent.cpp`.**