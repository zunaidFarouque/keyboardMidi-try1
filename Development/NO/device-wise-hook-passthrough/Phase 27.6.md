# 🤖 Cursor Prompt: Phase 27.6 - Dedicated Input Window & Hook Hardening

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 27.5 Complete.
*   **Critical Issues:**
    1.  **Background Death:** Raw Input stops when the main window loses focus (likely due to Hook/Sink conflict on the GUI window handle).
    2.  **Double Typing:** In Passthrough mode, keys type twice (Hook leak or Injection loop).

**Phase Goal:**
1.  Implement a **Message-Only Window** class to act as a robust `RIDEV_INPUTSINK` target.
2.  Refine the Hook logic to ensure it blocks reliably.

**Strict Constraints:**
1.  **Dedicated HWND:** Do not use `getPeer()->getNativeHandle()`. Create a hidden Win32 window using `CreateWindowEx(0, L"Message", ... HWND_MESSAGE ...)`.
2.  **Message Loop:** This window needs a `WndProc` to process `WM_INPUT`.
3.  **Lifecycle:** The window must be destroyed on shutdown.

---

### Step 1: `InputWindow.h/cpp` (New Class)
A dedicated hidden window for Raw Input.

**Requirements:**
1.  **Members:** `HWND hwnd = nullptr;`.
2.  **Constructor:**
    *   Register a custom Window Class (`WNDCLASSEX`).
    *   Call `CreateWindowEx` with `HWND_MESSAGE` parent.
    *   Store `this` pointer in `GWLP_USERDATA` to route messages to the C++ instance.
3.  **Method:** `HWND getHandle() { return hwnd; }`
4.  **Static Callback:** `static LRESULT CALLBACK WndProc(...)`
    *   Retrieve `this` from UserData.
    *   Call instance `handleMessage`.
5.  **Instance Method:** `handleMessage(uMsg, wParam, lParam)`
    *   If `uMsg == WM_INPUT`: Forward to `RawInputManager::getInstance()->processRawInput(...)`. (You might need to make `RawInputManager` a singleton or pass a callback).
    *   *Correction:* Pass a `std::function` callback to `InputWindow` constructor to link it back to `RawInputManager`.

### Step 2: Update `RawInputManager`
Refactor to use `InputWindow`.

**Updates:**
1.  **Members:** `std::unique_ptr<InputWindow> inputWindow;`
2.  **Initialize:**
    *   Remove `void* nativeHandle` argument.
    *   Create `inputWindow = std::make_unique<InputWindow>([this](...){ rawInputWndProc(...); });`
    *   Use `inputWindow->getHandle()` for `RegisterRawInputDevices`.
    *   Ensure `RIDEV_INPUTSINK` is set.

### Step 3: Update `RawInputManager.cpp` (Logic Cleanup)
Fix the Double Typing logic.

**Refactor `rawInputWndProc`:**
*   **Injection Check:** `if (raw->header.hDevice == 0) return 0;` (Keep this!).
*   **Mode Logic:**
    *   `if (MidiMode)`:
        *   `if (Assigned)`: Broadcast MIDI. **Do NOT Inject.**
        *   `if (Passthrough)`: **Inject.** (This generates a new event with `hDevice=0`, which gets caught by the check above).
        *   `if (Unassigned)`: **Inject.**

### Step 4: Update `MainComponent`
*   Remove the `initialize(hwnd)` call from the constructor/timer. `RawInputManager` should now initialize itself (creating the hidden window) in its own constructor or a parameterless `initialize()`.

---

**Generate code for: `InputWindow.h`, `InputWindow.cpp`, updated `RawInputManager.h/cpp`, and updated `MainComponent.cpp`.**