Here is the prompt for **Phase 37**.

This builds the infrastructure to store and edit the colors.

***

# 🤖 Cursor Prompt: Phase 37 - Mapping Color Configuration

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 36.1 Complete.
*   **Phase Goal:** Allow the user to define global colors for each Manual Mapping Type (Note, CC, Command, Macro, Envelope). These colors will later be used in the Visualizer.

**Strict Constraints:**
1.  **Storage:** Store colors in `SettingsManager` using the `ValueTree`. Use hex string properties (e.g., `color_Note`, `color_CC`).
2.  **Defaults:**
    *   Note: `SkyBlue`
    *   CC: `Orange`
    *   Command: `Red`
    *   Envelope: `Purple`
    *   Macro: `Yellow`
3.  **UI:** Update `SettingsPanel` to include 5 Color Buttons. Clicking one opens a `ColourSelector`.

---

### Step 1: Update `SettingsManager` (`Source/SettingsManager.h/cpp`)
Manage the color palette.

**Requirements:**
1.  **Method:** `juce::Colour getTypeColor(ActionType type);`
    *   Look up property `color_TypeName` in `rootNode`.
    *   If missing, return the default color for that type.
2.  **Method:** `void setTypeColor(ActionType type, juce::Colour color);`
    *   Convert color to String (`toString()`).
    *   Set property.
    *   Send `sendChangeMessage()`.
3.  **Helper:** `juce::String getTypePropertyName(ActionType type);` (Internal helper to map Enum -> String Key).

### Step 2: Update `SettingsPanel` (`Source/SettingsPanel.h/cpp`)
Create the editor.

**Requirements:**
1.  **Components:** Add 5 `juce::TextButton`s (one for each type).
    *   Background color should match the setting.
    *   Text should be the Type Name.
2.  **Layout:** Group them under a `juce::GroupComponent` or a `Label` header "Mapping Colors".
3.  **Logic:**
    *   **Constructor:** Initialize button colors from `SettingsManager`.
    *   **OnClick:** Launch `juce::ColourSelector` in a `juce::CallOutBox`.
    *   **On Color Change:** Update `SettingsManager`. Update the Button's own background color immediately.
    *   **Listener:** If `SettingsManager` changes (e.g. preset load), update all buttons.

---

**Generate code for: Updated `SettingsManager.h/cpp` and Updated `SettingsPanel.h/cpp`.**