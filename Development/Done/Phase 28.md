# 🤖 Cursor Prompt: Phase 28 - MIDI Persistence & Mapping Duplication

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 27.1 Complete.
*   **Issue 1:** Every time the app restarts, the MIDI Output resets to "No Device" or the first default. The user has to manually re-select "loopMIDI" or "HALion".
*   **Issue 2:** Creating similar mappings (e.g., CC 1 and CC 11) requires clicking "+", then re-entering all settings from scratch. Users want to Duplicate an existing row.

**Phase Goal:**
1.  Save/Load the last used MIDI Output device name in `SettingsManager`.
2.  Add a "Duplicate" button to `MappingEditorComponent`.

**Strict Constraints:**
1.  **Persistence:** Store the **Device Name** (String), not the Index (which changes if you unplug devices).
2.  **Fallback:** If the saved device is not found on startup, fall back to the first available device (Index 0).
3.  **Duplication:** The duplicate operation must create a **Deep Copy** of the `ValueTree` so changing the new one doesn't affect the old one.

---

### Step 1: Update `SettingsManager` (`Source/SettingsManager.h/cpp`)
Store the preference.

**Requirements:**
1.  **Property:** `lastMidiDevice` (String).
2.  **Methods:**
    *   `void setLastMidiDevice(String name);`
    *   `String getLastMidiDevice();`
3.  **Serialization:** Ensure this is saved/loaded in `saveToXml`/`loadFromXml`.

### Step 2: Update `MainComponent.cpp`
Implement the Auto-Connect logic.

**Refactor Constructor:**
1.  **After** populating `midiSelector` with `midiEngine.getDeviceNames()`:
2.  **Retrieve:** `String savedName = settingsManager.getLastMidiDevice();`
3.  **Select:**
    *   Iterate the ComboBox items.
    *   If item text matches `savedName`, select it.
    *   Else, select Index 0 (if items exist).
4.  **Refactor `onChange`:**
    *   Inside the lambda, after `midiEngine.setOutputDevice(...)`:
    *   Call `settingsManager.setLastMidiDevice(midiSelector.getText());`
    *   Trigger `settingsManager.saveGlobalConfig()` (if not auto-saved by a timer).

### Step 3: Update `MappingEditorComponent` (`Source/MappingEditorComponent.h/cpp`)
Add the Duplicate feature.

**1. UI:**
*   Add `juce::TextButton duplicateButton;` ("Duplicate").
*   Place it next to the `addButton` ("+").

**2. Logic (`onClick`):**
*   Get selected row: `int row = table.getSelectedRow();`
*   **Validation:** If `row == -1` or row >= numRows, do nothing.
*   **Copy:**
    *   `juce::ValueTree original = presetManager.getMappingsNode().getChild(row);`
    *   `juce::ValueTree copy = original.createCopy();`
*   **Insert:**
    *   `presetManager.getMappingsNode().addChild(copy, row + 1, nullptr);` (Insert directly below).
*   **UX:** Select the new row (`table.selectRow(row + 1)`).

---

**Generate code for: Updated `SettingsManager`, Updated `MainComponent.cpp`, Updated `MappingEditorComponent.h`, and `MappingEditorComponent.cpp`.**