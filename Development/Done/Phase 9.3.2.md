# 🤖 Cursor Prompt: Phase 9.3.2 - Rename Crash Fix & Logic Hardening

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Renaming an Alias causes a Heap Corruption Crash.
*   **Diagnosis 1 (Memory):** String references might be invalidated during ValueTree updates.
*   **Diagnosis 2 (Logic):** `InputProcessor`'s `valueTreePropertyChanged` uses a broken "Remove/Add" pattern. Since the Tree already holds the *New* value, `remove` fails to find the *Old* key in the map, causing map corruption/zombies.

**Phase Goal:**
1.  Harden `DeviceManager::renameAlias` to use isolated string copies.
2.  Fix `InputProcessor` to perform a full `rebuildMapFromTree` on property changes to ensure state integrity.

**Strict Constraints:**
1.  **DeviceManager:** Use `juce::String(oldName).detachFromOtherStringStorage()` or simply construct new strings to ensure deep copies before touching the Tree.
2.  **InputProcessor:** In `valueTreePropertyChanged`, if the change is relevant to mappings, call `rebuildMapFromTree()` instead of incremental updates.

---

### Step 1: Update `Source/DeviceManager.cpp`
Refactor `renameAlias` for safety.

```cpp
void DeviceManager::renameAlias(juce::String oldNameIn, juce::String newNameIn, PresetManager* presetManager)
{
    // 1. Force Deep Copies to isolate from ValueTree memory
    const juce::String oldName = oldNameIn;
    const juce::String newName = newNameIn;

    if (oldName == newName || newName.isEmpty()) return;

    // 2. Find Node
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

    // 3. Calculate Hashes
    uintptr_t oldHash = MappingTypes::getAliasHash(oldName);
    uintptr_t newHash = MappingTypes::getAliasHash(newName);

    // 4. Update Mappings
    if (presetManager)
    {
        auto mappings = presetManager->getMappingsNode();
        for (int i = 0; i < mappings.getNumChildren(); ++i)
        {
            auto mapping = mappings.getChild(i);
            
            // Safe Hex Parsing
            juce::String hashStr = mapping.getProperty("deviceHash").toString();
            uintptr_t currentHash = (uintptr_t)hashStr.getHexValue64();

            if (currentHash == oldHash)
            {
                // Update to new hash
                mapping.setProperty("deviceHash", juce::String::toHexString((juce::int64)newHash), nullptr);
            }
        }
    }

    // 5. Update Alias
    aliasNode.setProperty("name", newName, nullptr);
    
    // 6. Notify
    sendChangeMessage();
    saveGlobalConfig();
}
```

### Step 2: Update `Source/InputProcessor.cpp`
Fix the Map Corruption logic.

**Refactor `valueTreePropertyChanged`:**

```cpp
void InputProcessor::valueTreePropertyChanged(juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property)
{
    auto mappingsNode = presetManager.getMappingsNode();
    auto parent = treeWhosePropertyHasChanged.getParent();
    
    // Optimization: Only rebuild if relevant properties changed
    if (property == juce::Identifier("inputKey") || 
        property == juce::Identifier("deviceHash") ||
        property == juce::Identifier("type") ||
        property == juce::Identifier("channel") ||
        property == juce::Identifier("data1") ||
        property == juce::Identifier("data2") ||
        property == juce::Identifier("velRandom") ||
        property == juce::Identifier("adsrTarget") ||
        property == juce::Identifier("pbRange") ||
        property == juce::Identifier("pbShift") ||
        property == juce::Identifier("adsrAttack") || 
        property == juce::Identifier("adsrDecay") ||
        property == juce::Identifier("adsrSustain") ||
        property == juce::Identifier("adsrRelease"))
    {
        if (parent.isEquivalentTo(mappingsNode) || treeWhosePropertyHasChanged.isEquivalentTo(mappingsNode))
        {
            // SAFER: Rebuild everything. 
            // The logic to "Remove Old -> Add New" is impossible here because 
            // we don't know the Old values anymore (the Tree is already updated).
            rebuildMapFromTree();
        }
    }
}
```

---

**Generate code for: Updated `Source/DeviceManager.cpp` and Updated `Source/InputProcessor.cpp`.**