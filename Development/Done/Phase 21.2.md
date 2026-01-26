# 🤖 Cursor Prompt: Phase 21.2 - Global Inheritance UI

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 21.1 Complete. The Backend (`Zone`, `ZoneManager`) supports `useGlobalScale` and `useGlobalRoot` flags, but the UI (`ZonePropertiesPanel`) does not expose them.
*   **Phase Goal:** Add controls to `ZonePropertiesPanel` to toggle these flags.

**Strict Constraints:**
1.  **UI Logic:**
    *   Add "Follow Global" checkboxes next to the Scale and Root controls.
    *   If checked: **Disable** the local control (Scale Combo / Root Slider) to visually indicate it is overridden.
2.  **Data Logic:**
    *   When the toggle changes, update `currentZone->useGlobalScale` (or Root).
    *   Trigger a cache rebuild immediately so the sound updates.
3.  **Layout:** Squeeze these new buttons into the existing layout without doing a full redesign.

---

### Step 1: Update `ZonePropertiesPanel.h`
Add the new UI components.

**Requirements:**
1.  `juce::ToggleButton globalScaleToggle;` ("Global")
2.  `juce::ToggleButton globalRootToggle;` ("Global")

### Step 2: Update `ZonePropertiesPanel.cpp`
Implement the logic.

**1. Constructor:**
*   Configure toggles.
*   **Callback (`onClick`):**
    *   Update `currentZone` member (`useGlobalScale` / `useGlobalRoot`).
    *   Call `updateVisibility()` (New helper to enable/disable sliders).
    *   Call `rebuildZoneCache()` (To apply the musical change).

**2. Implement `updateVisibility()`:**
*   `scaleSelector.setEnabled(!globalScaleToggle.getToggleState());`
*   `rootNoteSlider.setEnabled(!globalRootToggle.getToggleState());`
*   *Note:* Also respect the "Piano" layout rule (Piano mode disables Scale regardless of Global toggle).

**3. Update `setZone(zone)`:**
*   Load values: `globalScaleToggle.setToggleState(zone->useGlobalScale, dontSendNotification);` (and Root).
*   Call `updateVisibility()`.

**4. Update `rebuildZoneCache()` (Crucial):**
*   This function calculates the arguments for `zone->rebuildCache(intervals, root)`.
*   **Logic:**
    *   `intervals`: If `globalScaleToggle` is ON, get from `zoneManager.getGlobalScaleName()`. Else, get from local `scaleSelector`.
    *   `root`: If `globalRootToggle` is ON, get from `zoneManager.getGlobalRootNote()`. Else, get from local `rootNoteSlider`.
    *   Pass these correct values to `currentZone->rebuildCache`.

**5. Update `resized()`:**
*   Place the toggles to the right of (or inside) the area for Scale and Root controls.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**