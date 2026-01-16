# 📝 OmniKey Project Architecture & Rules

**Context:** C++20, JUCE Framework, Windows 10/11 (Win32 API).
**Current State:** Phase 1 Complete (Raw Input hooked successfully via `WM_INPUT`).

---

## 🚫 STRICT Architectural Rules (DO NOT BREAK)

### 1. The "Void*" Isolation Rule
*   **Rule:** NEVER include `<windows.h>`, `<hidsdi.h>`, or `<hidpi.h>` in any **Header (`.h`)** file.
*   **Reason:** Windows headers define macros (like `min`/`max`) that corrupt JUCE classes (`juce::Rectangle`).
*   **Implementation:**
    *   In `.h` files: Always use `void*` to represent `HWND` or `HANDLE`.
    *   In `.cpp` files: Include `<windows.h>`, then `static_cast` the `void*` back to the native type.

### 2. Header Include Order
*   **Rule:** In `.cpp` files, always include the corresponding **Project Header** first, *before* any Windows headers.
*   **Example (`MainComponent.cpp`):**
    ```cpp
    #include "MainComponent.h" // Must be first to verify class definitions
    #include "RawInputManager.h"
    // Do NOT include windows.h here.
    ```

### 3. HID Strategy
*   **Rule:** Do **not** use `<hidpi.h>` or `<hidsdi.h>` unless absolutely necessary for parsing complex HID reports.
*   **Current State:** We are successfully using standard `RegisterRawInputDevices` (from `<windows.h>`) to detect keyboards. Keep it simple.

---

## 🛠 Build System (CMake)

### CMakeLists.txt Configuration
*   **Generator:** Visual Studio 17 2022 (or 18 2026).
*   **Platform:** x64.
*   **Required Command:** `juce_generate_juce_header(OmniKey)` must remain to generate `JuceHeader.h`.
*   **Definitions:** `NOMINMAX` and `WIN32_LEAN_AND_MEAN` are defined globally in CMake. **Do not** add them manually to `.cpp` files to avoid redefinition warnings.

```cmake
target_compile_definitions(OmniKey PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)