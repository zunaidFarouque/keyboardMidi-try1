# 🤖 Cursor Prompt: Phase 36.1 - Fix Key Name Display (Overrides)

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** `KeyNameUtilities::getKeyName` relies mostly on `GetKeyNameTextA`.
*   **The Problem:** Many navigation keys (Arrows, Home, End) share logic with the Numpad. The API defaults to naming them "Num 2", "Num 4", etc., which is confusing. Also, keys like `VK_APPS` (93) and `VK_PAUSE` are missing friendly names.
*   **Phase Goal:** Update `KeyNameUtilities.cpp` to enforce specific string names for these keys.

**Strict Constraints:**
1.  **Method:** Use a `switch(virtualKeyCode)` block **before** the API call.
2.  **Mappings to Fix:**
    *   `VK_APPS` (0x5D): "Context Menu"
    *   `VK_PAUSE` (0x13): "Pause / Break"
    *   `VK_SNAPSHOT` (0x2C): "Print Screen"
    *   `0xFF` (255): "Hardware / Fn" (Fallback for weird keyboard drivers).
    *   **Navigation:** `VK_LEFT`, `VK_RIGHT`, `VK_UP`, `VK_DOWN`, `VK_PRIOR` (PgUp), `VK_NEXT` (PgDn), `VK_HOME`, `VK_END`, `VK_INSERT`, `VK_DELETE`. (Name them clearly, e.g., "Left Arrow", "Page Up").
    *   **Numpad:** Ensure `VK_NUMPAD0` through `VK_DIVIDE` are still handled correctly (usually the API handles these fine, but check).

---

### Step 1: Update `Source/KeyNameUtilities.cpp`

**Refactor `getKeyName`:**

```cpp
juce::String KeyNameUtilities::getKeyName(int virtualKeyCode)
{
    // 1. Check for our internal Pseudo-Codes first (Scroll/Mouse)
    // ... (Keep existing logic for 0x1000, 0x2000 etc) ...

    // 2. Manual Overrides for Windows Ambiguities
    switch (virtualKeyCode)
    {
        // System
        case VK_APPS:       return "Context Menu";
        case VK_PAUSE:      return "Pause / Break";
        case VK_SNAPSHOT:   return "Print Screen";
        case 0xFF:          return "Function / Hardware";

        // Navigation (Force non-numpad names)
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
        
        // Lock Keys
        case VK_NUMLOCK:    return "Num Lock";
        case VK_SCROLL:     return "Scroll Lock";
        case VK_CAPITAL:    return "Caps Lock";
    }

    // 3. Fallback to Windows API for standard keys (A-Z, 0-9, F-Keys)
    int scanCode = MapVirtualKeyA(virtualKeyCode, MAPVK_VK_TO_VSC);
    
    // Safety for unmapped keys
    if (scanCode == 0) return "Key " + juce::String(virtualKeyCode);

    // Shift left to high word
    int lParam = (scanCode << 16); 
    
    // Edge Case: Some keys might need Extended Bit to name correctly via API, 
    // but the Switch above handles the worst offenders.
    
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