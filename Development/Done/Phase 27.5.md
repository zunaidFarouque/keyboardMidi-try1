# 🤖 Cursor Prompt: Phase 27.5 - Force Focus (Bypassing Windows Restrictions)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 27.4 implemented. `RawInputManager` attempts to call `SetForegroundWindow`, but Windows blocks it (Foreground Lock Timeout).
*   **Problem:** The window does not come to the front when a key is pressed in the background. Typing continues in the other app.
*   **Phase Goal:** Implement a robust `forceForegroundWindow` helper using `AttachThreadInput` to bypass the OS restriction.

**Strict Constraints:**
1.  **Mechanism:** You must use `AttachThreadInput` to attach the OmniKey thread to the current Foreground Window's thread before attempting to switch focus.
2.  **Safety:** Always detach (`AttachThreadInput(..., FALSE)`) immediately after the switch.
3.  **Restore:** Handle the `IsIconic` (Minimized) case explicitly with `ShowWindow(SW_RESTORE)`.

---

### Step 1: Update `RawInputManager.cpp`

**1. Add Helper Function (Internal):**
```cpp
void forceForegroundWindow(HWND hwnd)
{
    if (GetForegroundWindow() == hwnd) return;

    DWORD foregroundThread = GetWindowThreadProcessId(GetForegroundWindow(), NULL);
    DWORD appThread = GetCurrentThreadId();

    // 1. Restore if minimized
    if (IsIconic(hwnd))
    {
        ShowWindow(hwnd, SW_RESTORE);
    }

    // 2. Attach threads to bypass focus lock
    if (foregroundThread != appThread)
    {
        AttachThreadInput(foregroundThread, appThread, TRUE);
        
        // Trick: Sometimes sending a dummy ALT key helps unlock the state
        // keybd_event(VK_MENU, 0, 0, 0);
        // keybd_event(VK_MENU, 0, KEYEVENTF_KEYUP, 0);
        
        SetForegroundWindow(hwnd);
        SetFocus(hwnd); // Ensure keyboard focus too
        
        AttachThreadInput(foregroundThread, appThread, FALSE);
    }
    else
    {
        SetForegroundWindow(hwnd);
    }
}
```

**2. Update `rawInputWndProc`:**
*   Replace the simple `SetForegroundWindow` call with `forceForegroundWindow(hwndTarget)`.
*   **Logic Check:**
    *   Inside `WM_INPUT`:
    *   If `SettingsManager::isMidiModeActive()`:
        *   `forceForegroundWindow(targetHwnd);`
        *   (Then continue to process MIDI).

---

**Generate code for: Updated `RawInputManager.cpp`.**