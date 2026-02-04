# 🤖 Cursor Prompt: Phase 22.5 - Zone Editor Refactor (Legacy Purge & Logic)

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** `ZonePropertiesPanel` is in a broken "Hybrid" state. It contains **Duplicate Controls**: one set inside the new `struct` sections (`headerSection`, `musicalSection`...) and one set of "Legacy" member variables (`rootSlider`, `scaleSelector`...) at the main class level.
*   **Problem:** This causes visual glitches (double rendering), logic bugs, and spaghetti code.
*   **Phase Goal:** Delete ALL legacy variables and enforce a strict "Section-Based" visibility logic.

**Strict Constraints:**
1.  **Delete:** Remove all loose UI member variables from `ZonePropertiesPanel.h` (e.g., `scaleSelector`, `rootSlider`, `chipList` legacy). **Keep only** the `struct Section` instances.
2.  **Clean:** Remove all `addAndMakeVisible(legacyVar)` and `legacyVar.setBounds` calls from `.cpp`.
3.  **Logic:** Implement the **Visibility Tree** defined below in `updateVisibility`.

---

### Step 1: Header Cleanup (`Source/ZonePropertiesPanel.h`)
**Action:** Delete these members (and any similar loose ones):
*   `aliasSelector`, `aliasLabel`, `nameEditor`, `nameLabel`, `transposeLockButton`.
*   `captureKeysButton`, `removeKeysButton`, `chipList` (the loose one), `strategySelector`, `gridIntervalSlider`.
*   `scaleSelector`, `editScaleButton`, `scaleLabel`, `rootSlider`, `chromaticOffsetSlider`, `degreeOffsetSlider`.
*   `chordTypeSelector`, `voicingSelector`, `playModeSelector`, `strumSpeedSlider`.
*   `allowSustainToggle`, `releaseModeSelector`, `releaseDurationSlider`.
*   `baseVelSlider`, `randVelSlider`, `strictGhostToggle`, `ghostVelSlider`.
*   `bassToggle`, `bassOctaveSlider`, `displayModeSelector`, `channelSlider`, `colorButton`.

**Keep:** `headerSection`, `inputSection`, `musicalSection`, `performanceSection`, `appearanceSection`.

### Step 2: Implementation Cleanup (`Source/ZonePropertiesPanel.cpp`)

**1. Constructor:**
*   Remove all setup code for the deleted members.
*   Ensure all callbacks are assigned **only** to the controls inside `headerSection`, `musicalSection`, etc.
*   *Correction:* In `handleRawKeyEvent`, ensure you only check `inputSection.assignKeys.getToggleState()`.

**2. `updateControlsFromZone`:**
*   Remove all lines updating deleted members.
*   Map data **only** to `section.control`.

**3. `resized`:**
*   **Delete** the massive block of manual `setBounds` calls at the bottom of the function.
*   The function should effectively just call `layoutSections()` and `setSize`.

### Step 3: The Logic Tree (`updateVisibility`)
Implement exactly this logic:

1.  **Input Section:**
    *   `Grid Interval`: Visible if `Strategy == Grid`.
    *   `Piano Help`: Visible if `Strategy == Piano`.

2.  **Musical Section:**
    *   **If Strategy == Piano:**
        *   Hide `Mode`, `Scale`, `EditScale`, `DegreeOffset`.
        *   Show `RootSlider` (Label: "Start Note").
    *   **If Strategy != Piano:**
        *   Show `Mode`.
        *   **If Absolute:** Show `Scale`, `EditScale`, `RootSlider` (Label: "Root Note"), `DegreeOffset` (Label: "Degree Offset").
        *   **If Relative:** Hide `Scale`, `RootSlider`. Show `DegreeOffset` (Label: "Mode Shift").

3.  **Performance Section:**
    *   **Master Switch:** If `ChordType == None`:
        *   Hide `Voicing`, `Bass`, `Ghost`, `PlayMode`, `StrumSpeed`.
    *   **If ChordType != None:**
        *   Show `Voicing`, `Bass`, `PlayMode`.
        *   **Bass:** Show `BassOctave` only if `BassToggle == ON`.
        *   **Ghost:** Show `StrictToggle`, `GhostVel` only if `Voicing` is "Filled" type.
        *   **Strum:** Show `StrumSpeed` only if `PlayMode == StrumBuffer`.
    *   **Release:** Show `ReleaseDuration` only if `ReleaseMode == Time`.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.** 