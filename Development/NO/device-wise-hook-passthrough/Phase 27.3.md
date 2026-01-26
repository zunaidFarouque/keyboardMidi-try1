# 🤖 Cursor Prompt: Phase 27.3 - Fix Background Input & Injection Loop

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 27.2 Complete.
*   **Bug 1 (Background):** MIDI/Typing stops working when the app loses focus. This is because `RegisterRawInputDevices` is missing the `RIDEV_INPUTSINK` flag.
*   **Bug 2 (Double Typing):** When Passthrough is active, typing 'e' results in 'ee'. The `RawInputManager` is likely reacting to its own Injected event and re-injecting it.

**Phase Goal:**
1.  Enable Global Background Input support.
2.  Harden the "Self-Injection" detection to prevent loops.

**Strict Constraints:**
1.  **Flags:** Use `RIDEV_INPUTSINK` (0x00000100) when registering raw input. This requires `hwndTarget` to be set.
2.  **Loop Prevention:** In `rawInputWndProc`, check `GetMessageExtraInfo()` **immediately**. If it matches `0xFFC0FFEE`, return 0. Do not process. Do not pass Go.
3.  **Hook Logic:** Ensure `SystemIntegrator` correctly passes events with the magic signature.

---

### Step 1: Update `RawInputManager.cpp`
Fix the Registration and Loop Detection.

**1. Update `initialize`:**
```cpp
// ... inside initialize ...
RAWINPUTDEVICE rid[1];
rid[0].usUsagePage = 0x01; 
rid[0].usUsage = 0x06;     
// CRITICAL FIX: Allow background input
rid[0].dwFlags = RIDEV_INPUTSINK; 
rid[0].hwndTarget = hwnd; // Must be set for INPUTSINK to work

if (RegisterRawInputDevices(rid, 1, sizeof(rid[0])) == FALSE) {
    // Log error
}
```

**2. Update `rawInputWndProc`:**
Move the magic number check to the **very top**, before parsing `lParam`.

```cpp
int64_t __stdcall RawInputManager::rawInputWndProc(void* hwnd, unsigned int msg, uint64_t wParam, int64_t lParam)
{
    // CRITICAL FIX: Ignore our own injected keystrokes immediately
    // 0xFFC0FFEE is the signature set by InputInjector
    if (GetMessageExtraInfo() == 0xFFC0FFEE)
    {
        return CallWindowProc((WNDPROC)originalWndProc, (HWND)hwnd, msg, wParam, lParam);
    }

    if (msg == WM_INPUT)
    {
        // ... Existing Logic ...
    }
    
    // ... Existing CallWindowProc ...
}
```

### Step 2: Verify `SystemIntegrator.cpp`
Ensure the Hook allows the signature.

**Check `LowLevelKeyboardProc`:**
```cpp
KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
// If the key has our magic tattoo, let it pass (it's the passthrough key)
if (p->dwExtraInfo == 0xFFC0FFEE)
{
    return CallNextHookEx(hHook, nCode, wParam, lParam);
}
```

---

**Generate code for: Updated `RawInputManager.cpp` and `SystemIntegrator.cpp`.**