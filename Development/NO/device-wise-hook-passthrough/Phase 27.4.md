# 🤖 Cursor Prompt: Phase 27.4 - Robust Input Handling & Loop Prevention

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 27.3 Complete.
*   **Bug 1:** "Double Typing" (e.g., 'e' -> 'ee'). The `GetMessageExtraInfo` check is failing to detect our own injected keystrokes in the Raw Input loop.
*   **Bug 2:** "Background Blindness". Input stops working when the window loses focus, despite `RIDEV_INPUTSINK`.

**Phase Goal:**
1.  Implement a robust filter for Injected Keys using `hDevice == 0`.
2.  Add safety checks to `RegisterRawInputDevices`.

**Strict Constraints:**
1.  **Loop Prevention:** In `rawInputWndProc`, check `raw->header.hDevice`. If it is `0` (NULL), **Return Immediately**. This is the standard way to detect `SendInput` events in Raw Input.
2.  **Registration:** Wrap `RegisterRawInputDevices` in a check: `if (IsWindow(hwnd))`. If the handle is invalid, `RIDEV_INPUTSINK` fails silently. Log an error if registration fails.

---

### Step 1: Update `RawInputManager.cpp`

**1. Update `rawInputWndProc` (Loop Fix):**
Move the filter logic inside the `WM_INPUT` block where we have access to the header.

```cpp
int64_t __stdcall RawInputManager::rawInputWndProc(void* hwnd, unsigned int msg, uint64_t wParam, int64_t lParam)
{
    if (msg == WM_INPUT)
    {
        UINT dwSize = 0;
        GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
        
        if (dwSize > 0)
        {
            // Allocate on stack for speed if size is reasonable, or heap
            auto* lpb = new BYTE[dwSize];
            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
            {
                auto* raw = (RAWINPUT*)lpb;

                // --- CRITICAL FIX: Loop Prevention ---
                // Injected keys (SendInput) usually have hDevice = 0.
                // We MUST ignore these to prevent infinite loops (Double Typing).
                if (raw->header.hDevice == 0)
                {
                    delete[] lpb;
                    // Let the default proc handle it (so the injected key reaches the app focus if needed)
                    return CallWindowProc((WNDPROC)originalWndProc, (HWND)hwnd, msg, wParam, lParam); 
                }

                // ... Rest of your existing logic (Keyboard/Mouse handling) ...
                
                // Existing Logic for Passthrough:
                // if (settings.isMidiModeActive()) {
                //    if (passthrough) InputInjector::injectKey(...); // This triggers the loop we just prevented above
                // }
            }
            delete[] lpb;
        }
    }
    return CallWindowProc((WNDPROC)originalWndProc, (HWND)hwnd, msg, wParam, lParam);
}
```

**2. Update `initialize` (Background Fix):**
Ensure we are calling it correctly.

```cpp
void RawInputManager::initialize(void* nativeWindowHandle)
{
    if (isInitialized || nativeWindowHandle == nullptr) return;

    HWND hwnd = static_cast<HWND>(nativeWindowHandle);
    
    // Safety check
    if (!IsWindow(hwnd))
    {
        DBG("RawInputManager Error: Invalid Window Handle provided.");
        return;
    }

    targetHwnd = nativeWindowHandle;

    RAWINPUTDEVICE rid[1];
    rid[0].usUsagePage = 0x01; 
    rid[0].usUsage = 0x06;     
    rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY; // Added DEVNOTIFY for robustness
    rid[0].hwndTarget = hwnd;

    if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE)
    {
        // Get Error Code
        DWORD error = GetLastError();
        DBG("RawInputManager Failure: RegisterRawInputDevices failed with code: " + juce::String(error));
        return;
    }
    
    // ... Subclass logic ...
}
```

---

**Generate code for: Updated `RawInputManager.cpp`.**