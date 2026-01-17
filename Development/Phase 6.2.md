# 🤖 Cursor Prompt: Phase 6.2 - Discrete Scroll & Robust Learning

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 6.1 Complete.
*   **Issue 1:** Scroll Wheel currently sends one code (`0x1000`) for both directions. We need separate events (Up vs Down) so they can trigger different actions (e.g., Volume Up vs Volume Down).
*   **Issue 2:** Trackpad Learning is flaky or non-existent. The UI likely isn't listening to axis events, or jitter handling is missing.

**Objectives:**
1.  Split Scroll events into `ScrollUp` (`0x1001`) and `ScrollDown` (`0x1002`).
2.  Implement robust "Max Axis" learning logic for the Trackpad (Ignore jitter, map the axis that moves significantly).
3.  Ensure `MappingEditorComponent` correctly inherits and listens to Axis events.

---

### Step 1: Update `MappingTypes.h`
Define the new discrete codes.
```cpp
namespace InputTypes {
    // remove ScrollVertical = 0x1000
    constexpr int ScrollUp   = 0x1001;
    constexpr int ScrollDown = 0x1002;
    constexpr int PointerX   = 0x2000;
    constexpr int PointerY   = 0x2001;
}
```

### Step 2: Update `KeyNameUtilities` (`Source/KeyNameUtilities.cpp`)
Update the string lookups for the new codes.
*   `0x1001` -> "Scroll Up"
*   `0x1002` -> "Scroll Down"
*   (Ensure `0x2000`/`0x2001` map to "Trackpad X"/"Trackpad Y").

### Step 3: Refactor `RawInputManager` (Scroll Logic)
Modify `handleRawMouseEvent` (or the logic inside `rawInputWndProc` for `RIM_TYPEMOUSE`).

**Logic Change:**
*   Get the `wheelDelta`.
*   **Do not** broadcast generic `0x1000`.
*   If `wheelDelta > 0`: Broadcast `handleRawKeyEvent` with code `InputTypes::ScrollUp`.
    *   *Note:* Treat this as a "Button Press" (isDown = true, then immediately isDown = false? Or just a momentary trigger). For MIDI mapping, treating it as a Key Down is usually easiest.
*   If `wheelDelta < 0`: Broadcast `handleRawKeyEvent` with code `InputTypes::ScrollDown`.

### Step 4: Fix `MappingEditorComponent` (Trackpad Learning)
This is the critical fix for the trackpad.

**Requirements:**
1.  **Inheritance:** Ensure it inherits `public RawInputManager::Listener`.
2.  **Registration:** Ensure `rawInputManager->addListener(this)` is called in constructor.
3.  **Implement `handleAxisEvent(uintptr_t device, int axisID, float value)`:**
    *   **Check:** `if (!learnButton.getToggleState()) return;`
    *   **Jitter Filter:** Calculate deviation from center (assuming 0.0-1.0 range).
        *   `float deviation = std::abs(value - 0.5f);`
    *   **Threshold:** `if (deviation < 0.2f) return;`
        *   *Reasoning:* If the user just touches the pad (jitter), deviation is low. If they **swipe**, value goes > 0.7 or < 0.3. The first axis to cross this threshold wins.
    *   **Map It:**
        *   Update the selected row in `ValueTree` (inputKey = `axisID`, deviceHash = `device`, type = `CC`).
        *   **Stop Learning:** `learnButton.setToggleState(false, sendNotification);`

### Step 5: Verify `MainComponent` Logging
Update `MainComponent::handleAxisEvent`.
*   Ensure it logs the axis events to `LogComponent`.
*   This is crucial for the user to debug if the Trackpad is generating data at all.

---

**Generate code for: `MappingTypes.h`, `KeyNameUtilities.cpp`, `RawInputManager.cpp` (Scroll fix), `MappingEditorComponent.h/cpp` (Axis Learn fix), and `MainComponent.cpp` (Logging fix).**