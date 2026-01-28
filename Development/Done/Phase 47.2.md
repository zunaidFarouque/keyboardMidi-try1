# 🤖 Cursor Prompt: Phase 47.2 - Input Processor Debug Tracing

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Mappings for specific Device Aliases are not triggering Audio. Global mappings work.
*   **Goal:** Instrument `InputProcessor` with verbose logging to trace the Compilation (`rebuildMap`) and Execution (`processEvent`) paths.

**Strict Constraints:**
1.  **Log Content:**
    *   In `rebuildMap`: Log how many hardware IDs are found for the target alias.
    *   In `processEvent`: Log the incoming Device ID and whether a match was found in `compiledMap`.
2.  **Output:** Use `DBG()`.

---

### Step 1: Update `Source/InputProcessor.cpp`

**1. Update `addMappingFromTree`:**
Add logging inside the `aliasName` block.

```cpp
    if (!aliasName.isEmpty()) {
        juce::Array<uintptr_t> hardwareIds = deviceManager.getHardwareForAlias(aliasName);
        
        // DEBUG LOG
        // DBG("Compiler: Alias '" + aliasName + "' has " + String(hardwareIds.size()) + " devices.");

        for (uintptr_t hardwareId : hardwareIds) {
            // DBG("Compiler: Adding { " + String::toHexString((int64)hardwareId) + ", " + String(inputKey) + " }");
            InputID inputId = {hardwareId, inputKey};
            targetLayer.compiledMap[inputId] = action;
        }
        // ... global check ...
    }
```

**2. Update `findMapping`:**
Add logging to see what it looks for.

```cpp
const MidiAction *InputProcessor::findMapping(const InputID &input) {
    // DBG("Runtime: Looking for { " + String::toHexString((int64)input.deviceHandle) + ", " + String(input.keyCode) + " }");

    for (int i = (int)layers.size() - 1; i >= 0; --i) {
        if (!layers[i].isActive) continue;
        
        auto &cm = layers[i].compiledMap;
        auto it = cm.find(input);
        if (it != cm.end()) {
             // DBG("Runtime: MATCH found in Layer " + String(i));
             return &it->second;
        }
        
        // ... global fallback ...
    }
    return nullptr;
}
```

---

**Generate code for: Updated `InputProcessor.cpp` (Add DBG lines, you can comment them out later).**