# 🤖 Cursor Prompt: Phase 29 - Mini Status Window (Dynamic Shielding)

**Role:** Expert C++ Audio Developer (JUCE Framework & Win32 API).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 27.5 Complete. The app successfully steals focus using `AttachThreadInput` to block typing in other apps when MIDI Mode is ON.
*   **Problem:** Currently, it forces the **Main Window** to the front. If the user wants to work in a DAW while performing, the Main Window covers the screen.
*   **Phase Goal:** Implement a **"Mini Status Window"**.
    *   If Main Window is **Minimized**: Steal focus to the **Mini Window** (Small, Draggable).
    *   If Main Window is **Open**: Steal focus to the **Main Window** (Existing behavior).
    *   **Mini Window Behavior:** Closing it turns off MIDI Mode.

**Strict Constraints:**
1.  **Input Sink:** `RawInputManager` must continue using the **Main Window Handle** for `RegisterRawInputDevices`. Do *not* change the registration target. Only change the `forceForegroundWindow` target.
2.  **Thread Safety:** The "Get Focus Target" callback runs on the OS Input Thread. Keep the logic inside it simple (Win32 calls like `IsIconic` are thread-safe).
3.  **UX:** The Mini Window should be small (e.g., 300x50), show "MIDI MODE ACTIVE", and have a Close button.

---

### Step 1: `MiniStatusWindow` (`Source/MiniStatusWindow.h/cpp`)
Create the UI component.

**Requirements:**
1.  **Inheritance:** `juce::DocumentWindow`.
2.  **Constructor:**
    *   Title: "MIDIQy Status".
    *   Style: `allButtons` (to get Close/Min).
    *   Options: `setAlwaysOnTop(true)`, `setResizable(true, false)`.
    *   Content: A simple `juce::Label` ("MIDI Mode is ON. Press Scroll Lock or Close to stop.").
3.  **Close Button:** Override `closeButtonPressed()`.
    *   Action: Call `SettingsManager::getInstance()->setMidiModeActive(false)`.
    *   Action: `setVisible(false)`.

### Step 2: Update `RawInputManager` (`Source/RawInputManager.h/cpp`)
Make the focus target dynamic.

**1. Header:**
*   Add `std::function<void*()> focusTargetCallback;`
*   Add `void setFocusTargetCallback(std::function<void*()> cb);`

**2. Implementation:**
*   In `rawInputWndProc` (inside the MIDI Mode check):
    *   Instead of `forceForegroundWindow(targetHwnd)`, do:
    *   `HWND shield = targetHwnd;` (Default)
    *   `if (instance->focusTargetCallback) shield = (HWND)instance->focusTargetCallback();`
    *   `forceForegroundWindow(shield);`

### Step 3: Update `MainComponent` (`Source/MainComponent.cpp`)
Manage the windows.

**1. Members:**
*   `std::unique_ptr<MiniStatusWindow> miniWindow;`

**2. Constructor:**
*   Initialize `miniWindow`.
*   **Register Callback:**
    ```cpp
    rawInputManager->setFocusTargetCallback([this]() -> void* {
        // Check if Main Window is Minimized (Iconic)
        auto* peer = this->getPeer();
        if (peer && IsIconic((HWND)peer->getNativeHandle()))
        {
            // Ensure Mini Window is ready
            if (!miniWindow->isVisible())
            {
                // Must call on Message Thread if possible, but inside WndProc 
                // we might need to rely on the window handle being valid.
                // For safety, MainComponent should handle visibility in the 
                // SettingsManager callback (Step 4), here we just return the handle.
            }
            // Return Mini Window Handle
            return miniWindow->getPeer() ? miniWindow->getPeer()->getNativeHandle() : peer->getNativeHandle();
        }
        // Return Main Window Handle
        return peer->getNativeHandle();
    });
    ```

**3. Settings Listener:**
*   Listen to `SettingsManager`.
*   If `isMidiModeActive()` becomes **False**:
    *   `miniWindow->setVisible(false);`
*   If `isMidiModeActive()` becomes **True** AND Main is Minimized:
    *   `miniWindow->setVisible(true);`

---

**Generate code for: `MiniStatusWindow`, Updated `RawInputManager`, and Updated `MainComponent`.**