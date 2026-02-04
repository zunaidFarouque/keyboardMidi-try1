# 🤖 Cursor Prompt: Phase 6.1 - Mouse UI, Smart Locking & Learn

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 6 Complete. The Engine can process Mouse/Trackpad data (`PointerInputManager`), but the UI has no way to map them. The "Performance Mode" (Cursor Lock) is currently dumb and just hides the mouse regardless of whether it's needed.
*   **Phase Goal:** Make the Mouse/Trackpad features accessible in the UI, allow "Learning" of axis movements, and implement "Smart Locking" for the cursor.

**Strict Constraints:**
1.  **Architecture:** Do not break the existing `void*` isolation rules.
2.  **Thread Safety:** Use `juce::MessageManager::callAsync` for all UI updates triggered by Input events.
3.  **CMake:** Provide the snippet if new files are created (none expected, mostly updates).

---

### Step 1: Update `KeyNameUtilities` (The Translation Layer)
The UI currently sees `0x2000` and displays "Key [8192]". We need it to be readable.

**Update `Source/KeyNameUtilities.cpp`:**
Modify `getKeyName(int virtualKeyCode)`:
*   Add a check for our Pseudo-Codes (defined in Phase 6):
    *   If code == `0x1000`: Return "Mouse Scroll".
    *   If code == `0x2000`: Return "Trackpad X".
    *   If code == `0x2001`: Return "Trackpad Y".
*   If none of the above, proceed with standard `GetKeyNameTextA`.

### Step 2: Update `MappingEditorComponent` (The "Add" Menu)
Currently, the "+" button only adds a Note mapping. We need a menu to add specific types manually.

**Update `Source/MappingEditorComponent.cpp`:**
1.  **Refactor `addButton.onClick`**:
    *   Instead of immediately creating a tree, create a `juce::PopupMenu`.
    *   **Item 1:** "Add Key Mapping" (Triggers existing logic: Key Q -> Note).
    *   **Item 2:** "Add Scroll Mapping" (Sets `inputKey` to `0x1000`, `type` to "CC", `data1` to 1).
    *   **Item 3:** "Add Trackpad X" (Sets `inputKey` to `0x2000`, `type` to "CC", `data1` to 10).
    *   **Item 4:** "Add Trackpad Y" (Sets `inputKey` to `0x2001`, `type` to "CC", `data1` to 11).
    *   Show the menu asynchronously.

### Step 3: Update `MappingEditorComponent` (Axis Learning)
We want to support "Learning" for the Trackpad too. If the user clicks "Learn" and swipes the pad, it should register.

**Update `Source/MappingEditorComponent.h/cpp`:**
1.  **Interface:** Ensure `MappingEditorComponent` inherits from `RawInputManager::Listener`.
2.  **Implement `handleAxisEvent(uintptr_t device, int axisID, float value)`**:
    *   **Check:** If `!learnButton.getToggleState()`, return.
    *   **Threshold:** Only trigger if `value` changes significantly (to prevent noise from triggering a learn).
    *   **Logic:**
        *   Get selected row.
        *   Set `inputKey` to `axisID` (e.g., `0x2000` for X).
        *   Set `deviceHash` to `device`.
        *   Set `displayName` using `KeyNameUtilities`.
        *   Turn off Learn button.
    *   *Note:* Ensure this UI update happens on the Message Thread.

### Step 4: Implement "Smart Performance Mode"
The cursor should only lock/hide if the current preset actually *uses* the trackpad.

**1. Update `InputProcessor`:**
*   Add method: `bool hasPointerMappings()`.
*   *Logic:* Iterate through the `keyMapping` map. Return `true` if any key equals `0x2000` (X) or `0x2001` (Y).

**2. Update `MainComponent`:**
*   **Refactor `performanceModeButton.onClick`**:
    *   First, call `inputProcessor.hasPointerMappings()`.
    *   **If True:** Allow the Lock. Call `::ShowCursor(FALSE)` and `::ClipCursor`. Change button text to "Unlock Cursor (Esc)".
    *   **If False:** Show a `juce::AlertWindow` or `NativeMessageBox` saying "No Trackpad mappings found in this preset." and do NOT lock the cursor.
    *   *Safety:* Add a check in `timerCallback` or `processEvent` to detect the `VK_ESCAPE` key and forcibly unlock the cursor if the user gets stuck.

### Step 5: Verify Logging
**Update `MainComponent`:**
*   Ensure `MainComponent` implements `handleAxisEvent` (from the Listener interface).
*   In the implementation, call `logEvent(...)` just like you do for keys, passing the Axis ID as the Key Code.
*   *Result:* The log should show "Dev: [ID] | VAL | Trackpad X" when you move the mouse.

---

**Generate the code updates for: `KeyNameUtilities`, `MappingEditorComponent`, `InputProcessor`, and `MainComponent`.**