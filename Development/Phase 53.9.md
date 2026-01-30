### 📋 Cursor Prompt: Phase 53.9 - Device-Specific Integration Test

**Target File:** `Source/Tests/InputProcessorTests.cpp`

**Task:**
Add a test case that mimics the exact user scenario: Using a specific Hardware Device to trigger a Layer change, then playing a note on that new layer. We verify both **Device-Specific Mappings** and **Device-Inheriting-Global Mappings**.

**Specific Instructions:**

1.  **Open `Source/Tests/InputProcessorTests.cpp`**.
2.  **Add `TEST_F(InputProcessorTest, DeviceSpecificLayerSwitching)`:**

    ```cpp
    TEST_F(InputProcessorTest, DeviceSpecificLayerSwitching) {
        // --- ARRANGE ---
        // Mock a specific device hash (e.g. 0x12345678)
        uintptr_t devHash = 0x12345;
        // Important: We must register this alias in DeviceManager so the Compiler creates a grid for it!
        deviceMgr.createAlias("TestDevice");
        deviceMgr.assignHardware("TestDevice", devHash);
        // Note: The compiler uses the Alias Hash, InputProcessor uses Hardware Hash.
        // In this test setup, we simulate the "Hardware Hash" being passed in, 
        // and we ensure DeviceManager maps it to the Alias.
        // Wait: The Compiler iterates *Aliases*. 
        // InputProcessor's compile looks up hardware IDs from Alias to build 'deviceGrids'.
        // So we need: Alias "TestDevice" -> Hardware ID 0x12345.
        // Then we add mapping to Alias "TestDevice".
        
        uintptr_t aliasHash = static_cast<uintptr_t>(std::hash<juce::String>{}("TestDevice"));

        int keyLayer = 10;
        int keyNoteLocal = 20;
        int keyNoteGlobal = 30;

        // 1. Layer 0 (Device Specific): Key A -> Momentary Layer 1
        {
            auto mappings = presetMgr.getMappingsListForLayer(0);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyLayer, nullptr);
            m.setProperty("inputAlias", "TestDevice", nullptr); // Map to Alias
            m.setProperty("type", "Command", nullptr);
            m.setProperty("data1", (int)OmniKey::CommandID::LayerMomentary, nullptr);
            m.setProperty("data2", 1, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        // 2. Layer 1 (Device Specific): Key S -> Note 60
        {
            auto mappings = presetMgr.getMappingsListForLayer(1);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyNoteLocal, nullptr);
            m.setProperty("inputAlias", "TestDevice", nullptr);
            m.setProperty("type", "Note", nullptr);
            m.setProperty("data1", 60, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        // 3. Layer 1 (Global): Key D -> Note 62
        // Device should inherit this!
        {
            auto mappings = presetMgr.getMappingsListForLayer(1);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyNoteGlobal, nullptr);
            m.setProperty("inputAlias", "", nullptr); // Global
            m.setProperty("type", "Note", nullptr);
            m.setProperty("data1", 62, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        proc.forceRebuildMappings();

        // --- ACT 1: Hold Layer Key on Device ---
        InputID idLayer{devHash, keyLayer};
        proc.processEvent(idLayer, true); 

        // Assert State
        EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);
        
        // Check Status Bar Text (Diagnostic)
        auto names = proc.getActiveLayerNames();
        EXPECT_TRUE(names.contains("Layer 1 (Hold)"));

        // --- ACT 2: Press Local Note Key on Device ---
        InputID idLocal{devHash, keyNoteLocal};
        // We can't check audio output easily, but we can check if getMappingForInput finds it
        // resolving the full stack logic.
        // Actually, we can check processEvent logic by spying or just relying on internal state if we had it.
        // Let's rely on finding the mapping in the active context manually to verify the GRID exists.
        
        auto ctx = proc.getContext();
        // The device grid for Layer 1 must exist
        ASSERT_TRUE(ctx->deviceGrids.count(devHash));
        auto gridL1 = ctx->deviceGrids.at(devHash)[1];
        ASSERT_TRUE(gridL1 != nullptr);
        
        EXPECT_TRUE((*gridL1)[keyNoteLocal].isActive); // Local mapping
        EXPECT_TRUE((*gridL1)[keyNoteGlobal].isActive); // Inherited Global mapping
    }
    ```

3.  **Run the Test.**
    *   If this fails, we know the **Device Grid compilation** or **Hardware ID lookup** is broken.
    *   If this passes, then the issue is likely **RawInputManager** sending a Handle that doesn't match what `DeviceManager` expects (e.g. 0 vs 0x... mismatch, or Studio Mode logic in `InputProcessor` ignoring the device).

**One Critical Check in `InputProcessor.cpp`:**
In `processEvent`, look at this line:
```cpp
// Determine effectiveDevice
uintptr_t effectiveDevice = input.deviceHandle;
if (!settingsManager.isStudioMode()) {
    effectiveDevice = 0; // Force Global ID
}
```
If Studio Mode is OFF (default), `effectiveDevice` becomes 0.
But if we mapped the Layer Button to "My Keyboard" (Specific Alias), and `effectiveDevice` is forced to 0, `processEvent` will look at `globalGrids` and **NOT find the button**.

**Theory:** The user mapped a key to a specific device, but Studio Mode is OFF, so the engine is ignoring device-specific mappings.
**Fix:** The test above uses `isStudioMode` default (likely false). We might need to explicitly set `settingsManager.setStudioMode(true)` in the test to make it pass. **Add that line to the test setup.**