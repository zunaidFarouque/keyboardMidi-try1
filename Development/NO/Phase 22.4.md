# 🤖 Cursor Prompt: Phase 22.4 - UI Completion & Release Settings

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 22.3 Complete. The UI is modular.
*   **Missing Features:**
    1.  The `allowSustain` toggle (implemented in Phase 18) was accidentally omitted from the `PerformanceSection` refactor.
    2.  The requested "Release" controls (Mode/Duration) are missing.

**Phase Goal:** Add the missing controls to `PerformanceSection` and ensure they bind correctly to the data model.

**Strict Constraints:**
1.  **Location:** Add these to `struct PerformanceSection` in `ZonePropertiesPanel`.
2.  **Data Binding:**
    *   `allowSustain`: Binds to `zone->allowSustain`.
    *   `releaseDuration`: Binds to a NEW member in Zone `float releaseDurationMs` (Default 0).
    *   `releaseMode`: Binds to a NEW member in Zone `ReleaseMode releaseMode` (Instant / Time).
3.  **Visibility:** Release Duration slider should only be visible if `releaseMode == Time`.

---

### Step 1: Update `Zone` (`Source/Zone.h/cpp`)
Add the new data members for Release logic (Backend preparation).

**Requirements:**
1.  **Enum:** `enum class ReleaseMode { Instant, Time };`
2.  **Members:**
    *   `ReleaseMode releaseMode = ReleaseMode::Instant;`
    *   `float releaseDurationMs = 0.0f;`
3.  **Serialization:** Save/Load these properties in `toValueTree`/`fromValueTree`.

### Step 2: Update `ZonePropertiesPanel.h` (`PerformanceSection`)
Add the UI components.

**Requirements:**
1.  **Members:**
    *   `juce::ToggleButton allowSustainToggle { "Allow Sustain" };`
    *   `juce::ComboBox releaseModeSelector;`
    *   `juce::Slider releaseDurationSlider;`
    *   `juce::Label releaseDurationLabel;`

### Step 3: Update `ZonePropertiesPanel.cpp`
Implement logic and layout.

**1. Constructor:**
*   **Allow Sustain:** Restore the logic from Phase 18.
*   **Release Mode:** Items: "Instant" (1), "Timed Release" (2).
*   **Release Duration:** Range 0 to 5000 ms (5 seconds). Suffix " ms".
*   **Callbacks:** Update Zone members. Trigger `updateVisibility()` on Mode change.

**2. `updateVisibility()`:**
*   `releaseDurationSlider`: Visible only if `currentZone->releaseMode == ReleaseMode::Time`.

**3. `resized()`:**
*   Add these controls to the `PerformanceSection` layout stack.
*   *Suggestion:* Group `allowSustain` near the top of the section. Place Release controls at the bottom of the section.

**4. `setZone()`:**
*   Sync the new controls with `currentZone` data.

---

**Generate code for: Updated `Zone.h/cpp` (New members) and Updated `ZonePropertiesPanel.h/cpp` (Restored/New UI).**