# 🤖 Cursor Prompt: Phase 36.5 - Disable VKey Repair (Temporary Revert)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** The VKey Repair logic (Phase 36.3/36.4) is causing regressions (misidentifying NumLock as Pause, etc.).
*   **Goal:** Comment out the repair logic in `RawInputManager` to restore stable behavior. Re-enable the "Fn / Hardware" label in `KeyNameUtilities` for unmapped keys.

**Strict Constraints:**
1.  **Method:** Do not delete the code. Wrap the "VKey Repair" block in `RawInputManager.cpp` with `/* ... */` comments so we can fix it later.
2.  **UI:** In `KeyNameUtilities.cpp`, restore the `case 0xFF:` handler.

---

### Step 1: Update `Source/RawInputManager.cpp`
Disable the repair logic.

**Action:** Find the block between `// --- VKEY REPAIR START ---` and `// --- VKEY REPAIR END ---`.
**Update:**
```cpp
// ... extract raw data ...
UINT vKey = raw->data.keyboard.VKey;
UINT makeCode = raw->data.keyboard.MakeCode;
UINT flags = raw->data.keyboard.Flags;

// --- VKEY REPAIR START ---
/* TEMPORARILY DISABLED FOR STABILITY
if (vKey == 0 || vKey == 0xFF)
{
    // ... (Existing Logic) ...
}
*/
// --- VKEY REPAIR END ---
```

### Step 2: Update `Source/KeyNameUtilities.cpp`
Restore the generic name.

**Action:** Add the `0xFF` case back into the switch statement.
```cpp
switch (virtualKeyCode)
{
    // ... existing cases ...
    
    // Restore this:
    case 0xFF:          return "Fn / Hardware";
}
```

---

**Generate code for: Updated `Source/RawInputManager.cpp` and `Source/KeyNameUtilities.cpp`.**