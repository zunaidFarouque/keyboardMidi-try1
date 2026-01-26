# 🤖 Cursor Prompt: Phase 27.3 - Background Input Diagnostics

**Role:** Expert C++ Audio Developer (Win32 API Debugging).

**Context:**
*   **Problem:** Background MIDI is not working. We suspect `RIDEV_INPUTSINK` is failing or the message loop is blocked.
*   **Goal:** Add verbose Debug Logging to `RawInputManager` to diagnose the Win32 API calls.

**Strict Constraints:**
1.  **Log Everything:** Log the HWND address, the Register return value, and `GetLastError()` if it fails.
2.  **Message Inspection:** In `rawInputWndProc`, log the `wParam` value.
    *   `0` (`RIM_INPUT`) = Foreground (Normal).
    *   `1` (`RIM_INPUTSINK`) = Background (What we need).
3.  **Output:** Use `DBG()` so it appears in the IDE output, AND call `MainComponent::logEvent` (if accessible) or just use `DBG` if `MainComponent` is hard to reach from the static proc. *Actually, stick to `DBG` for the static proc to avoid crash risks, we will view it in the Output panel.*

---

### Step 1: Update `RawInputManager.cpp`

**Refactor `initialize`:**
```cpp
void RawInputManager::initialize(void* nativeWindowHandle)
{
    if (isInitialized || nativeWindowHandle == nullptr) return;

    HWND hwnd = static_cast<HWND>(nativeWindowHandle);
    targetHwnd = nativeWindowHandle;

    // Log the Handle
    DBG("RawInputManager: Initializing with HWND: " + String::toHexString((int64)hwnd));

    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = 0x01;
    rid[0].usUsage = 0x06;
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
    rid[0].hwndTarget = hwnd;

    if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE)
    {
        DWORD error = GetLastError();
        DBG("RawInputManager: CRITICAL ERROR - Registration Failed! Error Code: " + String(error));
    }
    else
    {
        DBG("RawInputManager: Registration Success. Flags: RIDEV_INPUTSINK");
    }

    // Subclass...
    originalWndProc = (void*)SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);
    
    if (originalWndProc) isInitialized = true;
}
```

**Refactor `rawInputWndProc`:**
Add this trace at the very top of the `WM_INPUT` case:

```cpp
// Inside rawInputWndProc switch(msg)...
case WM_INPUT:
{
    // DIAGNOSTIC LOG
    // GET_RAWINPUT_CODE_WPARAM is a macro to extract the input type
    int code = GET_RAWINPUT_CODE_WPARAM(wParam);
    String typeStr = (code == RIM_INPUT) ? "FOREGROUND" : (code == RIM_INPUTSINK ? "BACKGROUND" : "OTHER");
    
    // Only log occasionally or on specific keys to avoid flooding, 
    // OR just log everything for this short debug phase.
    // Let's log if it is BACKGROUND to prove it works.
    if (code == RIM_INPUTSINK)
    {
        DBG("RawInputManager: Received BACKGROUND Event!");
    }
    
    // ... rest of existing logic ...
}
```

---

**Generate code for: Updated `Source/RawInputManager.cpp`.**