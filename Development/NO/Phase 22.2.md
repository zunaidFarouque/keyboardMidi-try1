# 🤖 Cursor Prompt: Phase 22.2 - Musical Logic & Global Inheritance UI

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
*   **Current State:** Phase 22.1 Complete. `ZonePropertiesPanel` has a modular `Header` and `Input` section. Musical controls (Scale, Root) and Performance controls are still loose variables at the bottom.
*   **Phase Goal:** Migrate the **Musical Controls** into a `MusicalSection` struct and implement the dynamic visibility logic for "Absolute vs Relative" and "Piano" modes.

**Strict Constraints:**
1.  **Architecture:** Continue the `struct Section` pattern inside `ZonePropertiesPanel.h`.
2.  **Logic:** Implement `updateVisibility` to handle:
    *   **Piano Mode:** Hide Scale controls. Show Root Slider (Label: "Start Note").
    *   **Relative Mode:** Disable Scale/Root selectors (visualize as locked to Global). Highlight "Degree Offset" as "Mode Shift".
    *   **Absolute Mode:** Enable all standard controls.
3.  **Preservation:** Keep the **Performance** (Chords/Strum) controls working at the bottom.

---

### Step 1: Update `ZonePropertiesPanel.h`
Define the new Section.

**`struct MusicalSection`:**
*   `juce::Label title { "Musical Context" };`
*   `juce::ComboBox modeSelector;` (Items: "Absolute / Local", "Relative to Global").
    *   *Note:* This single combo controls both `useGlobalScale` and `useGlobalRoot` for simplicity, per requirements.
*   `juce::ComboBox scaleSelector;`
*   `juce::TextButton editScaleButton { "Edit" };`
*   `juce::Slider rootNoteSlider;` (Smart Note Slider).
*   `juce::Slider chromaticOffsetSlider;`
*   `juce::Slider degreeOffsetSlider;`
*   `juce::Label degreeLabel;` (Changes text based on mode: "Degree Offset" vs "Mode Shift").
*   `void setVisible(bool shouldShow);`

### Step 2: Update `ZonePropertiesPanel.cpp`
Implement the logic.

**1. `MusicalSection` Constructor/Setup:**
*   **Mode Selector:**
    *   ID 1: "Absolute" -> Sets `zone->useGlobalScale = false`, `zone->useGlobalRoot = false`.
    *   ID 2: "Relative" -> Sets `zone->useGlobalScale = true`, `zone->useGlobalRoot = true`.
*   **Root Slider:** Use `MidiNoteUtilities` (Phase 8 logic).
*   **Scale Selector:** Populate from `scaleLibrary`.

**2. Update `updateVisibility()` (The Brain):**
*   Get `LayoutStrategy`.
    *   **If Piano:**
        *   Show `rootNoteSlider` (Label "Start Note").
        *   Hide `scaleSelector`, `editScaleButton`, `modeSelector`, `degreeOffsetSlider`.
        *   Show `chromaticOffsetSlider` (maybe? Piano usually fixed, but transposing is useful).
    *   **If Linear / Grid:**
        *   Show `modeSelector`.
        *   **If Absolute:** Enable `scaleSelector`, `rootNoteSlider`. Label `degreeOffsetSlider` as "Degree Offset".
        *   **If Relative:** Disable `scaleSelector` (or show "Global"), Disable `rootNoteSlider`. Label `degreeOffsetSlider` as **"Mode Shift"**.

**3. Update `resized()`:**
*   Add `musicalSection` to the layout stack immediately after `inputSection`.
*   Continue to render the legacy Performance controls below it.

**4. Data Binding (`setZone`):**
*   Sync `modeSelector` based on the zone's boolean flags.
*   Sync Sliders/Combos.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**