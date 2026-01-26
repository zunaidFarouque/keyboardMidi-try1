# 🤖 Cursor Prompt: Phase 27.5 - System Integration Repair

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 27.4 Complete.
*   **Bug 1 (Double Typing):** When Passthrough is active, keys type twice. The Hook is not blocking the physical key.
*   **Bug 2 (Blindness):** `WM_INPUT` does not fire when the app is in the background.
*   **Bug 3 (External Dead):** Injected keys don't appear in other apps.

**Phase Goal:** Ensure robust Blocking (Hook), robust Listening (Raw Input), and robust Injection.

**Strict Constraints:**
1.  **Window Handle:** `RawInputManager` must be initialized with the **Top Level** HWND.
2.  **Hook Logic:** Ensure strict blocking. If `MidiMode` is ON, and `dwExtraInfo` is NOT Magic, and Key is NOT Toggle -> **Block**.
3.  **Injector:** Populate `wVk` correctly in `SendInput`.

---

### Step 1: Update `MainComponent.cpp`
Pass the correct handle.

**Update `timerCallback` (Initialization Logic):**
*   **Old:** `rawInputManager->initialize(getPeer()->getNativeHandle());`
*   **New:**
    ```cpp
    if (auto* topLevel = getTopLevelComponent())
    {
        if (auto* peer = topLevel->getPeer())
        {
            rawInputManager->initialize(peer->getNativeHandle());
            // ... logs ...
        }
    }
    ```

### Step 2: Update `SystemIntegrator.cpp`
Harden the blocking logic.

**Update `LowLevelKeyboardProc`:**
```cpp
// 1. Cast
KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

// 2. Check Injection Signature (Allow our own keys)
// Cast to ULONG_PTR to ensure safety on 64-bit systems
if ((ULONG_PTR)p->dwExtraInfo == 0xFFC0FFEE) 
    return CallNextHookEx(hHook, nCode, wParam, lParam);

// 3. Check Toggle Key (Scroll Lock) - Always Allow
if (p->vkCode == allowedKey)
    return CallNextHookEx(hHook, nCode, wParam, lParam);

// 4. BLOCK EVERYTHING ELSE (If Mode is Active)
return 1; 
```

### Step 3: Update `InputInjector.cpp`
Ensure standard Windows compatibility.

**Update `injectKey`:**
```cpp
void InputInjector::injectKey(int virtualKey, int scanCode, bool isDown, bool isExtended)
{
    INPUT input = { 0 };
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = (WORD)virtualKey;       // CRITICAL: Set Virtual Key
    input.ki.wScan = (WORD)scanCode;       // Set Scan Code
    input.ki.dwFlags = 0;                  // Default to VK
    
    // If scanCode is available, use scancode flag for better compatibility with games, 
    // BUT for general typing, VK is safer. Let's use VK priority.
    // actually, let's strictly use the flags passed in, but ensure wVk is set.
    
    if (!isDown) input.ki.dwFlags |= KEYEVENTF_KEYUP;
    if (isExtended) input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    
    input.ki.time = 0;
    input.ki.dwExtraInfo = 0xFFC0FFEE; // The Magic Signature

    SendInput(1, &input, sizeof(INPUT));
}
```

### Step 4: Update `RawInputManager.cpp`
Verify the Passthrough Logic.

**Check `rawInputWndProc`:**
*   Ensure `DeviceManager::isHardwarePassthrough` returns correct values.
*   Ensure `InputInjector::injectKey` is called with the correct `raw->data.keyboard.VKey`.

---

**Generate code for: Updated `MainComponent.cpp`, `SystemIntegrator.cpp`, and `InputInjector.cpp`.**