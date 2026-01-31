### 📋 Cursor Prompt: Phase 56.3 - Smart Input Capture

**Target Files:**
1.  `Source/MappingEditorComponent.h`
2.  `Source/MappingEditorComponent.cpp`

**Task:**
Replace the standard "Add" button behavior with a "Smart Capture" workflow.
1.  **Overlay:** Show a "Press any key..." overlay when clicking "+".
2.  **Auto-Enable:** Temporarily enable MIDI Mode if it was off.
3.  **Capture:** Detect the key press, resolve the Device Alias automatically, and create the mapping.
4.  **Fallback:** Allow "Skip" to create a default blank mapping.

**Specific Instructions:**

1.  **Update `Source/MappingEditorComponent.h`:**
    *   **Add Inner Class:** `class InputCaptureOverlay;` (Forward declaration).
    *   **Add Member:** `std::unique_ptr<InputCaptureOverlay> captureOverlay;`
    *   **Add State:** `bool wasMidiModeEnabledBeforeCapture = false;`
    *   **Add Helper:** `void startInputCapture();`
    *   **Add Helper:** `void finishInputCapture(uintptr_t deviceHandle, int keyCode, bool skipped);`

2.  **Update `Source/MappingEditorComponent.cpp`:**

    *   **Define `InputCaptureOverlay`:**
        *   A simple `juce::Component` with a semi-transparent black background.
        *   `Label`: "Press any key to add mapping..." (Large font).
        *   `TextButton`: "Skip (Add Default)" (Bottom right).
        *   `TextButton`: "Cancel".
        *   `std::function<void(bool skipped)> onDismiss;` (Callback).

    *   **Implement `startInputCapture` (Called by Add Button):**
        1.  Check `settingsManager.isMidiModeActive()`. Store in `wasMidiModeEnabledBeforeCapture`.
        2.  If `false`, call `settingsManager.setMidiModeActive(true)`.
        3.  Create `captureOverlay`.
        4.  Set `captureOverlay->onDismiss`:
            *   If Cancelled: Restore MIDI mode state, delete overlay.
            *   If Skipped: Call `finishInputCapture(0, 0, true)`.
        5.  `addAndMakeVisible(captureOverlay.get());`
        6.  `resized();` (To cover the area).

    *   **Update `handleRawKeyEvent`:**
        *   **Check:** `if (captureOverlay != nullptr)`
        *   **Logic:**
            1.  Only act on `isDown == true`.
            2.  Call `finishInputCapture(deviceHandle, keyCode, false)`.
            3.  **Return early** (Do not process other learn logic or normal input).

    *   **Implement `finishInputCapture`:**
        1.  **Cleanup:** Remove overlay. Restore MIDI mode if it was originally off.
        2.  **Creation Logic:**
            *   **Defaults:** `inputKey = 0`, `deviceHash = "0"`.
            *   **If NOT Skipped:**
                *   `inputKey = keyCode`.
                *   `inputAlias = deviceManager.getAliasForHardware(deviceHandle)`.
                *   If Alias is valid/known, calculate Hash and set `deviceHash`.
                *   If Alias is "Unassigned", force Global (Hash "0") or keep Unassigned? -> *User Rule: "map to Global".* So if unassigned, `deviceHash = "0"`.
            *   **Create ValueTree:** Use the logic previously in the `addButton` lambda (create "Mapping", set props, add to `getCurrentLayerMappings()`).
        3.  **Selection:** Select the new row in the table.

    *   **Refactor `resized`:**
        *   Ensure `captureOverlay` (if exists) covers `getLocalBounds()`.

**Outcome:**
Clicking "+" dims the screen. Pressing a key on your "Laptop" immediately creates a mapping assigned to the "Laptop" alias (if defined) for that specific key. Clicking "Skip" behaves like the old "+" button.