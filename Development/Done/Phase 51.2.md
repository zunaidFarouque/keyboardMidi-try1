Here is the Cursor prompt for **Phase 51.2**. This phase installs the testing machinery (GoogleTest) and verifies that the Test Executable can link to your Core logic.

### 📋 Cursor Prompt: Phase 51.2 - The Test Harness

**Target Files:**
1.  `CMakeLists.txt`
2.  `Source/Tests/SanityTest.cpp` (New File)

**Task:**
Set up the GoogleTest framework and create a new console executable named `OmniKey_Tests`. This executable will run our unit tests.

**Specific Instructions:**

1.  **Update `CMakeLists.txt`:**
    *   Add the **GoogleTest** dependency using `FetchContent` (add this block near the top, after `project(...)`):
        ```cmake
        include(FetchContent)
        FetchContent_Declare(
          googletest
          URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
        )
        # For Windows: Prevent GTest from overriding /MD with /MT
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
        ```

    *   **Define the Test Executable** (add this at the very end of the file):
        ```cmake
        # --- UNIT TESTS ---
        
        # 1. Enable CTest
        enable_testing()

        # 2. Define the Test Runner Exe
        add_executable(OmniKey_Tests
            Source/Tests/SanityTest.cpp
        )

        # 3. Link Dependencies
        # We link against our Core library (logic) and GTest
        target_link_libraries(OmniKey_Tests PRIVATE
            OmniKey_Core
            GTest::gtest_main # Provides a standard main() that runs all tests
        )

        # 4. Register with CTest
        include(GoogleTest)
        gtest_discover_tests(OmniKey_Tests)
        ```

2.  **Create `Source/Tests/SanityTest.cpp`:**
    Create a simple test to prove we can access Core classes and that math still works.

    ```cpp
    #include <gtest/gtest.h>
    #include "../MappingTypes.h" 
    #include "../ScaleUtilities.h" // Example of reaching into Core

    // 1. Basic Math Check (Infrastructure Test)
    TEST(SanityCheck, MathWorks) {
        EXPECT_EQ(1 + 1, 2);
    }

    // 2. Core Linkage Check (Dependency Test)
    TEST(SanityCheck, CanAccessCoreEnums) {
        // Just verify we can use types defined in OmniKey_Core
        ActionType t = ActionType::Note;
        EXPECT_EQ(t, ActionType::Note);
    }

    // 3. Logic Check (ScaleUtilities Test)
    TEST(SanityCheck, ScaleLogicFunction) {
        // C Major (0, 2, 4, 5, 7, 9, 11)
        std::vector<int> majorScale = {0, 2, 4, 5, 7, 9, 11};
        int root = 60; // C4
        
        // Degree 0 should be 60
        EXPECT_EQ(ScaleUtilities::calculateMidiNote(root, majorScale, 0), 60);
        
        // Degree 1 should be 62 (D4)
        EXPECT_EQ(ScaleUtilities::calculateMidiNote(root, majorScale, 1), 62);
    }
    ```

**Goal:**
After this, you should be able to run `OmniKey_Tests` (or use the Test Explorer in your IDE) and see 3 passing tests. This proves the architecture is ready for complex logic verification.