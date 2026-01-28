# 🤖 Cursor Prompt: Phase 39.9 - Modifier Key Alias Lookup

**Role:** Expert C++ Audio Developer (System Logic).

**Context:**
*   **Current State:** Visualizer uses Specific codes (`VK_LSHIFT` 0xA0) for drawing. Mappings often use Generic codes (`VK_SHIFT` 0x10).
*   **The Bug:** Mapped Modifier keys appear uncolored in the Visualizer because `simulateInput` doesn't link `LShift` to `Shift`.
*   **Phase Goal:** Update `InputProcessor::simulateInput` (and `processEvent` logic helper `lookupAction`) to fallback to Generic Modifier codes if Specific ones are not found.

**Strict Constraints:**
1.  **Mapping:**
    *   `VK_LSHIFT` / `VK_RSHIFT` -> Fallback to `VK_SHIFT`.
    *   `VK_LCONTROL` / `VK_RCONTROL` -> Fallback to `VK_CONTROL`.
    *   `VK_LMENU` / `VK_RMENU` (Alt) -> Fallback to `VK_MENU`.
2.  **Implementation:** Refactor the lookup logic inside `InputProcessor` to handle this aliasing so both the Audio Engine and Visualizer benefit.

---

### Step 1: Update `Source/InputProcessor.cpp`

**1. Create Helper Method:** `int getGenericKey(int specificKey)`
```cpp
int getGenericKey(int specificKey)
{
    switch (specificKey) {
        case 0xA0: // VK_LSHIFT
        case 0xA1: // VK_RSHIFT
            return 0x10; // VK_SHIFT
        case 0xA2: // VK_LCONTROL
        case 0xA3: // VK_RCONTROL
            return 0x11; // VK_CONTROL
        case 0xA4: // VK_LMENU
        case 0xA5: // VK_RMENU
            return 0x12; // VK_MENU
        default: 
            return 0;
    }
}
```

**2. Refactor `findMapping` (The Core Search):**
Currently, `findMapping` checks `{Device, Key}` then `{0, Key}` inside the layer loop.
Update it to also check Generics.

```cpp
const MidiAction *InputProcessor::findMapping(const InputID &input) {
    int genericKey = getGenericKey(input.keyCode);

    for (int i = (int)layers.size() - 1; i >= 0; --i) {
        if (!layers[i].isActive) continue;
        
        auto &cm = layers[i].compiledMap;
        
        // 1. Specific Device, Specific Key
        auto it = cm.find(input);
        if (it != cm.end()) return &it->second;
        
        // 2. Specific Device, Generic Key
        if (genericKey != 0) {
            it = cm.find({input.deviceHandle, genericKey});
            if (it != cm.end()) return &it->second;
        }

        // 3. Global Device, Specific Key
        if (input.deviceHandle != 0) {
            InputID anyDevice = {0, input.keyCode};
            it = cm.find(anyDevice);
            if (it != cm.end()) return &it->second;
            
            // 4. Global Device, Generic Key
            if (genericKey != 0) {
                InputID anyDeviceGeneric = {0, genericKey};
                it = cm.find(anyDeviceGeneric);
                if (it != cm.end()) return &it->second;
            }
        }
    }
    return nullptr;
}
```

**3. Refactor `simulateInput` (Visualizer Lookup):**
Apply similar logic to the `configMap` search.

```cpp
// ... inside layer loop ...
auto &cm = layers[i].configMap;
int genericKey = getGenericKey(keyCode);

// (Adapt the specific/global checks to include genericKey fallbacks)
// Example:
if (auto it = cm.find({viewDeviceHash, keyCode}); it != cm.end()) ...
else if (genericKey && (it = cm.find({viewDeviceHash, genericKey})) != cm.end()) ...
// ... same for global ...
```

**4. Zone Lookup:**
Update `zoneManager.handleInput` logic?
*   Actually, Zones typically use specific keys (user presses Q).
*   However, if user added "Shift" to a Zone using "Assign Keys" and it captured `0x10`, but Visualizer queries `0xA0`, we might need it there too.
*   *Decision:* For now, apply only to Manual Mappings. Zones usually capture exact codes.

---

**Generate code for: Updated `InputProcessor.cpp`.**