# 🤖 Cursor Prompt: Phase 17.2 - Chord & Strum UI Integration

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 17 and 17.1 are complete. The backend (`ChordUtilities`, `StrumEngine`, `InputProcessor`) supports complex chords and strumming logic.
*   **Problem:** The `ZonePropertiesPanel` does not have controls for these features. Users cannot change Chord Type, Voicing, or Strum Speed.
*   **Phase Goal:** Update the UI to expose these parameters.

**Strict Constraints:**
1.  **Data Binding:** Controls must read/write directly to the `currentZone` members.
2.  **Layout:** The panel is getting crowded. Use a logical grouping or `GroupComponent` concept (visually) if possible, or just neat rows.
3.  **Enums:** Ensure ComboBox IDs match the Enums defined in `ChordUtilities` and `Zone` (Start at 1 usually).

---

### Step 1: Update `ZonePropertiesPanel.h`
Add the new UI controls.

**Requirements:**
1.  **Chord Controls:**
    *   `juce::ComboBox chordTypeSelector;` (Off, Triad, Seventh, Ninth).
    *   `juce::ComboBox voicingSelector;` (Close, Open, Guitar).
2.  **Strum Controls:**
    *   `juce::ComboBox playModeSelector;` (Direct, Strum/Buffer).
    *   `juce::Slider strumSpeedSlider;` (Range 0ms - 200ms).
    *   `juce::Label strumSpeedLabel;`

### Step 2: Update `ZonePropertiesPanel.cpp`
Implement the logic.

**1. Constructor:**
*   **Chord Type:** Add items: "Off" (1), "Triad" (2), "Seventh" (3), "Ninth" (4).
*   **Voicing:** Add items: "Close" (1), "Open" (2), "Guitar" (3).
*   **Play Mode:** Add items: "Direct" (1), "Strum Buffer" (2).
*   **Speed:** Range 0 to 500ms. Suffix " ms".
*   **Callbacks:**
    *   On change, update `currentZone->chordType`, `currentZone->voicing`, `currentZone->playMode`, `currentZone->strumSpeedMs`.
    *   Trigger `zoneManager->rebuildLookupTable()` (or generally refresh) if needed, though mostly these are runtime properties.

**2. `setZone(std::shared_ptr<Zone> zone)`:**
*   Update all new controls to match the zone's state.
*   *Safety:* Check `if (zone == nullptr)` before accessing.

**3. `resized()`:**
*   Layout the new controls below the Transpose sliders.
*   Make sure to update `getRequiredHeight()` so the Viewport knows to scroll down to reveal these new controls.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**