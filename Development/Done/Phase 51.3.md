Here is the Cursor prompt for **Phase 51.3**. This writes the actual logic tests to prove the Grid Compiler works correctly (or fails where we expect it to).

### 📋 Cursor Prompt: Phase 51.3 - Logic Verification Tests

**Target File:** `Source/Tests/GridCompilerTests.cpp` (New File)

**Task:**
Create a comprehensive test suite to verify the `GridCompiler` logic without running the GUI. We need to verify Layer Stacking, Overrides, and Conflict detection.

**Specific Instructions:**

1.  **Create the File:** `Source/Tests/GridCompilerTests.cpp`.

2.  **Add Setup Code (The Fixture):**
    We need a standard setup for every test (Managers + Compiler).
    ```cpp
    #include <gtest/gtest.h>
    #include "../GridCompiler.h"
    #include "../PresetManager.h"
    #include "../DeviceManager.h"
    #include "../ZoneManager.h"
    #include "../ScaleLibrary.h"
    #include "../MappingTypes.h"

    class GridCompilerTest : public ::testing::Test {
    protected:
        // Core Dependencies
        PresetManager presetMgr;
        DeviceManager deviceMgr;
        ScaleLibrary scaleLib;
        ZoneManager zoneMgr{scaleLib};
        
        // Mock Device Hash
        const juce::String aliasName = "TestDevice";
        uintptr_t aliasHash;

        void SetUp() override {
            // Setup a clean state
            presetMgr.getLayersList().removeAllChildren(nullptr);
            presetMgr.ensureStaticLayers();
            
            // Create a test alias
            deviceMgr.createAlias(aliasName);
            aliasHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
        }

        // Helper to add a manual mapping
        void addMapping(int layerId, int keyCode, uintptr_t deviceH, ActionType type = ActionType::Note) {
            auto mappings = presetMgr.getMappingsListForLayer(layerId);
            juce::ValueTree m("Mapping");
            m.setProperty("inputKey", keyCode, nullptr);
            // We use the hex string format that DeviceManager expects
            m.setProperty("deviceHash", juce::String::toHexString((juce::int64)deviceH).toUpperCase(), nullptr);
            m.setProperty("type", (type == ActionType::Note ? "Note" : "CC"), nullptr);
            m.setProperty("layerID", layerId, nullptr);
            mappings.addChild(m, -1, nullptr);
        }

        // Helper to add a Zone
        void addZone(int layerId, int startKey, int count, uintptr_t targetHash) {
            auto zone = zoneMgr.createDefaultZone();
            zone->layerID = layerId;
            zone->targetAliasHash = targetHash;
            zone->inputKeyCodes.clear();
            for(int i=0; i<count; ++i) zone->inputKeyCodes.push_back(startKey + i);
            // Zone is already added to manager by createDefaultZone
        }
    };
    ```

3.  **Test Case 1: Vertical Inheritance (Layer 0 -> Layer 1)**
    *   **Goal:** Prove that a key mapped on Layer 0 shows up as `Inherited` on Layer 1.
    *   **Code:**
        ```cpp
        TEST_F(GridCompilerTest, VerticalInheritance) {
            // Arrange: Map Q (Key 81) on Layer 0 (Global)
            addMapping(0, 81, 0); 

            // Act: Compile
            auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

            // Assert: Layer 0 Global
            auto l0 = context->visualLookup[0][0];
            EXPECT_EQ((*l0)[81].state, VisualState::Active);

            // Assert: Layer 1 Global
            auto l1 = context->visualLookup[0][1];
            EXPECT_EQ((*l1)[81].state, VisualState::Inherited);
            EXPECT_EQ((*l1)[81].displayColor.getAlpha(), 76); // ~0.3f alpha (checking if it is dimmed)
        }
        ```

4.  **Test Case 2: Vertical Override (Layer 1 blocks Layer 0)**
    *   **Goal:** Prove that mapping the same key on Layer 1 overrides Layer 0.
    *   **Code:**
        ```cpp
        TEST_F(GridCompilerTest, VerticalOverride) {
            // Arrange
            addMapping(0, 81, 0); // Base: Note
            addMapping(1, 81, 0); // Overlay: Note

            // Act
            auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

            // Assert
            auto l1 = context->visualLookup[0][1];
            EXPECT_EQ((*l1)[81].state, VisualState::Override);
        }
        ```

5.  **Test Case 3: Conflict Detection (The Bug Fix)**
    *   **Goal:** Prove that Mapping + Zone on same key/layer = Conflict.
    *   **Code:**
        ```cpp
        TEST_F(GridCompilerTest, ConflictDetection) {
            // Arrange: Layer 0
            // 1. Add Mapping on Key 81
            addMapping(0, 81, 0);
            
            // 2. Add Zone covering Key 81
            addZone(0, 81, 1, 0); // Zone with 1 key (81)

            // Act
            auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

            // Assert
            auto l0 = context->visualLookup[0][0];
            EXPECT_EQ((*l0)[81].state, VisualState::Conflict);
            EXPECT_EQ((*l0)[81].displayColor, juce::Colours::red);
        }
        ```

6.  **Test Case 4: Device Specific Override**
    *   **Goal:** Global Mapping vs Specific Device Mapping on same layer.
    *   **Code:**
        ```cpp
        TEST_F(GridCompilerTest, DeviceOverridesGlobal) {
            // Arrange
            addMapping(0, 81, 0);         // Global Q
            addMapping(0, 81, aliasHash); // Specific Q

            // Act
            auto context = GridCompiler::compile(presetMgr, deviceMgr, zoneMgr);

            // Assert: Global Grid should be Active
            auto globalGrid = context->visualLookup[0][0];
            EXPECT_EQ((*globalGrid)[81].state, VisualState::Active);

            // Assert: Device Grid should be Override (Local overrides inherited Global)
            auto deviceGrid = context->visualLookup[aliasHash][0];
            EXPECT_EQ((*deviceGrid)[81].state, VisualState::Override);
        }
        ```

**Outcome:**
Running this test suite will tell us *instantly* if the `GridCompiler` logic is correct. If "ConflictDetection" fails (shows `Active` or `Override`), we know the `touchedKeys` logic isn't working properly yet.