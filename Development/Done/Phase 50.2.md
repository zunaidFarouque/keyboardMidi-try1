### 📋 Cursor Prompt: Phase 50.2 - The Grid Compiler (Skeleton & Manual Mappings)

**Target Files:**
1.  `CMakeLists.txt`
2.  `Source/GridCompiler.h` (New File)
3.  `Source/GridCompiler.cpp` (New File)

**Task:**
Create the `GridCompiler` class. This class is responsible for converting the `PresetManager` (ValueTree), `DeviceManager`, and `ZoneManager` state into the static `CompiledMapContext` defined in Phase 50.1.

**Specific Instructions:**

1.  **Update `CMakeLists.txt`:**
    Add `Source/GridCompiler.cpp` to the `target_sources`.

2.  **Create `Source/GridCompiler.h`:**
    *   Include `MappingTypes.h`, `PresetManager.h`, `DeviceManager.h`, `ZoneManager.h`.
    *   Define class `GridCompiler`.
    *   Add a public static method:
        ```cpp
        static std::shared_ptr<CompiledMapContext> compile(
            PresetManager& presetMgr, 
            DeviceManager& deviceMgr, 
            ZoneManager& zoneMgr
        );
        ```

3.  **Create `Source/GridCompiler.cpp`:**
    Implement the `compile` method. The logic should be:

    *   **Step 1: Setup**
        *   Create a new `CompiledMapContext`.
        *   Initialize `globalGrid` (filled with empty slots).
        *   Get all defined Aliases from `DeviceManager` and create entry in `deviceGrids` and `visualLookup` for each (plus Global).

    *   **Step 2: Layer Iteration (0 to 8)**
        *   For each layer, get the Mappings ValueTree.
        *   **Important:** We need to populate `visualLookup` for *every* layer, even if empty, so the UI can switch to it safely.

    *   **Step 3: Manual Mapping Compilation**
        *   Iterate through the ValueTree children (Mappings).
        *   Extract `inputKey` (KeyCode) and `deviceHash` (or `inputAlias`).
        *   **Targeting:**
            *   If `deviceHash` is 0 (Global), write to `globalGrid` AND all `deviceGrids`.
            *   If `deviceHash` is specific, write only to that specific `deviceGrid`.
        *   **Writing to AudioGrid:**
            *   Set `slot.isActive = true`.
            *   Set `slot.action` from the mapping properties.
        *   **Writing to VisualGrid:**
            *   Set `slot.state = VisualState::Active`.
            *   Set `slot.displayColor` based on ActionType (use `SettingsManager` logic or hardcoded defaults for now).
            *   Set `slot.label` (e.g. "Note 60" or "CC 1").

    *   **Step 4: Generic Modifier Replication (Critical)**
        *   If the `inputKey` is a Generic Modifier (e.g., `VK_SHIFT` = 0x10), you must **also** write to the specific slots (`VK_LSHIFT` = 0xA0 and `VK_RSHIFT` = 0xA1) **UNLESS** those specific slots were already written to by a specific mapping in this layer.
        *   *Logic:* Check if target slot `isActive`. If false, copy the generic action there.

    *   **Step 5: Return**
        *   Return the populated context.

**Optimization Note:**
Do *not* implement Zone compilation yet (we will do that in 50.3). Just focus on getting Manual Mappings into the Grid correctly.

**Helper for Modifiers:**
Remember the mapping:
*   SHIFT (0x10) -> LSHIFT (0xA0), RSHIFT (0xA1)
*   CTRL (0x11) -> LCTRL (0xA2), RCTRL (0xA3)
*   ALT (0x12) -> LALT (0xA4), RALT (0xA5)