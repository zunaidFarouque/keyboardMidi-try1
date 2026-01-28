# 🤖 Cursor Prompt: Phase 46.4 - Fix Inspector Alias Selection

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
*   **Current State:** Phase 46.3 Complete.
*   **The Bug:** `MappingInspector`'s Alias ComboBox always defaults to "Global", even when the selected mapping is for a specific alias (e.g., "external").
*   **Diagnosis:** The `setSelection` method is likely trying to set the ID using the Hash (which fails due to 32/64-bit truncation) or isn't resolving the Hash to a Name first.

**Phase Goal:** Update `MappingInspector` to resolve the Alias Hash to a Name using `DeviceManager`, then select that Name in the ComboBox.

**Strict Constraints:**
1.  **Reference:** `MappingInspector` MUST hold `DeviceManager&` as a member. Update constructor to accept it.
2.  **Update `setSelection`:**
    *   Get `deviceHash` from ValueTree.
    *   Resolve to String via `deviceManager.getAliasName(hash)`.
    *   Iterate ComboBox items. If `itemText == resolvedName`, select it.
3.  **Update `MappingEditorComponent`:** Pass the `DeviceManager` to the Inspector.

---

### Step 1: Update `Source/MappingInspector.h`
Add the dependency.

```cpp
class MappingInspector : public juce::Component, public juce::ValueTree::Listener
{
public:
    // Update constructor signature
    MappingInspector(juce::UndoManager& um, DeviceManager& dm); 

private:
    DeviceManager& deviceManager; // <--- NEW MEMBER
    // ...
};
```

### Step 2: Update `Source/MappingInspector.cpp`
Implement the lookup logic.

**1. Update Constructor:**
*   Initialize `deviceManager(dm)`.
*   (Ensure `aliasSelector` is populated from `deviceManager.getAllAliasNames()` here too).

**2. Refactor `setSelection`:**

```cpp
void MappingInspector::setSelection(const std::vector<juce::ValueTree>& selection)
{
    isUpdatingFromTree = true;
    selectedTrees = selection;

    // ... (Existing Visibility Logic) ...

    if (!selectedTrees.empty())
    {
        auto firstTree = selectedTrees[0];

        // --- FIX ALIAS SELECTION ---
        juce::String hashStr = firstTree.getProperty("deviceHash").toString();
        uintptr_t hash = (uintptr_t)hashStr.getHexValue64();
        
        juce::String aliasName = deviceManager.getAliasName(hash);
        
        // Find ID/Index by Text (Safest way)
        bool found = false;
        for (int i = 0; i < aliasSelector.getNumItems(); ++i)
        {
            if (aliasSelector.getItemText(i) == aliasName)
            {
                aliasSelector.setSelectedItemIndex(i, juce::dontSendNotification);
                found = true;
                break;
            }
        }
        
        // Fallback to Global if name not found (e.g. deleted alias)
        if (!found) 
            aliasSelector.setSelectedItemIndex(0, juce::dontSendNotification);

        // ... (Existing logic for sliders etc) ...
    }
    
    // ...
    isUpdatingFromTree = false;
}
```

### Step 3: Update `Source/MappingEditorComponent.cpp`
Pass the reference.

**Constructor:**
*   `inspector(*undoManager, deviceManager)` (Pass deviceManager).

---

**Generate code for: Updated `MappingInspector.h`, Updated `MappingInspector.cpp`, and Updated `MappingEditorComponent.cpp`.**