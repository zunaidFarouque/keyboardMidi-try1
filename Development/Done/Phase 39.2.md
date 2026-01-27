# 🤖 Cursor Prompt: Phase 39.2 - Fix Visualizer Hash Truncation

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 39.1 Complete.
*   **The Bug:** In the Visualizer, selecting a specific Device Alias fails to show mappings for that alias. It only shows inherited Global mappings.
*   **The Cause:** `juce::ComboBox` IDs are 32-bit `int`. Alias Hashes are 64-bit `uintptr_t`. Storing the hash in the ID truncates it, causing lookup failures in `InputProcessor`.

**Phase Goal:**
1.  Update `VisualizerComponent` to store a `std::vector<uintptr_t> viewHashes`.
2.  Use the ComboBox **Index** to retrieve the correct 64-bit Hash from this vector.
3.  Ensure `InputProcessor::simulateInput` logic correctly identifies "Overrides" (Local exists + Global exists) vs "Conflicts".

---

### Step 1: Update `VisualizerComponent.h`
Add the storage.

**Members:**
*   `std::vector<uintptr_t> viewHashes;`
*   Remove any reliance on `viewSelector.getSelectedId()`.

### Step 2: Update `VisualizerComponent.cpp`
Fix the population and selection logic.

**1. Refactor `updateViewSelector()` (or Constructor):**
```cpp
void VisualizerComponent::updateViewSelector()
{
    // 1. Save current selection
    uintptr_t currentHash = 0;
    if (!viewHashes.empty()) 
        currentHash = viewHashes[viewSelector.getSelectedItemIndex()];

    // 2. Clear
    viewSelector.clear();
    viewHashes.clear();

    // 3. Add Master (Index 0)
    viewSelector.addItem("Any / Master", 1);
    viewHashes.push_back(0);

    // 4. Add Aliases
    auto aliases = deviceManager.getAllAliasNames(); // Assuming this returns StringArray
    // Or iterate hardware map if needed, but names are better.
    // Need a way to get Name AND Hash.
    // Let's iterate DeviceManager's internal list or use a helper "getAllAliases()" returns pair.
    
    // BETTER:
    // DeviceManager should expose: std::vector<std::pair<String, uintptr_t>> getAliases();
    // For now, if getAllAliasNames() exists:
    for (const auto& name : aliases)
    {
        viewSelector.addItem(name, viewSelector.getNumItems() + 1);
        viewHashes.push_back(MappingTypes::getAliasHash(name));
    }

    // 5. Restore selection
    // Find index of currentHash in viewHashes, default to 0
    // ... logic ...
}
```

**2. Refactor `viewSelector.onChange`:**
```cpp
viewSelector.onChange = [this] {
    int index = viewSelector.getSelectedItemIndex();
    if (index >= 0 && index < viewHashes.size()) {
        currentViewHash = viewHashes[index];
        refreshCache(); // Or repaint
    }
};
```

### Step 3: Update `DeviceManager`
Ensure we can get the list cleanly.

**Method:** `juce::StringArray getAllAliasNames();` (If not already present).

### Step 4: Verify `InputProcessor.cpp` (`simulateInput`)
Ensure the logic handles the Override state correctly now that the hash is correct.
*   **Logic:**
    *   If `specificMap` exists AND (`globalMap` exists OR `globalZone` exists) -> `VisualState::Override`.
    *   *Visuals:* This should draw an **Orange Border** in the Visualizer (Phase 39.1 logic).

---

**Generate code for: Updated `VisualizerComponent.h/cpp` and check `DeviceManager` helper.**