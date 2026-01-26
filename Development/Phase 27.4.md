# 🤖 Cursor Prompt: Phase 27.4 - Focus Guard (The Input Magnet)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "OmniKey".
*   **Current State:** The "Hook" method (Phase 27.1) prevents device distinction. The "Driver" method is too invasive.
*   **Solution:** We will use a **Focus Stealing** strategy.
*   **Phase Goal:**
    1.  Remove the Low-Level Hook (`SystemIntegrator`) entirely.
    2.  Update `RawInputManager` to force the OmniKey window to the foreground whenever a key is pressed while "Midi Mode" is Active.

**Strict Constraints:**
1.  **Cleanup:** Remove all `SystemIntegrator` code from `MainComponent`.
2.  **Raw Input:** Ensure `RIDEV_INPUTSINK` is set so we detect the trigger key even when backgrounded.
3.  **Focus Logic:** In `rawInputWndProc`, if `Settings::isMidiModeActive()` is true and the window is NOT foreground, call `SetForegroundWindow` (and `ShowWindow` if minimized).

---

### Step 1: Delete `SystemIntegrator`
*   Delete `Source/SystemIntegrator.h` and `.cpp`.
*   Remove from `CMakeLists.txt`.

### Step 2: Update `RawInputManager.cpp`
Implement the Focus Guard.

**1. `initialize`:**
*   Ensure `rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;`
*   Ensure `rid[0].hwndTarget = hwnd;`

**2. `rawInputWndProc`:**
*   Inside `WM_INPUT`:
    *   Check `if (SettingsManager::getInstance()->isMidiModeActive())` (You may need to pass a reference to `SettingsManager` into `RawInputManager` or make the settings static/singleton for easy access from the WNDPROC).
    *   **Check Foreground:**
        ```cpp
        HWND currentForeground = GetForegroundWindow();
        if (currentForeground != hwndTarget)
        {
            // Force Restore if minimized
            if (IsIconic(hwndTarget)) 
                ShowWindow(hwndTarget, SW_RESTORE);
            
            // Steal Focus
            SetForegroundWindow(hwndTarget);
        }
        ```
    *   **Process:** Continue to broadcast the event to `listeners`.

### Step 3: Update `MainComponent.cpp`
Cleanup and Wiring.

**1. Constructor:**
*   Remove `SystemIntegrator` calls.
*   Ensure `rawInputManager` has access to `SettingsManager` (pass it in constructor).

**2. Destructor:**
*   Remove `SystemIntegrator` cleanup.

---

**Generate code for: Updated `RawInputManager.h/cpp` (Focus Logic & Settings Dependency), Updated `MainComponent.cpp` (Cleanup), and Updated `CMakeLists.txt` (Remove file).**