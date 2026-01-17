# 🤖 Cursor Prompt: Phase 16.6 - Zone Colors, Channels & Visual Tweaks

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 16.5 Complete. Zones exist but have hardcoded output channels and auto-assigned colors.
*   **Phase Goal:** Allow users to set specific Colors and MIDI Channels per Zone. Change Visualizer underlay to be rectangular.

**Strict Constraints:**
1.  **Serialization:** `midiChannel` and `zoneColor` MUST be saved/loaded via `Zone::toValueTree` and `fromValueTree`.
2.  **Visuals:** The Zone Underlay (Layer 1) in the Visualizer must be a sharp rectangle (no rounded corners).
3.  **UI:** Use a `juce::TextButton` to display/edit the color (opens a `CallOutBox` or `ColourSelector` on click).

---

### Step 1: Update `Zone` (`Source/Zone.h/cpp`)
Enhance the data model.

**Requirements:**
1.  **Members:** Add `int midiChannel = 1;`. (Ensure `zoneColor` is already there from Phase 16.1).
2.  **Serialization:**
    *   **Save:** Write `midiChannel` property. Write `zoneColor` as a String (use `zoneColor.toString()`).
    *   **Load:** Read `midiChannel`. Read `zoneColor` (use `juce::Colour::fromString(...)`).
3.  **Process:** In `processKey`, ensure the returned `MidiAction` uses `this->midiChannel`.

### Step 2: Update `ZonePropertiesPanel` (`Source/ZonePropertiesPanel.h/cpp`)
Add controls to edit these new properties.

**Requirements:**
1.  **UI Elements:**
    *   `juce::Slider channelSlider;` (Range 1-16).
    *   `juce::Label channelLabel;`
    *   `juce::TextButton colorButton;` (Displays the color).
2.  **Constructor:** Setup slider and button.
3.  **Logic (`colorButton.onClick`):**
    *   Create a `juce::ColourSelector`.
    *   Use `juce::CallOutBox::launchAsynchronously` to show it attached to the button.
    *   When color changes, update `currentZone->zoneColor`, update button background color, and trigger a `ZoneManager` change (to refresh Visualizer).
4.  **Logic (`setZone`):**
    *   Update `channelSlider` value.
    *   Update `colorButton` background colour to match zone.

### Step 3: Update `VisualizerComponent` (`Source/VisualizerComponent.cpp`)
Tweak the rendering style.

**Refactor `paint` (Layer 1):**
*   **Old:** `g.fillRoundedRectangle(fullBounds, 4.0f);`
*   **New:** `g.fillRect(fullBounds);` (Sharp edges for the underlay).
*   *Keep Layers 2, 3, 4 (Key Body) rounded.*

---

**Generate code for: Updated `Zone.h/cpp`, Updated `ZonePropertiesPanel.h/cpp`, and Updated `VisualizerComponent.cpp`.**