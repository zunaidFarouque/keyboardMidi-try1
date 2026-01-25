# 🤖 Cursor Prompt: Phase 22.3 - Performance, Appearance & Final Polish

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 22.2 Complete. `Header` and `Musical` sections are modular. However, `Performance` and `Appearance` controls are still loose legacy variables at the bottom of the file.
*   **Phase Goal:** Migrate the remaining controls into `PerformanceSection` and `AppearanceSection`, implement dynamic visibility (e.g., hide Ghost controls if not using Filled voicing), and delete legacy code.

**Strict Constraints:**
1.  **Architecture:** Complete the `struct Section` pattern in `ZonePropertiesPanel.h`.
2.  **Smart Visibility:**
    *   If `Chord Type == Off`: Hide Voicing, Bass, and Ghost controls.
    *   If `Voicing != Filled` (SmoothFilled/GuitarFilled): Hide Ghost controls (Strict Toggle, Ghost Vel).
    *   If `Play Mode == Direct`: Hide Strum Speed.
3.  **Cleanup:** Remove **ALL** legacy member variables (`ScopedPointer` style sliders) that were replaced by the structs.

---

### Step 1: Update `ZonePropertiesPanel.h`
Define the final two sections.

**1. `struct PerformanceSection`:**
*   `juce::Label title { "Performance & Chords" };`
*   `juce::ComboBox chordTypeSelector;`
*   `juce::ComboBox voicingSelector;`
*   `juce::ToggleButton strictGhostToggle { "Strict Harmony" };`
*   `juce::Slider ghostVelSlider;`
*   `juce::ToggleButton bassToggle { "Add Bass Root" };`
*   `juce::Slider bassOctaveSlider;`
*   `juce::ComboBox playModeSelector;`
*   `juce::Slider strumSpeedSlider;`
*   `void setVisible(bool shouldShow);`

**2. `struct AppearanceSection`:**
*   `juce::Label title { "Output & Visuals" };`
*   `juce::Slider channelSlider;`
*   `juce::Slider baseVelSlider;`
*   `juce::Slider randVelSlider;`
*   `juce::ComboBox displayModeSelector;` (Note Name vs Roman).
*   `juce::TextButton colorButton;`
*   `void setVisible(bool shouldShow);`

### Step 2: Update `ZonePropertiesPanel.cpp`
Implement logic and layout.

**1. Constructor & Setup:**
*   **Ghost Vel:** Range 0.0 to 1.0 (display as %).
*   **Bass Octave:** Range -3 to -1 (Interval 1).
*   **Strum Speed:** Range 0 to 500 ms.
*   **Velocity:** Range 0-127.
*   **Channel:** Range 1-16.
*   **Color Button:** Keep the existing `ColourSelector` logic but move it here.

**2. Update `updateVisibility()` (The Logic):**
*   `bool hasChords = currentZone->chordType != ChordType::None;`
*   `bool isFilled = (currentZone->voicing == Voicing::SmoothFilled || currentZone->voicing == Voicing::GuitarFilled);`
*   `bool isStrum = currentZone->playMode == PlayMode::Strum;`
*   **Rules:**
    *   `voicingSelector`, `bassToggle`, `bassOctaveSlider`: Visible only if `hasChords`.
    *   `strictGhostToggle`, `ghostVelSlider`: Visible only if `hasChords && isFilled`.
    *   `strumSpeedSlider`: Visible only if `isStrum`.

**3. Update `resized()` (The Stack):**
*   Start `y`.
*   Layout `headerSection`.
*   Layout `inputSection`.
*   Layout `musicalSection`.
*   Layout `performanceSection`.
*   Layout `appearanceSection`.
*   Update `requiredHeight`.

**4. Cleanup:** Delete all the old member variables and their initialization code from the constructor/destructor to finalize the refactor.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**