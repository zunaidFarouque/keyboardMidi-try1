This is the correct foundation. By creating a **Global Settings Manager**, we create a single source of truth for the Pitch Bend Range.

This allows us to implement the "Compiler" logic you requested:
1.  **Change Global Range** (e.g., from 12 to 24).
2.  **Trigger Rebuild:** `InputProcessor` recalculates the raw MIDI values for every PB mapping based on this new range.
3.  **Play Time:** The engine uses the pre-calculated integer. Zero math.

Here is the prompt for **Phase 25**.

***

# 🤖 Cursor Prompt: Phase 25 - Global Settings & Pitch Bend Architecture

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Pitch Bend mappings currently rely on local, hardcoded ranges in the Inspector. This makes it hard to sync with VSTs.
*   **Phase Goal:** Implement a **Global Pitch Bend Range** setting. Update the system so that Mappings define a *Semitone Shift*, and the `InputProcessor` "compiles" this into the correct MIDI value based on the Global Range.

**Strict Constraints:**
1.  **Architecture:** Create `SettingsManager` to hold application-wide config (PB Range, eventually Theme/Colors).
2.  **The Compiler:** `InputProcessor::rebuildMapFromTree` must use the Global Range to convert `pbShift` (semitones) into `action.data2` (raw MIDI 0-16383).
3.  **UI:** Add a "Settings" tab to the Left Panel in `MainComponent`.

---

### Step 1: `SettingsManager` (`Source/SettingsManager.h/cpp`)
The configuration container.

**Requirements:**
1.  **Inheritance:** `juce::ChangeBroadcaster`, `juce::ValueTree::Listener`.
2.  **Storage:** `juce::ValueTree rootNode { "OmniKeySettings" };`
3.  **Properties:**
    *   `pitchBendRange` (int): Default 12.
4.  **Methods:**
    *   `int getPitchBendRange();`
    *   `void setPitchBendRange(int range);`
    *   `void saveToXml(File file);` / `void loadFromXml(File file);` (Standard persistence).

### Step 2: Update `InputProcessor.cpp` (The Logic Update)
Refactor how Pitch Bend envelopes are compiled.

**1. Dependency:**
*   Add `SettingsManager& settingsManager;` to constructor.
*   Register as a `ChangeListener` to `SettingsManager`.
*   In `changeListenerCallback`, call `rebuildMapFromTree`.

**2. Update `addMappingFromTree`:**
*   **Old Logic:** Calculated raw MIDI value based on local sliders.
*   **New Logic:**
    *   Read `pbShift` (int, semitones) from the Mapping node.
    *   Get `globalRange` from `settingsManager`.
    *   **Math:**
        *   `double stepsPerSemi = 8192.0 / globalRange;`
        *   `int targetValue = 8192 + (int)(pbShift * stepsPerSemi);`
        *   Clamp `targetValue` to 0-16383.
    *   Store `targetValue` in `action.data2`. (This is what `ExpressionEngine` uses).

### Step 3: `SettingsPanel` (`Source/SettingsPanel.h/cpp`)
The UI for global config.

**Requirements:**
1.  **UI Elements:**
    *   `juce::Slider pbRangeSlider;` (Range 1-96).
    *   `juce::Label pbRangeLabel;` ("Global Pitch Bend Range (+/- semitones)").
2.  **Logic:**
    *   Read/Write to `SettingsManager`.

### Step 4: Integration
1.  **`MainComponent`:**
    *   Initialize `SettingsManager`.
    *   Pass it to `InputProcessor`, `MappingEditor`, and `SettingsPanel`.
    *   **Tabs:** Update the Left Panel. It currently holds `ZoneEditor`. Change it to a `juce::TabbedComponent`.
        *   Tab 1: "Mappings" (`MappingEditorComponent`).
        *   Tab 2: "Zones" (`ZoneEditorComponent`).
        *   Tab 3: "Settings" (`SettingsPanel`).
2.  **`StartupManager`:** Ensure `SettingsManager` loads/saves its XML to AppData.

### Step 5: Update `MappingInspector`
Remove the local "Range" slider.

*   If Target == Pitch Bend:
    *   Hide "Range" slider.
    *   Keep "Shift" slider.
    *   Add Label: "Uses Global Range".

---

**Generate code for: `SettingsManager`, `SettingsPanel`, updated `InputProcessor`, updated `MappingInspector`, and updated `MainComponent`.**