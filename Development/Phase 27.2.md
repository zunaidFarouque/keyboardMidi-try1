# 🤖 Cursor Prompt: Phase 27.2 - Fix Background Input (InputSink)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 27.1 Complete. The Global Hook blocks keys correctly.
*   **The Bug:** MIDI generation stops when the OmniKey window loses focus (e.g., clicking into a DAW).
*   **The Cause:** `RegisterRawInputDevices` is likely missing the `RIDEV_INPUTSINK` flag, or `hwndTarget` is not set correctly. Without `INPUTSINK`, Windows only sends `WM_INPUT` to the foreground window.

**Phase Goal:** Force the application to receive Raw Input globally, even when minimized or in the background.

**Strict Constraints:**
1.  **Flags:** You MUST use `RIDEV_INPUTSINK`.
2.  **Handle:** You MUST set `rid.hwndTarget` to the valid `HWND` passed to `initialize`. Do not leave it NULL.
3.  **Initialization:** Ensure `MainComponent` passes the **Top Level Peer** handle, not a child component handle.

---

### Step 1: Update `RawInputManager.cpp`
Fix the registration struct.

**Refactor `initialize(void* nativeWindowHandle)`:**
```cpp
void RawInputManager::initialize(void* nativeWindowHandle)
{
    if (isInitialized || nativeWindowHandle == nullptr) return;

    HWND hwnd = static_cast<HWND>(nativeWindowHandle);
    targetHwnd = nativeWindowHandle;

    // RAW INPUT REGISTRATION
    RAWINPUTDEVICE rid[1];
    
    // Page 1, Usage 6 = Keyboard
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    
    // CRITICAL FIX: RIDEV_INPUTSINK enables background monitoring.
    // We also keep RIDEV_DEVNOTIFY to handle plug/unplug events.
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    
    // CRITICAL FIX: Target must be explicit for InputSink to work.
    rid[0].hwndTarget = hwnd;

    if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE)
    {
        // GetLastError() could be logged here for debugging
        DBG("RawInputManager: Failed to register raw input device.");
        return;
    }

    // Subclassing logic remains the same...
    originalWndProc = (void*)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);

    if (originalWndProc)
    {
        isInitialized = true;
        DBG("RawInputManager: Initialized with RIDEV_INPUTSINK.");
    }
}
```

### Step 2: Verification in `MainComponent.cpp`
Ensure the handle passed is correct.

**Update Constructor:**
*   Check the logic where `rawInputManager->initialize` is called.
*   It should look like this:
    ```cpp
    // In timerCallback or after window is visible
    if (auto* top = getTopLevelComponent())
    {
        if (auto* peer = top->getPeer())
        {
            rawInputManager->initialize(peer->getNativeHandle());
            // ... stop timer/flag init ...
        }
    }
    ```

---

**Generate code for: Updated `RawInputManager.cpp` and `MainComponent.cpp`.**