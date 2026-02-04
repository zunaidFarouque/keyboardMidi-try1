# 🤖 Cursor Prompt: Phase 18.1 - Performance UI Integration

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18 Complete. The Engine supports Commands (Sustain, Latch) and Zone Sustain Permissions, but the UI has no controls for them.
*   **Phase Goal:** Update `MappingInspector` to assign Commands to keys, and `ZonePropertiesPanel` to toggle sustain permission.

**Strict Constraints:**
1.  **Dynamic Inspector:** When `ActionType` is "Command", the Inspector must **Hide** the Channel/Velocity/Note sliders and **Show** a Command Selector ComboBox.
2.  **Data Binding:** The Command Selector writes to the `data1` property of the ValueTree (just like the Note Slider did).
3.  **Zone UI:** Add a simple checkbox for "Allow Sustain".

---

### Step 1: Update `MappingInspector.h`
Add the new control.

**Requirements:**
1.  **Member:** `juce::ComboBox commandSelector;`.
2.  **Label:** `juce::Label commandLabel;`.

### Step 2: Update `MappingInspector.cpp`
Implement the dynamic UI logic.

**1. Constructor:**
*   Populate `commandSelector` with items matching `CommandID` enum:
    *   ID 1: "Sustain (Momentary)"
    *   ID 2: "Sustain (Toggle)"
    *   ID 3: "Sustain (Inverse)"
    *   ID 4: "Latch (Toggle)"
    *   ID 5: "Panic (All Off)"
    *   ID 6: "Panic (Latched Only)"
*   *Note:* Ensure IDs match `MappingTypes.h`.

**2. Update `setSelection` (The Logic):**
*   Get `ActionType`.
*   **If Type == Command:**
    *   `channelSlider`, `data1Slider`, `data2Slider` -> `setVisible(false)`.
    *   `commandSelector` -> `setVisible(true)`.
    *   Read `data1` from ValueTree -> Set `commandSelector` ID.
*   **If Type == Note/CC:**
    *   Sliders -> `setVisible(true)`.
    *   `commandSelector` -> `setVisible(false)`.

**3. Update `resized`:**
*   Handle the layout change. If Command mode is active, place the ComboBox where the Sliders usually go.

**4. Callback (`commandSelector.onChange`):**
*   Write selected ID to `data1` property of selected trees (using `undoManager`).

### Step 3: Update `ZonePropertiesPanel` (`Source/ZonePropertiesPanel.h/cpp`)
Add the permission flag.

**Requirements:**
1.  **UI:** Add `juce::ToggleButton allowSustainToggle;`.
2.  **Logic:**
    *   In `setZone`: `allowSustainToggle.setToggleState(zone->allowSustain, dontSendNotification);`
    *   In `onClick`: Update `currentZone->allowSustain`. Save to ValueTree if applicable (or just memory).
3.  **Layout:** Place it near the "Lock Transpose" toggle.

---

**Generate code for: Updated `MappingInspector.h`, Updated `MappingInspector.cpp`, Updated `ZonePropertiesPanel.h`, and `ZonePropertiesPanel.cpp`.**