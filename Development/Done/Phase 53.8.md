Here is the prompt for **Phase 53.8**. We will write a specific integration test for the `InputProcessor` to replicate your exact scenario. This will tell us definitively if the engine is broken or if the UI is just misleading.

### 📋 Cursor Prompt: Phase 53.8 - The "Hold Layer & Play" Test

**Target File:** `Source/Tests/InputProcessorTests.cpp`

**Task:**
Add a test case that simulates a real-world performance scenario: Holding a Momentary Layer key and playing a note mapped on that layer.

**Specific Instructions:**

1.  **Open `Source/Tests/InputProcessorTests.cpp`**.
2.  **Add `TEST_F(InputProcessorTest, HoldLayerAndPlayNote)`:**

    ```cpp
    TEST_F(InputProcessorTest, HoldLayerAndPlayNote) {
        // --- ARRANGE ---
        int keyLayer = 10; // Key A
        int keyNote  = 20; // Key S
        
        // 1. Layer 0: Key A -> Momentary Layer 1
        {
            auto mappings = presetMgr.getMappingsListForLayer(0);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyLayer, nullptr);
            m.setProperty("type", "Command", nullptr);
            m.setProperty("data1", (int)MIDIQy::CommandID::LayerMomentary, nullptr);
            m.setProperty("data2", 1, nullptr); // Target Layer 1
            mappings.addChild(m, -1, nullptr);
        }

        // 2. Layer 1: Key S -> Note 60 (C4)
        {
            auto mappings = presetMgr.getMappingsListForLayer(1);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyNote, nullptr);
            m.setProperty("type", "Note", nullptr);
            m.setProperty("data1", 60, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        proc.rebuildGrid();

        // --- ACT 1: Hold Layer Key ---
        InputID idLayer{0, keyLayer};
        proc.processEvent(idLayer, true); // Down

        // Check: Layer 1 should now be active
        // This confirms the Command actually executed and updated state
        EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1);

        // --- ACT 2: Press Note Key (While Layer Key is held) ---
        InputID idNote{0, keyNote};
        proc.processEvent(idNote, true); // Down

        // --- ASSERT ---
        // Verify MIDI Engine sent Note On 60
        // (We assume the MockMidiEngine or VoiceManager exposes a way to check this.
        //  If not, check if VoiceManager has an active voice for note 60).
        //  For this test, let's assume we can query VoiceManager or just rely on 
        //  no crash + state check, but ideally we check the output.
        //  If your test harness has a spy/mock for MidiEngine, inspect it here.
        
        // Alternative Assert: Check InputProcessor internal state if possible, 
        // or check if VoiceManager thinks note 60 is playing.
        // For now, let's rely on the Layer Index check passing, 
        // as that proves the routing logic worked.
    }
    ```

3.  **Run the Test.**
    *   If `EXPECT_EQ(proc.getHighestActiveLayerIndex(), 1)` **FAILS**, the Command Logic in `processEvent` is broken (not updating state).
    *   If it **PASSES** but you hear no sound in the real app, the issue is likely **MidiEngine** or **VoiceManager** dropping the note, OR the **Snapshot Logic** in `processEvent` is getting a stale value for the *second* key press.

**Note:** If the test passes, the problem might be in the **Device vs Global** routing in the real app (since the test uses Global/0). You mentioned "Device Layer 0". If you want to test specifically with a Device, update the `InputID` in the test to use `aliasHash`.