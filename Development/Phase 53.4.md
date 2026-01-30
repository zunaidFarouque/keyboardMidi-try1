
### The Plan

1.  **New Test (Fail First):** We will add `LayerCmdNotInherited` and `DeviceVerticalInheritance` to prove the bugs.
2.  **Fix GridCompiler (Pass 1 - Vertical):** Modify the copy loop to **filter out** Layer Commands.
3.  **Fix GridCompiler (Pass 2 - Device):** Modify the device loop. Instead of naive re-application, we must instruct `applyLayerToGrid` that "Hey, I am applying Layer 0 data, but the View is Layer 1, so mark it **Inherited**".

I will start with the **Test Prompt** to confirm these logic gaps.

### 📋 Cursor Prompt: Phase 53.4 - Testing Inheritance Fixes

**Target File:** `Source/Tests/GridCompilerTests.cpp`

**Task:**
Add tests to expose bugs in Layer Command inheritance and Device-scope vertical inheritance.

**Specific Instructions:**

1.  **Add `TEST_F(GridCompilerTest, LayerCommandsAreNotInherited)`:**
    *   **Arrange:** Map Key 10 on Layer 0 to `CommandID::LayerMomentary`.
    *   **Act:** Compile.
    *   **Assert:**
        *   Layer 0, Key 10 is `Active`.
        *   Layer 1, Key 10 is `Empty` (VisualState::Empty). *Currently this will fail (it will be Inherited).*

2.  **Add `TEST_F(GridCompilerTest, DeviceVerticalInheritanceIsDimmed)`:**
    *   **Arrange:** Map Key 20 on Layer 0 for Specific Device (AliasHash).
    *   **Act:** Compile.
    *   **Assert:**
        *   Device View, Layer 0, Key 20 is `Active`.
        *   Device View, Layer 1, Key 20 is `Inherited`. *Currently this will fail (it will be Active/Override due to the k-loop).*

**Note:** Run these tests. They should fail. Then we fix the compiler.