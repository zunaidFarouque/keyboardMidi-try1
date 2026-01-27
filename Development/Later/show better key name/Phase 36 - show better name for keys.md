# 🤖 Cursor Prompt: Phase 36 - Mapping Table UI Cleanup

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
*   **Current State:** The `MappingEditorComponent` table is redundant.
*   **The Issue:**
    *   **Column 1 (Key):** Displays "[Alias] - [Key]" (e.g., "Laptop - Q").
    *   **Column 2 (Device):** Displays "[Alias]" (e.g., "Laptop").
*   **Goal:** Change Column 1 to display **ONLY** the Key Name (e.g., "Q").

**Strict Constraints:**
1.  **Logic:** In `paintCell` (Column 1), do **not** read the `displayName` property from the ValueTree.
2.  **Implementation:** Read the `inputKey` (int) property and pass it to `KeyNameUtilities::getKeyName(...)` to get the clean name.
3.  **Dependencies:** Ensure `#include "KeyNameUtilities.h"` is present.

---

### Step 1: Update `Source/MappingEditorComponent.cpp`

**Refactor `paintCell`:**

Locate the `switch (columnId)` block. Update Case 1.

```cpp
// ... inside paintCell ...

    switch (columnId)
    {
    case 1: 
        // OLD: text = node.getProperty("displayName").toString();
        // NEW: Calculate clean name dynamically
        text = KeyNameUtilities::getKeyName((int)node.getProperty("inputKey"));
        break;
        
    case 2:
        text = getHex(node.getProperty("deviceHash")); // (This already does Alias lookup logic)
        break;
    
    // ... other cases remain same ...
    }

// ...
```

---

**Generate code for: Updated `Source/MappingEditorComponent.cpp`.**