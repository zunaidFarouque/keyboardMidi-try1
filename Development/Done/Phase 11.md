# 🤖 Cursor Prompt: Phase 11 - Zone Configuration UI

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 10 Complete. The `ZoneManager` engine works, but we have no UI to create/edit zones.
*   **Phase Goal:** Implement the UI to manage Zones, including a "Key Capture" workflow to define input ranges.

**Strict Constraints:**
1.  **Architecture:** UI components must use `juce::MessageManager::callAsync` when updating from Input threads.
2.  **Reusability:** Reuse `MidiNoteUtilities` (Phase 8) for the Root Note slider.
3.  **Dependencies:** `ZonePropertiesPanel` must allow selecting a Device Alias (via `DeviceManager`).

---

### Step 1: `ZoneListPanel` (`Source/ZoneListPanel.h/cpp`)
Displays the list of active zones.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::ListBoxModel`, `juce::ChangeListener` (listen to `ZoneManager`).
2.  **Members:** `juce::ListBox listBox;`, `juce::TextButton addButton, removeButton;`.
3.  **Logic:**
    *   `addButton`: Calls `zoneManager->createDefaultZone()`.
    *   `removeButton`: Removes selected zone.
    *   **Selection:** When a row is selected, call a callback `std::function<void(std::shared_ptr<Zone>)> onSelectionChanged`.

### Step 2: `ZonePropertiesPanel` (`Source/ZonePropertiesPanel.h/cpp`)
Edits the selected Zone.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `RawInputManager::Listener`.
2.  **Controls:**
    *   **Alias:** `juce::ComboBox` (Populated from `DeviceManager`).
    *   **Name:** `juce::TextEditor`.
    *   **Scale:** `juce::ComboBox` (Major, Minor, etc.).
    *   **Root:** `juce::Slider` (Use `MidiNoteUtilities` for "C4" display).
    *   **Offsets:** `juce::Slider` (Octave shift).
    *   **Toggle:** `juce::ToggleButton` ("Lock Transpose").
    *   **Capture:** `juce::ToggleButton` ("Assign Keys").
    *   **Label:** "X Keys Assigned".
3.  **Logic (Selection):** `void setZone(std::shared_ptr<Zone> zone);` updates all controls to match the zone data.
4.  **Logic (Key Capture - `handleRawKeyEvent`):**
    *   If "Assign Keys" is **ON**:
        *   If `isDown`: Add `keyCode` to `currentZone->inputKeyCodes` (if unique).
        *   Update "X Keys Assigned" label.
        *   **Important:** Do NOT trigger sound engine.
    *   If "Assign Keys" is **OFF**: Do nothing.

### Step 3: `GlobalPerformancePanel` (`Source/GlobalPerformancePanel.h/cpp`)
Live controls for the `ZoneManager`.

**Requirements:**
1.  **Controls:**
    *   Degree Shift: `[-1]` `[+1]` buttons.
    *   Chromatic Shift: `[-1]` `[+1]` buttons.
    *   Display: Label showing current offsets (e.g. "Scale: +1 | Pitch: +2st").
2.  **Connection:** Call `zoneManager->setGlobalTranspose`. Listen to `ZoneManager` to update the label.

### Step 4: `ZoneEditorComponent` (`Source/ZoneEditorComponent.h/cpp`)
The container.

**Requirements:**
1.  **Layout:** `FlexBox` or `Grid`.
    *   Left: `ZoneListPanel`.
    *   Center: `ZonePropertiesPanel`.
    *   Top/Bottom: `GlobalPerformancePanel`.
2.  **Wiring:**
    *   Pass `ZoneManager`, `DeviceManager`, `RawInputManager` to children.
    *   Connect `ZoneListPanel::onSelectionChanged` -> `ZonePropertiesPanel::setZone`.

### Step 5: Integration & CMake
1.  Add `ZoneEditorComponent` to `MainComponent` (as a new Tab or simply replace the view for now).
2.  Update `CMakeLists.txt` with new files.

---

**Generate code for: `ZoneListPanel`, `ZonePropertiesPanel`, `GlobalPerformancePanel`, `ZoneEditorComponent`, and the `CMakeLists.txt` snippet.**