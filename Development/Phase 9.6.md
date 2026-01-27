# 🤖 Cursor Prompt: Phase 9.6 - Rig Health Check & Quick Setup Wizard

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 9.5 Complete. Studio Mode allows specific Alias usage.
*   **Problem:** If a user loads a complex Preset (e.g., "LeftKeys" + "RightKeys") but hasn't configured their hardware, the Preset loads silently but makes no sound (mapped aliases have no hardware attached).
*   **Phase Goal:** Implement a **Rig Health Check** on preset load. If Aliases are missing hardware, launch a **Quick Setup Wizard** overlay.

**Strict Constraints:**
1.  **Studio Mode Only:** If `SettingsManager::isStudioMode()` is false, ignore all checks (Global mode works automatically).
2.  **Input Focus:** The Wizard must capture Raw Input. Ensure it inherits `RawInputManager::Listener` and handles the "Device Assignment" logic.
3.  **UI:** Implement as an Overlay Component inside `MainComponent` (z-order top), ensuring it blocks interaction with the rest of the app until finished or cancelled.

---

### Step 1: Update `DeviceManager` (`Source/DeviceManager.h/cpp`)
Add diagnostics.

**Method:** `StringArray getEmptyAliases(const std::vector<uintptr_t>& requiredAliasHashes);`
*   **Logic:**
    *   Iterate `requiredAliasHashes`.
    *   Check if Alias exists. (If not, Create it!).
    *   Check if Alias has associated hardware (`getHardwareForAlias(name).size() > 0`).
    *   If Hardware count is 0, add Name to the return list.

### Step 2: `QuickSetupWizard` (`Source/QuickSetupWizard.h/cpp`)
The interactive calibration tool.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `RawInputManager::Listener`.
2.  **State:**
    *   `juce::StringArray aliasesToMap;`
    *   `int currentIndex = 0;`
3.  **UI:**
    *   Large Font: "Action Required".
    *   Instruction: "Press any key on: **[Current Alias Name]**".
    *   `juce::TextButton skipButton` ("Skip").
    *   `juce::TextButton cancelButton` ("Cancel Setup").
4.  **Logic (`handleRawKeyEvent`):**
    *   **Ignore Global/Virtual events** (ensure it's a real device).
    *   **Action:**
        *   Call `deviceManager.assignHardware(aliasesToMap[currentIndex], deviceHandle)`.
        *   Increment `currentIndex`.
        *   `repaint()` (Update text for next alias).
    *   **Completion:** If `currentIndex >= size`, `setVisible(false)`, call `onFinish` callback.

### Step 3: Update `InputProcessor` or `MainComponent` (The Trigger)
Where do we check the preset? `MainComponent` is the best place as it owns the UI.

**Update `MainComponent.cpp`:**
1.  **Member:** `QuickSetupWizard setupWizard;` (Initialize with DeviceMgr).
2.  **Resize:** Make `setupWizard` cover the entire bounds.
3.  **Logic (Inside `loadButton.onClick` or listener):**
    *   After `presetManager.loadFromFile(...)`:
    *   Check `if (settingsManager.isStudioMode())`:
        *   Scan Preset for all `deviceHash` properties (Aliases).
        *   Call `deviceManager.getEmptyAliases(...)`.
        *   If result not empty:
            *   `setupWizard.startSequence(missingAliases);`
            *   `setupWizard.setVisible(true);`

---

**Generate code for: Updated `DeviceManager`, `QuickSetupWizard`, and Updated `MainComponent`.**