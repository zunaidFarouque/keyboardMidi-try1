# 🤖 Cursor Prompt: Phase 9.3.1 - Fix Rename Crash & Logic

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 9.3 Complete.
*   **The Bug:** Renaming an Alias causes a **Heap Corruption Crash** (CRT Debug Error).
*   **Likely Cause:**
    1.  **Memory:** Modifying the `name` property invalidates string references passed to listeners/callbacks.
    2.  **Logic:** Renaming an Alias changes its `getAliasHash()`. Existing Mappings in `PresetManager` still hold the *Old Hash*, effectively orphaning them.

**Phase Goal:**
1.  Implement a safe `renameAlias` method in `DeviceManager` that prevents memory corruption.
2.  Ensure that when an Alias is renamed, **all Mappings using that Alias are automatically updated** to the new Hash.

**Strict Constraints:**
1.  **String Safety:** In `renameAlias`, strictly make local **copies** of `oldName` and `newName` before touching the ValueTree. Do not rely on references.
2.  **Update Mappings:** `DeviceManager` needs a reference to `PresetManager` (or we handle this update in `MainComponent` via a callback).
    *   *Better Approach:* Add a method `DeviceManager::renameAlias(oldName, newName, PresetManager* optionalPresetMgr)`.
3.  **Atomic Update:** The renaming and the mapping updates should happen in one block to prevent the engine from seeing a broken state (where Alias is new but Mapping is old).

---

### Step 1: Update `DeviceManager` (`Source/DeviceManager.cpp`)

Refactor `renameAlias` to be robust.

**Logic:**
```cpp
void DeviceManager::renameAlias(juce::String oldName, juce::String newName, PresetManager* presetManager)
{
    // 1. Sanity Check
    if (oldName == newName || newName.isEmpty()) return;

    // 2. Find the Alias Node
    // Iterate carefully
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

    // 3. Calculate Hashes (Use MappingTypes helper)
    uintptr_t oldHash = MappingTypes::getAliasHash(oldName);
    uintptr_t newHash = MappingTypes::getAliasHash(newName);

    // 4. Update Mappings FIRST (if manager provided)
    // We update the consumer (Mappings) to point to the new ID before we change the ID.
    // Actually, order doesn't matter much if single threaded, but let's do it cleanly.
    if (presetManager)
    {
        auto mappings = presetManager->getMappingsNode();
        for (int i = 0; i < mappings.getNumChildren(); ++i)
        {
            auto mapping = mappings.getChild(i);
            // Check if this mapping uses the old alias hash
            // (Note: deviceHash stores the Alias Hash in this architecture)
            // Parse correctly using the hex/string logic from Phase 9
            juce::String hashStr = mapping.getProperty("deviceHash").toString();
            uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();

            if (currentHash == oldHash)
            {
                // Update to new hash
                mapping.setProperty("deviceHash", String::toHexString((int64)newHash), nullptr);
            }
        }
    }

    // 5. Update the Alias Definition
    // This will trigger listeners (UI update).
    aliasNode.setProperty("name", newName, nullptr);
    
    // 6. Force Config Save
    sendChangeMessage(); 
    // saveGlobalConfig(); // Optional, immediate save
}
```

### Step 2: Update `DeviceSetupComponent` (`Source/DeviceSetupComponent.cpp`)

Update the call site.

**Refactor `renameButton.onClick`:**
*   Inside the AlertWindow callback:
*   Call `deviceManager.renameAlias(oldName, newName, &presetManager);`
    *   *Note:* You will need to pass `PresetManager` to `DeviceSetupComponent` constructor now.

### Step 3: Update `MainComponent`
*   Pass `presetManager` to `DeviceSetupComponent` constructor when creating the window.

---

**Generate code for: Updated `DeviceManager.h/cpp` (Signature & Logic), Updated `DeviceSetupComponent.h/cpp` (Constructor & Call site), and `MainComponent.cpp`.**