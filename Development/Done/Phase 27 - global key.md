# 🤖 Cursor Prompt: Phase 27 - Global Input Gate (System Integration)

**Role:** Expert C++ Audio Developer (Win32 API Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26 Complete. The app generates MIDI from keyboard input.
*   **Problem:** Keystrokes still reach the active window (e.g., typing "Q" into a document while playing C4).
*   **Phase Goal:** Implement a **Global Toggle** (default: Scroll Lock).
    *   **Mode ON:** Block keystrokes from the OS (Interception), Send MIDI.
    *   **Mode OFF:** Pass keystrokes to OS (Typing), Silence MIDI.

**Strict Constraints:**
1.  **Hybrid Architecture:** Use `RawInputManager` to detect the Toggle Key and generate MIDI. Use `WH_KEYBOARD_LL` (Hook) **only** to suppress keystrokes.
2.  **Safety:** The Hook must be uninstalled in the destructor to prevent locking the keyboard if the app closes.
3.  **Toggle Logic:** The Toggle Key itself must **never** be blocked by the hook, otherwise the user cannot turn the mode off.

---

### Step 1: Update `SettingsManager`
Store the global state.

**Requirements:**
1.  **Members:**
    *   `bool midiModeActive = false;`
    *   `int toggleKeyCode = 0x91;` (VK_SCROLL).
2.  **Methods:**
    *   `bool isMidiModeActive();`
    *   `void setMidiModeActive(bool active);` (Broadcasts change).
    *   `int getToggleKey();`
    *   `void setToggleKey(int vkCode);`
3.  **Serialization:** Save/Load these values.

### Step 2: `SystemIntegrator` (`Source/SystemIntegrator.h/cpp`)
Manage the Low-Level Hook.

**Requirements:**
1.  **Inheritance:** `juce::ChangeListener` (Listen to `SettingsManager`).
2.  **Static Data:**
    *   `static HHOOK hHook;`
    *   `static int allowedKey;` (The Toggle Key).
3.  **Static Callback:** `LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);`
    *   **Logic:**
        *   If `nCode == HC_ACTION`:
        *   Extract `KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;`
        *   If `p->vkCode == allowedKey`: Return `CallNextHookEx`. (Let it pass).
        *   Else: Return `1`. (Block it).
        *   Else: Return `CallNextHookEx`.
4.  **Methods:**
    *   `void installHook();`
    *   `void uninstallHook();`
    *   `changeListenerCallback`: If `settings.isMidiModeActive()` -> Install. Else -> Uninstall.
5.  **Destructor:** Call `uninstallHook()`.

### Step 3: Update `MainComponent` (The Trigger)
We need to detect the toggle press via Raw Input.

**Update `handleRawKeyEvent`:**
1.  Check: `if (keyCode == settingsManager.getToggleKey() && isDown)`
    *   `settingsManager.setMidiModeActive(!settingsManager.isMidiModeActive());`
    *   `return;` (Don't process this key for MIDI).

### Step 4: Update `InputProcessor` (The Gate)
**Update `processEvent`:**
*   Add check at the very top:
    *   `if (!settingsManager.isMidiModeActive()) return;`
    *   *Result:* No MIDI is generated when off.

### Step 5: Update `SettingsPanel`
Allow changing the key.

**UI:**
*   Add `juce::TextButton toggleKeyButton;` (Shows current key name).
*   **Logic:** Click -> Enter "Learn" mode -> Press Key -> Update `SettingsManager`.

### Step 6: Update `VisualizerComponent`
Visual Feedback.

**Update `paint`:**
*   If `!settingsManager.isMidiModeActive()`:
    *   Draw a semi-transparent black overlay over the keyboard.
    *   Draw text "MIDI MODE DISABLED (Press Scroll Lock)".

---

**Generate code for: `SystemIntegrator`, Updated `SettingsManager`, Updated `MainComponent`, Updated `InputProcessor`, Updated `SettingsPanel`, Updated `VisualizerComponent`, and `CMakeLists.txt`.**