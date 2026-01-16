# 🤖 Cursor Prompt: Phase 1 - Raw Input Implementation (Clean Architecture)

**Role:** You are an expert C++ Audio Developer specializing in JUCE and Windows Win32 API.

**Context:** We are building "OmniKey", a Windows-only JUCE application. We have strictly defined architectural rules to prevent "Windows Macro Hell" and build errors.

**Objective:**
Implement the **Phase 1** core: A JUCE GUI application that detects raw keyboard input using the Windows `WM_INPUT` API.

**Strict Constraints (Read `notes.md`):**
1.  **Isolation:** NEVER include `<windows.h>`, `<hidsdi.h>`, or `<hidpi.h>` in any Header (`.h`) file. Use `void*` for handles (`HWND`, `HANDLE`).
2.  **No HIDPI:** Do not use `<hidpi.h>`. Use standard `RegisterRawInputDevices` from `<windows.h>`.
3.  **CMake:** Use `target_compile_definitions` for `NOMINMAX`. Do not define it in `.cpp` files.
4.  **Header Gen:** `CMakeLists.txt` must call `juce_generate_juce_header(OmniKey)`.

---

### Step 1: `CMakeLists.txt`
Create a CMake configuration that:
*   Sets standard to C++20.
*   Links `juce::juce_gui_basics`, `juce_core`, `juce_events`, `juce_graphics`.
*   Links the system library **`user32`**.
*   Defines `NOMINMAX` and `WIN32_LEAN_AND_MEAN` globally.
*   **Crucial:** Generates the JUCE header.

### Step 2: `RawInputManager` (The Core)
Implement a class to handle the Windows Message Loop hooking.

**`RawInputManager.h`**:
*   Dependencies: Only `<JuceHeader.h>` and `<functional>`.
*   Method: `void initialize(void* nativeWindowHandle);` (Note the `void*`).
*   Method: `static juce::String getKeyName(int virtualKey);`

**`RawInputManager.cpp`**:
*   Includes: `#include "RawInputManager.h"` **FIRST**.
*   Includes: `#include <windows.h>` **SECOND**.
*   Logic:
    *   Cast `void*` to `HWND`.
    *   Use `RegisterRawInputDevices` with `RIDEV_INPUTSINK` (Usage Page 1, Usage 6).
    *   Subclass the window using `SetWindowLongPtr` with `GWLP_WNDPROC`.
    *   Intercept `WM_INPUT` to read `GetRawInputData`.
    *   Extract `hDevice`, `VKey`, and `Flags`.
    *   **Always** call `CallWindowProc` to forward messages to the original JUCE WndProc.

### Step 3: `MainComponent` (The UI)
Implement a simple logger.

**`MainComponent.cpp`**:
*   Includes: `#include "MainComponent.h"` **FIRST**.
*   **Forbidden:** Do NOT include `<windows.h>` here.
*   Logic:
    *   In `timerCallback`, wait for `getPeer()` to return valid.
    *   Get the native handle: `peer->getNativeHandle()`.
    *   Pass it to `RawInputManager`.
    *   Log key events to a `juce::TextEditor`.
    *   Use `juce::MessageManager::callAsync` for thread-safe UI updates.

