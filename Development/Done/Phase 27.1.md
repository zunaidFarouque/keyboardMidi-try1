# 🤖 Cursor Prompt: Phase 27.1 - Stable Global Input Gate (No Passthrough)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "OmniKey".
*   **Current State:** The input system has become overly complex with "Injection" logic causing loops and instability.
*   **Goal:** Revert to a clean, stable **Global Mode**.
    *   **Midi Mode ON:** ALL keyboards are blocked from Windows (No Typing). MIDI works globally (Foreground & Background).
    *   **Midi Mode OFF:** ALL keyboards work normally. No MIDI.
*   **Files to Cleanup:** Remove any usage of `InputInjector`. Remove `dwExtraInfo` checks.

**Strict Constraints:**
1.  **Hook Logic (`SystemIntegrator`):** If Mode is ON, return `1` (Block) for everything *except* the Toggle Key (Scroll Lock). Do NOT check for injection signatures.
2.  **Raw Input Logic (`RawInputManager`):**
    *   Remove all `InputInjector::injectKey` calls.
    *   **CRITICAL:** You MUST use the `RIDEV_INPUTSINK` flag in `RegisterRawInputDevices` so MIDI works when the app is minimized/unfocused.
    *   **CRITICAL:** Ensure the `hwndTarget` is the **Top Level Window Handle**, otherwise `INPUTSINK` fails.

---

### Step 1: Update `SystemIntegrator.cpp` (Simplified Hook)
Restore the clean blocking logic.

**Refactor `LowLevelKeyboardProc`:**
```cpp
KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

// 1. Always allow the Toggle Key (so we can turn it off)
if (p->vkCode == allowedKey)
    return CallNextHookEx(hHook, nCode, wParam, lParam);

// 2. If MIDI Mode is Active, BLOCK EVERYTHING.
// (No passthrough, no injection checks).
return 1; 
```

### Step 2: Update `RawInputManager.cpp` (Robust Listener)
Clean up the processing loop and fix the background registration.

**1. Refactor `initialize`:**
*   Accept `void* nativeWindowHandle`.
*   Cast to `HWND`.
*   **Flags:** `rid[0].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;`
*   **Target:** `rid[0].hwndTarget = hwnd;`

**2. Refactor `rawInputWndProc`:**
*   Remove `GetMessageExtraInfo` checks.
*   Remove `hDevice == 0` checks.
*   **Logic:**
    *   If `settings.isMidiModeActive()`:
    *   Broadcast to Listeners (MIDI).
    *   (Do nothing else. The Hook handles the blocking).

### Step 3: Update `MainComponent.cpp`
Ensure we pass the correct Window Handle.

**Update Constructor/Timer:**
*   When calling `rawInputManager->initialize(...)`, ensure you get the **Real OS Handle**:
    ```cpp
    if (auto* top = getTopLevelComponent())
        if (auto* peer = top->getPeer())
             rawInputManager->initialize(peer->getNativeHandle());
    ```

---

**Generate code for: `SystemIntegrator.cpp`, `RawInputManager.cpp`, and `MainComponent.cpp` (initialization logic only).**