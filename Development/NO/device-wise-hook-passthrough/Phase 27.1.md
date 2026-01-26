# 🤖 Cursor Prompt: Phase 27.1 - Selective Input Passthrough (Injection)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 27 Complete. MIDI Mode blocks ALL keys using a Low-Level Hook.
*   **Problem:** Users want unassigned keyboards (e.g., Laptop keyboard) to work normally for typing, even while a secondary MIDI keyboard is intercepted.
*   **Phase Goal:** Implement a **Block-And-Reinject** architecture.
    *   The Hook blocks everything by default.
    *   `RawInputManager` checks if a device is "Unassigned".
    *   If Unassigned, it re-injects the keystroke with a specific "Magic Signature".
    *   The Hook sees the Signature and lets the key pass.

**Strict Constraints:**
1.  **Magic Number:** Use `0xFFC0FFEE` as `dwExtraInfo`.
2.  **Infinite Loop Prevention:** `RawInputManager` MUST check `GetMessageExtraInfo()` and ignore events matching the magic number.
3.  **Safety:** The Toggle Key (Scroll Lock) logic from Phase 27 must remain (it is always allowed by Hook, never re-injected).

---

### Step 1: `InputInjector` (`Source/InputInjector.h/cpp`)
Static helper for `SendInput`.

**Requirements:**
1.  Include `<windows.h>`.
2.  `static void injectKey(int virtualKey, int scanCode, bool isDown, bool isExtended);`
    *   Construct `INPUT` struct.
    *   Set `type = INPUT_KEYBOARD`.
    *   Set `wVk`, `wScan`.
    *   Set `dwFlags`:
        *   `KEYEVENTF_SCANCODE`.
        *   If `!isDown` add `KEYEVENTF_KEYUP`.
        *   If `isExtended` add `KEYEVENTF_EXTENDEDKEY`.
    *   **Crucial:** Set `dwExtraInfo = 0xFFC0FFEE;`
    *   Call `SendInput`.

### Step 2: Update `SystemIntegrator.cpp` (The Hook)
Allow the injected keys.

**Update `LowLevelKeyboardProc`:**
```cpp
// Check Signature
KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;
if (p->dwExtraInfo == 0xFFC0FFEE) 
    return CallNextHookEx(hHook, nCode, wParam, lParam);

// ... existing Toggle Key check ...
// ... existing Block logic ...
```

### Step 3: Update `DeviceManager`
Add the lookup.

**Method:** `bool isHardwareAssigned(uintptr_t hardwareHash);`
*   Iterate `hardwareToAliasMap`. Return true if hash is found as a Key (or inside the value vector, depending on your Phase 9 implementation structure).

### Step 4: Update `RawInputManager.cpp`
Implement the routing.

**1. Update `rawInputWndProc`:**
*   **Top Check:**
    ```cpp
    if (GetMessageExtraInfo() == 0xFFC0FFEE) return 0; // Ignore our own injection
    ```
*   **Logic inside `WM_INPUT`:**
    *   Extract `flags` from `raw->data.keyboard`.
    *   `bool isE0 = (flags & RI_KEY_E0) != 0;`
    *   **Mode Check:**
        *   If `SettingsManager::isMidiModeActive()`:
            *   Check `DeviceManager::isHardwareAssigned(device)`.
            *   **If Assigned:** Proceed to Broadcast (MIDI).
            *   **If Unassigned:**
                *   Check if `vKey == ToggleKey`. If yes, Ignore (let Hook handle/allow it).
                *   Else: `InputInjector::injectKey(vKey, makeCode, isDown, isE0);`
        *   Else (Mode Off): Proceed to Broadcast/Ignore (Standard behavior).

---

**Generate code for: `InputInjector`, Updated `SystemIntegrator`, Updated `DeviceManager`, and Updated `RawInputManager`.**