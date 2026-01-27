# 🤖 Cursor Prompt: Phase 36.2 - Key Name Repair (Exhaustive Overrides)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** `KeyNameUtilities` is mislabeling navigation keys.
*   **Problem:** "Left Arrow" shows as "Num 4", "Home" shows as "Num 7", "Pause" is missing/wrong. This is because `GetKeyNameTextA` requires specific flags we don't have stored.
*   **Phase Goal:** Rewrite `KeyNameUtilities::getKeyName` to enforce a strict manual mapping for ALL ambiguous keys.

**Strict Constraints:**
1.  **Priority:** The `switch` statement must happen **BEFORE** calling `GetKeyNameTextA`.
2.  **Coverage:** You must include cases for:
    *   `VK_LEFT`, `VK_RIGHT`, `VK_UP`, `VK_DOWN` -> "Left Arrow", etc.
    *   `VK_PRIOR`, `VK_NEXT` -> "Page Up", "Page Down".
    *   `VK_HOME`, `VK_END`, `VK_INSERT`, `VK_DELETE` -> "Home", "End", "Insert", "Delete".
    *   `VK_SNAPSHOT` -> "Print Screen".
    *   `VK_PAUSE` -> "Pause / Break".
    *   `VK_APPS` -> "Context Menu".
    *   `VK_NUMLOCK` -> "Num Lock".
    *   `VK_SCROLL` -> "Scroll Lock".
    *   `VK_CAPITAL` -> "Caps Lock".
    *   `VK_LWIN`, `VK_RWIN` -> "Left Windows", "Right Windows".

---

### Step 1: Update `Source/KeyNameUtilities.cpp`

Replace the `getKeyName` function entirely with this robust version.

```cpp
juce::String KeyNameUtilities::getKeyName(int virtualKeyCode)
{
    // 1. Check for Internal Pseudo-Codes
    if (virtualKeyCode == InputTypes::ScrollUp) return "Scroll Up";
    if (virtualKeyCode == InputTypes::ScrollDown) return "Scroll Down";
    if (virtualKeyCode == InputTypes::PointerX) return "Trackpad X";
    if (virtualKeyCode == InputTypes::PointerY) return "Trackpad Y";

    // 2. Manual Overrides for Windows Ambiguities
    // These keys often map to Numpad names if we don't handle them explicitly.
    switch (virtualKeyCode)
    {
        // Navigation
        case VK_LEFT:       return "Left Arrow";
        case VK_RIGHT:      return "Right Arrow";
        case VK_UP:         return "Up Arrow";
        case VK_DOWN:       return "Down Arrow";
        case VK_PRIOR:      return "Page Up";
        case VK_NEXT:       return "Page Down";
        case VK_HOME:       return "Home";
        case VK_END:        return "End";
        case VK_INSERT:     return "Insert";
        case VK_DELETE:     return "Delete";
        
        // System / Locks
        case VK_SNAPSHOT:   return "Print Screen";
        case VK_SCROLL:     return "Scroll Lock";
        case VK_PAUSE:      return "Pause / Break";
        case VK_NUMLOCK:    return "Num Lock";
        case VK_CAPITAL:    return "Caps Lock";
        case VK_APPS:       return "Context Menu";
        
        // Windows Keys
        case VK_LWIN:       return "Left Windows";
        case VK_RWIN:       return "Right Windows";
        
        // Misc
        case VK_CLEAR:      return "Clear (Num 5)";
        case 0xFF:          return "Fn / Hardware";
    }

    // 3. Fallback to Windows API for standard keys (A-Z, 0-9, F-Keys, Symbols)
    int scanCode = MapVirtualKeyA(virtualKeyCode, MAPVK_VK_TO_VSC);
    
    // Safety for unmapped keys
    if (scanCode == 0) return "Key " + juce::String(virtualKeyCode);

    // Shift left to high word
    int lParam = (scanCode << 16); 
    
    char name[128] = { 0 };
    if (GetKeyNameTextA(lParam, name, 128) > 0)
    {
        return juce::String(name);
    }

    return "Key " + juce::String(virtualKeyCode);
}
```

---

**Generate code for: Updated `Source/KeyNameUtilities.cpp`.**