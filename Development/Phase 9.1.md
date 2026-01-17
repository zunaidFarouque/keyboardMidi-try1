# 🤖 Cursor Prompt: Phase 9.1 - Device Setup UI Integration

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 9 Complete. Backend logic (`DeviceManager`, `InputProcessor`) is ready. `DeviceSetupComponent` exists but is isolated.
*   **Phase Goal:** Integrate the Device Setup UI into the main window and ensure hardware IDs are displayed with friendly names.

**Strict Constraints:**
1.  **Windowing:** Use `juce::DialogWindow::LaunchOptions` to open the setup panel.
2.  **Readability:** The hardware list must use `KeyNameUtilities::getFriendlyDeviceName`.

---

### Step 1: Update `DeviceSetupComponent` (Polish)
Currently, the "Right Panel" likely shows raw Hardware IDs. Make it user-friendly.

**Requirements:**
1.  **Dependency:** Include `KeyNameUtilities.h`.
2.  **Display:** In the `ListBoxModel` for the Hardware list:
    *   Instead of just drawing the ID, call `KeyNameUtilities::getFriendlyDeviceName(id)`.
    *   Draw the formatted name in the cell.
3.  **Interaction:**
    *   Add a **"Delete Alias"** button (to remove unused groups).
    *   Add a **"Remove Hardware"** button (to unbind a device).

### Step 2: Update `MainComponent` (The Button)
Add the entry point for the user.

**Requirements:**
1.  **UI:** Add `juce::TextButton deviceSetupButton;` ("Device Setup").
2.  **Layout:** Place it in the top header, next to Save/Load.
3.  **Logic (`onClick`):**
    *   Create a `juce::DialogWindow::LaunchOptions`.
    *   Set `content` to `new DeviceSetupComponent(deviceManager)`.
    *   Set title to "Rig Configuration".
    *   Set `resizable` to true, `useNativeTitleBar` to true.
    *   Call `launchAsync()`.

### Step 3: Verify `InputProcessor` (Fallback)
Ensure existing presets (from Phase 8) don't crash the app.

**Logic Update (`addMappingFromTree`):**
*   The system now expects `inputAlias`.
*   **Check:** If `inputAlias` is missing but `deviceHash` exists (Legacy Preset):
    *   Treat it as "Unassigned" or auto-map it to "Master Input".
    *   *Goal:* Prevent the mapping from being silently dropped.

---

**Generate code for: Updated `DeviceSetupComponent`, Updated `MainComponent`, and the `InputProcessor` fallback fix.**