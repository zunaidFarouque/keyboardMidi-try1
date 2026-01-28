Here is the prompt for **Phase 27.6**.

This "Unlocks" the specific modifier keys. By default, Windows Raw Input lazily reports "Shift" (0x10) for both keys. We will force it to do the work to distinguish Left (0xA0) from Right (0xA1), utilizing the Scan Code and the Extended Key Flag (`E0`).

Since we already added the "Generic Fallback" logic in `InputProcessor` (Phase 39.9), this update is safe: existing mappings for "Shift" will still work via fallback, but now you *can* map "Left Shift" specifically if you want.

***

# 🤖 Cursor Prompt: Phase 27.6 - Modifier & Extended Key Disambiguation

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "OmniKey".
*   **Current State:** `RawInputManager` receives key events but often reports generic Virtual Keys (`VK_SHIFT` 0x10, `VK_CONTROL` 0x11) instead of specific ones (`VK_LSHIFT`, `VK_RCONTROL`).
*   **Problem:** Users cannot map Left Shift and Right Shift to different functions.
*   **Phase Goal:** Update `rawInputWndProc` to refine the `VKey` using the Scan Code (`MakeCode`) and the Extended Flag (`RI_KEY_E0`).

**Strict Constraints:**
1.  **MapVirtualKey:** Use `MapVirtualKey(makeCode, MAPVK_VSC_TO_VK_EX)` to resolve L/R Shift.
2.  **Extended Flag (E0):** Use `(flags & RI_KEY_E0)` to distinguish Right Control, Right Alt, and Numpad Enter/Slash vs. Main Enter/Slash.
3.  **Scope:** Apply this fix inside the `WM_INPUT` handler in `RawInputManager.cpp` before broadcasting the event.

---

### Step 1: Update `Source/RawInputManager.cpp`

**Refactor `rawInputWndProc`:**

Locate the `RIM_TYPEKEYBOARD` block. Replace the key extraction logic with this robust version:

```cpp
// ... inside RIM_TYPEKEYBOARD ...
UINT vKey = raw->data.keyboard.VKey;
UINT makeCode = raw->data.keyboard.MakeCode;
UINT flags = raw->data.keyboard.Flags;
bool isDown = (flags & RI_KEY_BREAK) == 0;
bool isE0 = (flags & RI_KEY_E0) != 0;
bool isE1 = (flags & RI_KEY_E1) != 0;

// --- VKEY DISAMBIGUATION START ---

// 1. Map generic keys to specific Left/Right keys using Scan Code
if (vKey == VK_SHIFT || vKey == VK_CONTROL || vKey == VK_MENU)
{
    // Try to resolve L/R using Scan Code
    UINT extendedVKey = MapVirtualKeyA(makeCode, MAPVK_VSC_TO_VK_EX);
    if (extendedVKey != 0)
    {
        vKey = extendedVKey;
    }

    // 2. Fix Control/Alt using E0 Flag
    // MapVirtualKey often fails for E0 keys (Right Ctrl/Alt), defaulting to Left.
    // We must force Right if E0 is present.
    if (vKey == VK_LCONTROL && isE0)
    {
        vKey = VK_RCONTROL;
    }
    else if (vKey == VK_LMENU && isE0)
    {
        vKey = VK_RMENU;
    }
}

// 3. Fix Numpad Enter (Distinguish from Main Enter)
// Main Enter = 0x0D (No E0). Numpad Enter = 0x0D (With E0).
// Note: We don't have a separate VK for Numpad Enter in standard Windows API 
// (VK_RETURN is used for both), but we can invent one or just map it if needed.
// However, the standard behavior is usually sufficient unless you specifically asked for Numpad Enter distinction.
// *Optional:* If you want Numpad Enter to be treated differently, you could map it to a custom internal code,
// but for standard VK compatibility, leaving it as VK_RETURN is standard. 
// BUT, Visualizer might distinguish them by geometry. 
// Let's stick to Modifiers for this Phase.

// --- VKEY DISAMBIGUATION END ---

// ... existing broadcast logic ...
```

---

**Generate code for: Updated `Source/RawInputManager.cpp`.**