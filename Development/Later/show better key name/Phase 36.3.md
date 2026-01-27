This is a **Raw Input Resolution Error**.

The issue is that for certain keys (like Print Screen and Numpad/Nav keys on laptops), the Raw Input driver sometimes sends a `VKey` of `0` or `0xFF` (255) while providing a valid `MakeCode` (Scan Code).

Currently, our code trusts the `VKey`. If it sees `255`, it passes `255` to the utility, which prints "Fn/Hardware". Since multiple keys are defaulting to `255`, they all trigger the same action (like your MIDI Toggle).

We need to **repair** the Virtual Key Code inside `RawInputManager` before broadcasting it.

Here is the plan for **Phase 36.3**.

### The Fix Logic
1.  **Inside `RawInputManager::rawInputWndProc`:**
    *   Check if `raw->data.keyboard.VKey` is `0` or `0xFF`.
    *   If so, ignore the broken VKey and calculate the **Correct VKey** from `raw->data.keyboard.MakeCode` using the Windows API `MapVirtualKey`.
    *   *Specific Fix for PrintScreen:* If Scan Code is `0x37` (Snapshot) or `0x54` (Alt+PrtScr), force `VK_SNAPSHOT`.
    *   *Specific Fix for Pause:* If Scan Code is `0x45` (NumLock position) but flagged `E1` (Pause sequence), force `VK_PAUSE`.

2.  **Inside `KeyNameUtilities`:**
    *   Remove the `case 0xFF: return "Fn / Hardware";` line. It is hiding bugs. We want to see the raw number if something is still wrong.

***

# 🤖 Cursor Prompt: Phase 36.3 - Fix Raw Input VKey Resolution

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
*   **Current State:** Keys like Print Screen, Pause, and Laptop Navigation keys often register as `VKey = 255 (0xFF)`.
*   **Problem:** This causes key conflicts (multiple physical keys map to ID 255) and prevents proper naming ("Fn/Hardware").
*   **Phase Goal:** Implement VKey Repair logic in `RawInputManager` to convert Scan Codes to valid VKeys when the driver fails to provide them.

**Strict Constraints:**
1.  **Repair Logic:** Only attempt to repair if `VKey == 0` or `VKey == 0xFF`.
2.  **Method:** Use `MapVirtualKey(makeCode, MAPVK_VSC_TO_VK_EX)` or `MAPVK_VSC_TO_VK`.
3.  **Naming:** Remove the `0xFF` case from `KeyNameUtilities` to prevent masking errors.

---

### Step 1: Update `Source/RawInputManager.cpp`

**Refactor `rawInputWndProc` (The Repair Logic):**

Inside the `WM_INPUT` -> `RIM_TYPEKEYBOARD` block:

```cpp
// ... extract raw data ...
UINT vKey = raw->data.keyboard.VKey;
UINT makeCode = raw->data.keyboard.MakeCode;
UINT flags = raw->data.keyboard.Flags;

// --- VKEY REPAIR START ---
if (vKey == 0 || vKey == 0xFF)
{
    // Try to resolve generic keys from Scan Code
    vKey = MapVirtualKey(makeCode, MAPVK_VSC_TO_VK);

    // Manual Fixes for Stubborn Keys
    // Print Screen often sends MakeCode 0x37 (55) but VKey 0/255
    if (makeCode == 0x37) vKey = VK_SNAPSHOT;
    
    // Pause/Break is tricky (E1 prefix), but often maps to 0x45 or similar 
    // depending on the hardware layer. 
    // If MapVirtualKey failed (still 0), check for Pause scan code sequence start
    if (vKey == 0 && makeCode == 0x45) vKey = VK_NUMLOCK; // or VK_PAUSE logic
}

// Correction for Extended Keys (E0 flag)
// Some keys (Insert, Del, Home, Arrows) share Scan Codes with Numpad 
// but are differentiated by the E0 flag. MapVirtualKey might return the Numpad version.
// We usually prefer the generic VKey (e.g. VK_HOME) which handles both, 
// but let's ensure we aren't getting stuck with 0xFF.
// --- VKEY REPAIR END ---

// ... Proceed with existing "Ignore Repeats" logic using the FIXED vKey ...
// ... Broadcast ...
```

### Step 2: Update `Source/KeyNameUtilities.cpp`

**Remove the Mask:**
*   Delete the line: `case 0xFF: return "Fn / Hardware";`
*   Let it fall through to the API or the default "Key [Code]" string so we can debug if it's still broken.

---

**Generate code for: Updated `Source/RawInputManager.cpp` and `Source/KeyNameUtilities.cpp`.**