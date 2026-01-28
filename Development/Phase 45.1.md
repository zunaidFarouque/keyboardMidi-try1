# 🤖 Cursor Prompt: Phase 45.1 - Force Inspector Refresh on Layer Switch

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
*   **Current State:** Phase 45 Complete. Layer switching restores the row index correctly.
*   **The Bug:** If the row index is the same between layers (e.g., Row 0 -> Row 0), `TableListBox` does not trigger a selection change callback. The Inspector stays stuck showing the old layer's data.
*   **Phase Goal:** Ensure the Inspector **always** refreshes when the Layer changes, even if the row number remains identical.

**Strict Constraints:**
1.  **Refactoring:** Extract the logic from `selectedRowsChanged` into a new private helper `void updateInspectorFromSelection();`.
2.  **Logic:**
    *   `selectedRowsChanged` calls `updateInspectorFromSelection()`.
    *   `layerList.onLayerSelected` calls `updateInspectorFromSelection()` **after** restoring the row selection.

---

### Step 1: Update `MappingEditorComponent.h`
Add the helper method.

```cpp
private:
    void updateInspectorFromSelection(); // New Helper
```

### Step 2: Update `MappingEditorComponent.cpp`
Refactor the selection logic.

**1. Implement `updateInspectorFromSelection`:**
Move the existing logic from `selectedRowsChanged` here.
```cpp
void MappingEditorComponent::updateInspectorFromSelection()
{
    // 1. Get Indices
    auto selectedRows = table.getSelectedRows(); // SparseSet
    
    // 2. Build List of Trees
    std::vector<juce::ValueTree> selectedTrees;
    auto mappingsNode = presetManager.getMappingsNode(currentLayerId); // Use CURRENT Layer
    
    for (int i = 0; i < selectedRows.size(); ++i)
    {
        int row = selectedRows[i];
        if (row < mappingsNode.getNumChildren())
        {
            selectedTrees.push_back(mappingsNode.getChild(row));
        }
    }

    // 3. Update Inspector
    inspector.setSelection(selectedTrees);
}
```

**2. Update `selectedRowsChanged`:**
```cpp
void MappingEditorComponent::selectedRowsChanged(int lastRowSelected)
{
    updateInspectorFromSelection();
}
```

**3. Update Constructor (`onLayerSelected` lambda):**
```cpp
    layerList.onLayerSelected = [this](int newLayerId) {
        // ... (Existing History Save Logic) ...

        currentLayerId = newLayerId;
        table.updateContent();

        // ... (Existing History Restore Logic) ...
        // if (savedRow...) table.selectRow(savedRow);

        // FORCE UPDATE (Fixes the issue)
        updateInspectorFromSelection(); 
    };
```

---

**Generate code for: Updated `MappingEditorComponent.h` and `MappingEditorComponent.cpp`.**