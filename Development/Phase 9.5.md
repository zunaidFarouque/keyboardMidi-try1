# 🤖 Cursor Prompt: Phase 9.5 - Studio Mode (Simple vs. Advanced)

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 9.4 Complete. We have a complex Device Alias system.
*   **Problem:** For casual users with one keyboard, dealing with Device IDs and Aliases is over-engineered. They just want "Plug and Play".
*   **Phase Goal:** Implement **"Studio Mode"**.
    *   **Studio Mode OFF (Default?):** All input devices are treated as **ID 0 (Global)**. Device distinction is disabled. "Device Setup" UI is hidden.
    *   **Studio Mode ON:** Input devices retain their specific IDs. Aliases work. "Device Setup" UI is visible.

**Strict Constraints:**
1.  **Input Logic:** In `InputProcessor::processEvent`, if Studio Mode is OFF, `deviceHandle` must be forced to `0` before any lookup happens.
2.  **UI Logic:**
    *   `MainComponent` must hide the "Device Setup" button when Studio Mode is OFF.
    *   `VisualizerComponent` must lock its view to "Global" when Studio Mode is OFF.
3.  **Persistence:** Save this setting in `OmniKeySettings.xml`.

---

### Step 1: Update `SettingsManager` (`Source/SettingsManager.h/cpp`)
Add the flag.

**Requirements:**
1.  **Member:** `bool studioMode = false;` (Start simple).
2.  **Methods:**
    *   `bool isStudioMode() const;`
    *   `void setStudioMode(bool active);` (Broadcast change).
3.  **Serialization:** Save/Load this boolean.

### Step 2: Update `InputProcessor` (`Source/InputProcessor.cpp`)
Implement the bypass.

**Refactor `processEvent`:**
```cpp
void InputProcessor::processEvent(uintptr_t rawDeviceHandle, int key, bool isDown)
{
    // LOGIC CHANGE: Studio Mode Check
    uintptr_t effectiveDevice = rawDeviceHandle;
    if (!settingsManager.isStudioMode())
    {
        effectiveDevice = 0; // Force Global ID
    }

    // Proceed with existing Alias Lookup using effectiveDevice...
}
```

**Refactor `simulateInput`:**
*   Apply the same logic. If `!settings.isStudioMode()`, force `viewDeviceHash = 0`.

### Step 3: Update `SettingsPanel` (`Source/SettingsPanel.cpp`)
Add the toggle.

**UI:**
*   Add `juce::ToggleButton studioModeToggle;` ("Studio Mode (Multi-Device Support)").
*   **Logic:** Connect to `SettingsManager`.

### Step 4: Update `MainComponent` (`Source/MainComponent.cpp`)
Hide advanced controls.

**1. Constructor:**
*   Register as `ChangeListener` to `settingsManager`.
*   Check initial state: `deviceSetupButton.setVisible(settingsManager.isStudioMode());`

**2. `changeListenerCallback`:**
*   Update `deviceSetupButton` visibility based on new state.
*   Trigger `resized()` to re-layout header.

### Step 5: Update `VisualizerComponent` (`Source/VisualizerComponent.cpp`)
Simplify the view.

**Update `updateViewSelector`:**
*   If `!settingsManager.isStudioMode()`:
    *   Clear items.
    *   Add ONLY "Global (All Devices)".
    *   Select index 0.
    *   Disable the ComboBox (`setEnabled(false)`).
*   Else:
    *   Enable ComboBox.
    *   Populate normally (Global + Aliases).

---

**Generate code for: `SettingsManager`, `SettingsPanel`, `InputProcessor`, `MainComponent`, and `VisualizerComponent`.**