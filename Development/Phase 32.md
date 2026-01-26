# 🤖 Cursor Prompt: Phase 32 - Fix Shutdown Crash (Zombie WNDPROC)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** The app works but crashes with `_CrtIsValidHeapPointer` on exit.
*   **Diagnosis:** The `RawInputManager` is destroyed by `MainComponent`, but the Windows OS continues to send messages (`WM_DESTROY`, etc.) to the static `rawInputWndProc`. The static function tries to access the deleted instance, causing a heap corruption error.

**Phase Goal:** Hard-harden the `RawInputManager` destruction sequence to prevent the static WNDPROC from accessing dead memory.

**Strict Constraints:**
1.  **Static Pointer Safety:** Ensure `rawInputWndProc` checks `if (globalInstance == nullptr)` at the very top. If null, it must call `CallWindowProc` using the stored `originalWndProc` (if valid) or `DefWindowProc`.
2.  **Destructor Order:** In `~RawInputManager()`:
    *   **First:** Set `globalInstance = nullptr`. (Stops the static hook from accessing member variables immediately).
    *   **Second:** Call `SetWindowLongPtr` to restore the `originalWndProc`.
3.  **Variable Storage:** Since `originalWndProc` is needed inside the static function *even after the class instance dies* (to pass the message along), `originalWndProc` itself should likely be **static** or carefully managed.
    *   *Better Approach:* If `globalInstance` is null, we might not be able to access `originalWndProc` if it's a member variable.
    *   *Solution:* Make `originalWndProc` a **static** variable in `.cpp` (file scope) so it survives the class destruction for that final split second.

---

### Step 1: Update `Source/RawInputManager.cpp`

**Refactor Global Variables:**
Move these out of the class or make them static file-scope variables at the top of the cpp file:
```cpp
static RawInputManager* globalManagerInstance = nullptr;
static WNDPROC globalOriginalWndProc = nullptr; // Store the old proc here
```

**Refactor `initialize`:**
```cpp
void RawInputManager::initialize(void* nativeWindowHandle)
{
    // ... existing setup ...
    
    // Store global pointers
    globalManagerInstance = this;
    
    // Subclass
    LONG_PTR oldProc = SetWindowLongPtr(hwnd, GWLP_WNDPROC, (LONG_PTR)rawInputWndProc);
    globalOriginalWndProc = (WNDPROC)oldProc;
    
    // ...
}
```

**Refactor `rawInputWndProc` (The Crash Fix):**
```cpp
LRESULT CALLBACK RawInputManager::rawInputWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 1. SAFETY CHECK: If the class is dead, just pass through.
    if (globalManagerInstance == nullptr)
    {
        if (globalOriginalWndProc)
            return CallWindowProc(globalOriginalWndProc, hwnd, msg, wParam, lParam);
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // 2. Normal Processing
    // ... existing switch(msg) logic ...
    // Note: Use 'globalManagerInstance->' instead of 'this->' or implicit members
    
    // 3. Pass to original
    return CallWindowProc(globalOriginalWndProc, hwnd, msg, wParam, lParam);
}
```

**Refactor Destructor (`~RawInputManager`):**
```cpp
RawInputManager::~RawInputManager()
{
    // 1. CUT THE CORD IMMEDIATELY
    // This prevents the static proc from touching 'this' ever again.
    globalManagerInstance = nullptr;

    // 2. Restore the window (Unsubclass)
    if (targetHwnd && globalOriginalWndProc)
    {
        SetWindowLongPtr((HWND)targetHwnd, GWLP_WNDPROC, (LONG_PTR)globalOriginalWndProc);
    }
    
    // 3. Clean up static state
    // globalOriginalWndProc = nullptr; // Optional, maybe keep it safe in case a lagging message hits
}
```

---

**Generate code for: Updated `Source/RawInputManager.cpp`.**