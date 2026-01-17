# 🤖 Cursor Prompt: Phase 16.5 - Full 104-Key Layout Implementation

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** `KeyboardLayoutUtils` only defines the main alphanumeric block.
*   **Phase Goal:** Implement the full Desktop Keyboard layout (Function row, Nav cluster, Numpad, Modifiers).

**Strict Constraints:**
1.  **Completeness:** You must map **ALL** standard keys (Esc, F1-F12, PrintScreen, ScrollLock, Pause, Insert/Home/PgUp/Del/End/PgDn, Arrows, Numpad, and Modifiers).
2.  **Geometry:** Use standard ANSI spacing (e.g., Backspace is 2u, Spacebar is 6.25u, Numpad 0 is 2u).
3.  **Codes:** Use the correct Win32 Virtual Key Codes (hex literals are fine, e.g., `0x70` for F1).

---

### Step 1: Update `KeyboardLayoutUtils.cpp`
Refactor `getLayout()` to populate the full map.

**Groups to Implement:**

1.  **Function Row (Row -1):**
    *   Esc (`0x1B`) at x=0.
    *   F1-F4 (`0x70`-`0x73`) starting x=2.
    *   F5-F8 (`0x74`-`0x77`) starting x=6.5.
    *   F9-F12 (`0x78`-`0x7B`) starting x=11.
    *   PrntScr/Scroll/Pause starting x=15.5.

2.  **Main Block (Rows 0-4) - Updates:**
    *   **Row 0:** Add Backspace (Width 2.0).
    *   **Row 1:** Tab (Width 1.5), `\` (Width 1.5).
    *   **Row 2:** Caps (Width 1.75), Enter (Width 2.25).
    *   **Row 3:** LShift (Width 2.25), RShift (Width 2.75).
    *   **Row 4 (Modifiers):**
        *   LCtrl (1.25), LWin (1.25), LAlt (1.25).
        *   Space (6.25).
        *   RAlt (1.25), RWin (1.25), Menu (1.25), RCtrl (1.25).

3.  **Navigation Cluster:**
    *   Ins/Home/PgUp (Row 0, x=15.5).
    *   Del/End/PgDn (Row 1, x=15.5).
    *   Arrow Up (Row 3, x=16.5).
    *   Left/Down/Right (Row 4, x=15.5).

4.  **Numpad (Starting x=19):**
    *   NumLock, /, *, - (Row 0).
    *   7, 8, 9, + (Tall, Height 2.0) (Row 1).
    *   4, 5, 6 (Row 2).
    *   1, 2, 3, Enter (Tall, Height 2.0) (Row 3).
    *   0 (Width 2.0), . (Row 4).

### Step 2: Update `VisualizerComponent.cpp`
Update the scaling logic to account for the wider layout.

**Refactor `paint`:**
1.  Change `float unitsWide = 15.5f;` to **`23.0f`**.
2.  Change `float unitsTall = 5.5f;` to **`6.5f`**.
3.  Adjust `startY` logic to center the keyboard vertically correctly (accounting for the Function row gap).

---

**Generate code for: `Source/KeyboardLayoutUtils.cpp` (Full content) and updated `VisualizerComponent.cpp` (Scaling logic).**