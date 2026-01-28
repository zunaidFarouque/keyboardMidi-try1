# 🤖 Cursor Prompt: Phase 46.6 - Fix Learn Data Consistency

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 46.5 Complete.
*   **The Bug:** "Learning" a mapping updates the UI correctly, but the mapping does not generate MIDI. Manually selecting the same alias fixes it.
*   **Diagnosis:** The "Learn" logic writes the `deviceHash` (Alias Hash) but fails to write the `inputAlias` (String Name). The `InputProcessor` compiler relies on `inputAlias` being present to correctly identify the target hardware. If missing, it treats `deviceHash` as a legacy Hardware ID, which fails.
*   **Phase Goal:** Update `MappingEditorComponent::handleRawKeyEvent` to resolve the Alias Hash to a Name and write it to the ValueTree.

**Strict Constraints:**
1.  **Logic:** After finding `bestAlias` (uintptr_t), call `deviceManager.getAliasName(bestAlias)`.
2.  **Write:** `node.setProperty("inputAlias", aliasName, nullptr);`
3.  **Result:** The XML node must look identical whether created via Dropdown or Learn.

---

### Step 1: Update `Source/MappingEditorComponent.cpp`

**Refactor `handleRawKeyEvent`:**

Focus on the block where `foundValid` is true.

```cpp
            if (foundValid)
            {
                juce::String aliasName = deviceManager.getAliasName(bestAlias);
                
                DBG("Learn: Switching to Alias " + aliasName + " (" + juce::String::toHexString((int64)bestAlias) + ")");
                
                // 1. Write Hash (For UI selection logic)
                node.setProperty("deviceHash", juce::String::toHexString((int64)bestAlias), nullptr);
                
                // 2. Write Name (CRITICAL for InputProcessor Compiler)
                node.setProperty("inputAlias", aliasName, nullptr);
            }
            else
            {
                DBG("Learn: No alias found. Keeping existing.");
            }
```

**Also handle the Global Case (Studio Mode OFF):**
```cpp
            else
            {
                // Studio Mode OFF -> Force Global
                node.setProperty("deviceHash", "0", nullptr);
                node.setProperty("inputAlias", "Global (All Devices)", nullptr); // Ensure name matches
            }
```

---

**Generate code for: Updated `Source/MappingEditorComponent.cpp`.**