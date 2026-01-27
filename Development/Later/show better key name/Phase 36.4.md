# 🤖 Cursor Prompt: Phase 36.4 - High-Precision Key Repair

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Phase 36.3 applied.
*   **The Bug:**
    *   NumLock (Scan 0x45) is identifying as Pause.
    *   Print Screen (Scan 0x37) is identifying as Shift.
*   **Cause:** `MapVirtualKey` is unreliable for these specific keys or the flag logic is missing.
*   **Phase Goal:** Update `RawInputManager::rawInputWndProc` to handle `RI_KEY_E0` and `RI_KEY_E1` flags explicitly to distinguish these keys.

**Strict Constraints:**
1.  **Pause/Break:** If `(flags & RI_KEY_E1)`, force `vKey = VK_PAUSE`.
2.  **Print Screen:** If `makeCode == 0x37` AND `(flags & RI_KEY_E0)`, force `vKey = VK_SNAPSHOT`.
3.  **Num Lock:** If `makeCode == 0x45` AND `!(flags & RI_KEY_E1)`, force `vKey = VK_NUMLOCK`.
4.  **Repair Condition:** Only apply this logic if `raw->data.keyboard.VKey` is `0` or `0xFF`.

---

### Step 1: Update `Source/RawInputManager.cpp`

Refactor the **VKEY REPAIR** block inside `rawInputWndProc`.

```cpp
// ... extract raw data ...
UINT vKey = raw->data.keyboard.VKey;
UINT makeCode = raw->data.keyboard.MakeCode;
UINT flags = raw->data.keyboard.Flags;

// --- VKEY REPAIR START ---
if (vKey == 0 || vKey == 0xFF)
{
    // 1. Check Flags for Special Keys
    if (flags & RI_KEY_E1) 
    {
        // Only Pause/Break uses E1
        vKey = VK_PAUSE; 
    }
    else if (makeCode == 0x37 && (flags & RI_KEY_E0))
    {
        // Print Screen (E0 + 37)
        vKey = VK_SNAPSHOT;
    }
    else if (makeCode == 0x45)
    {
        // Num Lock (Scan 45, No E1)
        vKey = VK_NUMLOCK;
    }
    else
    {
        // 2. Fallback to Windows Mapping
        // MAPVK_VSC_TO_VK_EX distinguishes Left/Right Shift/Ctrl/Alt
        vKey = MapVirtualKey(makeCode, MAPVK_VSC_TO_VK_EX);
        
        if (vKey == 0) vKey = MapVirtualKey(makeCode, MAPVK_VSC_TO_VK);
    }
}
// --- VKEY REPAIR END ---

// ... proceed with existing repeat filtering ...
```

---

**Generate code for: Updated `Source/RawInputManager.cpp`.**