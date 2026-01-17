# 🤖 Cursor Prompt: Phase 5 - Interactive Mapping ("MIDI Learn")

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 4 Complete. We have a `ValueTree` backed Table Editor.
*   **Phase Goal:** Implement "MIDI Learn". The user clicks "Learn", presses a key, and the mapping updates automatically.

**Strict Constraints:**
1.  **Architecture:** `KeyNameUtilities.h` must NOT include `<windows.h>`. Use `uintptr_t` for handles.
2.  **Thread Safety:** The Raw Input hook runs on the OS thread. UI updates (changing the ValueTree, toggling buttons) **must** happen on the Message Thread using `juce::MessageManager::callAsync`.
3.  **CMake:** Provide the snippet to add `KeyNameUtilities.cpp`.

---

### Step 1: `KeyNameUtilities` (`Source/KeyNameUtilities.h/cpp`)
Create a static helper class to make sense of raw data.

**Requirements:**
1.  **`KeyNameUtilities.h`**:
    *   `static juce::String getKeyName(int virtualKeyCode);`
    *   `static juce::String getFriendlyDeviceName(uintptr_t deviceHandle);`
2.  **`KeyNameUtilities.cpp`**:
    *   Include `<windows.h>` **first**.
    *   `getKeyName`: Use `GetKeyNameTextA`. Fallback to "Key [Code]".
    *   `getFriendlyDeviceName`: Use `GetRawInputDeviceInfoA` with `RIDI_DEVICENAME`.
        *   *Logic:* The raw name is a messy hardware ID string. Extract the VID/PID (e.g., "VID_046D") to make a short, readable string like "Device [VID_046D]". Fallback to the Hex Handle if extraction fails.

### Step 2: Refactor `RawInputManager` (Observer Pattern)
Change the callback system to a Listener system so multiple parts of the app can listen to keys.

**Requirements:**
1.  **New Inner Class:** `struct Listener { virtual void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) = 0; };`
2.  **Members:** Replace the single `std::function` callback with `juce::ListenerList<Listener>`.
3.  **Methods:** `addListener`, `removeListener`.
4.  **Implementation:** In `rawInputWndProc`, iterate the list and call `handleRawKeyEvent`.

### Step 3: Update `MappingEditorComponent` (The Learn Logic)
**UI Additions:**
1.  **`juce::ToggleButton learnButton;`**: Labeled "L" or "Learn".
2.  **Inheritance:** Inherit from `RawInputManager::Listener`.
3.  **Constructor:** Register as a listener to `RawInputManager`.

**Logic (`handleRawKeyEvent`):**
1.  **Check:** If `!learnButton.getToggleState()`, return immediately.
2.  **Check:** If `!isDown`, return (we only learn on Key Down).
3.  **Thread Safety:** Wrap the rest in `callAsync`.
4.  **Selection:** Get `table.getSelectedRow()`. If -1, return.
5.  **Update Tree:**
    *   Get the `ValueTree` for the selected row.
    *   Set `inputKey` property (from `keyCode`).
    *   Set `deviceHash` property (from `deviceHandle`).
    *   **New:** Set a `displayName` property using `KeyNameUtilities` (e.g., "Device [1A2B] - Spacebar").
6.  **Finish:** Set `learnButton` to false.

**Display:**
*   Update `paintCell` to look for the `displayName` property. If it exists, display that instead of the raw numbers.

### Step 4: Update `MainComponent`
1.  Inherit from `RawInputManager::Listener`.
2.  Move the logic from the old lambda callback into `handleRawKeyEvent`.
3.  Register `MainComponent` as a listener in the constructor.
4.  Remove the old `setCallback` call.

---

**Generate code for: `KeyNameUtilities`, updated `RawInputManager`, updated `MappingEditorComponent`, updated `MainComponent`, and the `CMakeLists.txt` snippet.**