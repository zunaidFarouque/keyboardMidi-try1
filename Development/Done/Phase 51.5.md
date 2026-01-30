### 📋 Cursor Prompt: Phase 51.5 - Horizontal Inheritance (Device Scope)

**Target Files:**
1. `Source/Tests/GridCompilerTests.cpp`
2. `Source/GridCompiler.cpp`

**Task:**
Implement Pass 2 of the `GridCompiler` to handle specific devices, and add tests to prove Device > Global priority.

**Specific Instructions:**

1. **Update `Source/Tests/GridCompilerTests.cpp`:**
   Add these two tests at the bottom of the file.

   ```cpp
   // TEST C: Horizontal Inheritance (Global flows into Device)
   TEST_F(GridCompilerTest, DeviceInheritsFromGlobal) {
       // Arrange: Map Q globally on Layer 0
       addMapping(0, 81, 0); 

       // Act
       auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

       // Assert: The specific device should see this as Inherited
       auto deviceGrid = context->visualLookup[aliasHash][0];
       EXPECT_EQ((*deviceGrid)[81].state, VisualState::Inherited);
   }

   // TEST D: Horizontal Override (Device masks Global)
   TEST_F(GridCompilerTest, DeviceOverridesGlobal) {
       // Arrange
       addMapping(0, 81, 0);         // Global maps Q to Note
       addMapping(0, 81, aliasHash, ActionType::CC); // Device maps Q to CC

       // Act
       auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

       // Assert: Device grid should show Override
       auto deviceGrid = context->visualLookup[aliasHash][0];
       EXPECT_EQ((*deviceGrid)[81].state, VisualState::Override);
   }
   ```

2. **Update `Source/GridCompiler.cpp` (Main Loop):**
   Add "Pass 2" right after Pass 1 (Global) finishes.

   ```cpp
   // ... (End of Pass 1: Global Stack) ...

   // 4. PASS 2: Compile Device Stacks (Horizontal)
   auto aliases = deviceMgr.getAllAliasNames();
   for (const auto& aliasName : aliases) {
       uintptr_t devHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
       if (devHash == 0) continue; // Skip Global (already done)

       context->visualLookup[devHash].resize(9);

       for (int L = 0; L < 9; ++L) {
           auto vGrid = std::make_shared<VisualGrid>();
           auto aGrid = std::make_shared<AudioGrid>();

           // STEP A: INHERIT FROM GLOBAL AT THIS LAYER
           *vGrid = *context->visualLookup[0][L];
           *aGrid = *context->globalGrids[L];

           // VISUAL TRANSITION: Global data is "Inherited" from the device's perspective
           for (auto& slot : *vGrid) {
               if (slot.state != VisualState::Empty) {
                   slot.state = VisualState::Inherited;
               }
           }

           // STEP B: APPLY DEVICE SPECIFIC STACK (0 to L)
           // Why 0 to L? Because Device[0] overrides Global[0] which is inside Global[L].
           for (int k = 0; k <= L; ++k) {
               applyLayerToGrid(*vGrid, *aGrid, k, devHash);
           }

           // Store in Context
           context->visualLookup[devHash][L] = vGrid;
           // Note: deviceGrids uses std::array, so assign directly
           context->deviceGrids[devHash][L] = aGrid; 
       }
   }

   return context;
   ```

**Goal:**
All tests should pass. The `DeviceInheritsFromGlobal` test proves that if I only map things to "Global", my "Laptop" view still shows them. The `DeviceOverridesGlobal` test proves the specific device takes priority if it has a conflicting mapping.
