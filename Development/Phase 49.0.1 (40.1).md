# 🤖 Cursor Prompt: Phase 41.1 - Safe Loading (Fix Startup Hang)

**Role:** Expert C++ Audio Developer (Debugging & Optimization).

**Context:**
*   **Current State:** Phase 41 Complete.
*   **The Bug:** The app hangs on startup if `autoload.xml` exists. Deleting the file fixes it.
*   **Diagnosis:** `loadFromFile` triggers individual ValueTree callbacks for every layer/mapping. `InputProcessor` tries to rebuild the map dozens of times during the load, likely deadlocking with the Visualizer thread or creating an event storm.

**Phase Goal:** Implement a **"Silent Load"** mechanism using a status flag.

**Strict Constraints:**
1.  **PresetManager:** Must inherit `juce::ChangeBroadcaster`.
2.  **Atomic Flag:** Use `std::atomic<bool> isLoading { false };`.
3.  **InputProcessor:** Must ignore `ValueTree` events while `isLoading` is true. It must rebuild **only** when `PresetManager` sends a `ChangeMessage` at the end of the load.

---

### Step 1: Update `PresetManager` (`Source/PresetManager.h/cpp`)
Add the suppression logic.

**1. Header:**
*   Inherit `public juce::ChangeBroadcaster`.
*   Member: `std::atomic<bool> isLoading { false };`
*   Method: `bool getIsLoading() const { return isLoading; }`

**2. Update `loadFromFile`:**
```cpp
void PresetManager::loadFromFile(juce::File file)
{
    isLoading = true; // SUSPEND LISTENERS

    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        auto newTree = juce::ValueTree::fromXml(*xml);
        if (newTree.isValid() && newTree.hasType(rootNode.getType()))
        {
            // Sync properties
            rootNode.copyPropertiesFrom(newTree, nullptr);
            
            // Sync children
            rootNode.removeAllChildren(nullptr);
            for (auto child : newTree)
            {
                rootNode.addChild(child.createCopy(), -1, nullptr);
            }
            
            // Ensure structure
            ensureStaticLayers();
        }
    }
    
    isLoading = false; // RESUME LISTENERS
    sendChangeMessage(); // TRIGGER ONE REBUILD
}
```

### Step 2: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Respect the flag.

**1. Header:**
*   Inherit `public juce::ChangeListener`.
*   Override `changeListenerCallback`.

**2. Implementation:**
*   **Constructor:** `presetManager.addChangeListener(this);`
*   **Destructor:** `presetManager.removeChangeListener(this);`
*   **Update `valueTreeChildAdded/Removed/Changed`:**
    *   Add at the top: `if (presetManager.getIsLoading()) return;`
*   **Implement `changeListenerCallback`:**
    ```cpp
    void InputProcessor::changeListenerCallback(juce::ChangeBroadcaster* source)
    {
        if (source == &presetManager)
        {
            rebuildMapFromTree();
        }
        else if (source == &deviceManager || source == &settingsManager)
        {
            // (Existing logic for other managers if needed)
            rebuildMapFromTree();
        }
    }
    ```

### Step 3: Verify `MappingEditorComponent.cpp`
Ensure the UI also respects this (to avoid table flickering/crashes during load).

*   **Update `valueTreeChildAdded...`:**
    *   `if (presetManager.getIsLoading()) return;`
*   **Update Constructor:**
    *   Register as `ChangeListener` to `presetManager`.
*   **Implement `changeListenerCallback`:**
    *   `if (source == &presetManager) table.updateContent();`

---

**Generate code for: Updated `PresetManager.h/cpp`, Updated `InputProcessor.h/cpp`, and Updated `MappingEditorComponent.h/cpp`.**