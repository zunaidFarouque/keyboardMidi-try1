### 📋 Cursor Prompt: Phase 53.1 - Testing Runtime Layer Logic

**Target File:** `Source/Tests/InputProcessorTests.cpp` (New File)

**Task:**
Create a new test file to verify the **Runtime Behavior** of `InputProcessor`. We need to prove that Layer Commands (Momentary/Toggle) actually change the active layer state.

**Specific Instructions:**

1.  **Create `Source/Tests/InputProcessorTests.cpp`:**
    *   Include headers: `gtest/gtest.h`, `InputProcessor.h`, `VoiceManager.h`, etc.
    *   **Fixture Setup:**
        *   Instantiate `MidiEngine` (mock or real), `SettingsManager`, `DeviceManager`, `PresetManager`, `ScaleLibrary`, `VoiceManager`, `InputProcessor`.
        *   **Crucial:** We need a way to check if `VoiceManager` received a note. Since `VoiceManager` is complex, we might check `InputProcessor::getHighestActiveLayerIndex()` as a proxy for success.

2.  **Test Case 1: Layer Momentary Action:**
    ```cpp
    TEST_F(InputProcessorTest, LayerMomentarySwitching) {
        // 1. Setup: Map Key 10 (Enter) on Layer 0 to "Layer Momentary 1"
        //    (Command ID 10 = LayerMomentary, Data2 = 1)
        {
            auto mappings = presetMgr.getMappingsListForLayer(0);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", 10, nullptr);
            m.setProperty("type", "Command", nullptr); 
            m.setProperty("data1", 10, nullptr); // CommandID::LayerMomentary
            m.setProperty("data2", 1, nullptr);  // Target Layer 1
            mappings.addChild(m, -1, nullptr);
        }

        // 2. Setup: Map Key 20 (Q) on Layer 1 to "Note 50"
        {
            auto mappings = presetMgr.getMappingsListForLayer(1);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", 20, nullptr);
            m.setProperty("type", "Note", nullptr);
            m.setProperty("data1", 50, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        // 3. Compile
        proc.rebuildGrid();

        // 4. Initial State Check
        EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);

        // 5. Act: Press Layer Button (Key 10)
        InputID layerBtn{0, 10}; // Global
        proc.processEvent(layerBtn, true); // Down

        // 6. Assert: Layer 1 should be active
        EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

        // 7. Act: Release Layer Button
        proc.processEvent(layerBtn, false); // Up

        // 8. Assert: Back to Layer 0
        EXPECT_EQ(proc.getHighestActiveLayerIndex(), 0);
    }
    ```

3.  **Update `CMakeLists.txt`:**
    Add `Source/Tests/InputProcessorTests.cpp` to the `OmniKey_Tests` executable sources.

**Expectation:**
This test will **FAIL**. The `getHighestActiveLayerIndex` will likely remain `0` because `processEvent` currently does nothing when it sees a Command. This confirms the bug.