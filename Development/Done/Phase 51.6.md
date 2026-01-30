### 📋 Cursor Prompt: Phase 51.6 - Advanced Feature Verification

**Target Files:**
1.  `Source/Tests/GridCompilerTests.cpp`
2.  `Source/GridCompiler.cpp`

**Task:**
Add tests for "Generic Modifier Expansion" and "Chord Pooling", and update the Compiler logic if necessary to make them pass.

**Specific Instructions:**

1.  **Update `Source/Tests/GridCompilerTests.cpp`:**
    Add these test cases:

    ```cpp
    // TEST F: Generic Modifier Expansion
    // If I map "Shift" (0x10), it should automatically map LShift (0xA0) and RShift (0xA1).
    TEST_F(GridCompilerTest, GenericShiftExpandsToSides) {
        // Arrange
        addMapping(0, 0x10, 0); // VK_SHIFT

        // Act
        auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);
        auto grid = context->visualLookup[0][0];

        // Assert
        EXPECT_EQ((*grid)[0xA0].state, VisualState::Active); // Left Shift
        EXPECT_EQ((*grid)[0xA1].state, VisualState::Active); // Right Shift
    }

    // TEST G: Generic Modifier Override
    // If I map "Shift" (Generic), but then specifically map "LShift", LShift should use the specific mapping.
    TEST_F(GridCompilerTest, SpecificModifierOverridesGeneric) {
        // Arrange
        addMapping(0, 0x10, 0, ActionType::CC);   // Generic -> CC
        addMapping(0, 0xA0, 0, ActionType::Note); // LShift -> Note

        // Act
        auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);
        auto grid = context->visualLookup[0][0];

        // Assert
        EXPECT_EQ((*grid)[0xA0].label, "Note 60"); // LShift specific
        EXPECT_EQ((*grid)[0xA1].label, "CC 1");    // RShift inherited generic
    }

    // TEST H: Chord Compilation (Audio Data)
    TEST_F(GridCompilerTest, ZoneCompilesToChordPool) {
        // Arrange: Zone on Layer 0, Key 81, Triad
        auto zone = zoneMgr.createDefaultZone();
        zone->layerID = 0;
        zone->targetAliasHash = 0;
        zone->inputKeyCodes = {81};
        zone->chordType = ChordUtilities::ChordType::Triad; // 3 notes
        
        // Act
        auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);
        auto audioGrid = context->globalGrids[0];

        // Assert
        const auto& slot = (*audioGrid)[81];
        EXPECT_TRUE(slot.isActive);
        EXPECT_GE(slot.chordIndex, 0); // Must point to pool
        
        // Check Pool
        ASSERT_LT(slot.chordIndex, context->chordPool.size());
        const auto& chord = context->chordPool[slot.chordIndex];
        EXPECT_EQ(chord.size(), 3); // Triad = 3 notes
    }
    ```

2.  **Update `Source/GridCompiler.cpp` (Implementation Check):**
    Ensure the `applyLayerToGrid` -> `compileMappingsForLayer` logic actually handles the expansion.

    *   **In `compileMappingsForLayer`:**
        When iterating mappings, if `inputKey` is `VK_SHIFT` (0x10), `VK_CONTROL` (0x11), or `VK_MENU` (0x12):
        1.  Determine Left/Right codes (e.g., 0xA0, 0xA1).
        2.  **Check Conflict/Touch:** `if (!touchedKeys[leftCode])` -> Write Mapping -> `touchedKeys[leftCode] = true`.
        3.  Repeat for Right code.
        *   *Crucial:* Do this **after** the specific key check so specifics win naturally? Or check `touchedKeys` to see if a specific mapping already claimed it. (The Test G will tell us if we got the order right).

    *   **In `compileZonesForLayer`:**
        Ensure that when a Zone returns multiple notes, we:
        1.  `context.chordPool.push_back(notes);`
        2.  `slot.chordIndex = context.chordPool.size() - 1;`

**Goal:**
Verify that the compiler is "smart" enough to handle Windows key quirks and complex musical data structures. Run the tests.