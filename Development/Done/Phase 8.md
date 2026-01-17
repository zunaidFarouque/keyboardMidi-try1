# 🤖 Cursor Prompt: Phase 8 - Smart Note Input & Text Parsing

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 7 Complete (Inspector Panel).
*   **Phase Goal:** Replace "Blind Numbers" in the Inspector with musical context. "Note 60" should appear as "C4". Users should be able to type "C#3" to set the value.

**Strict Constraints:**
1.  **Architecture:** `MidiNoteUtilities` must be a standalone helper class.
2.  **UX:** The slider text behavior must change dynamically. If the user selects a "CC" mapping, they should see numbers. If "Note", they should see Note Names.
3.  **CMake:** Provide the snippet to add `MidiNoteUtilities.cpp`.

---

### Step 1: `MidiNoteUtilities` (`Source/MidiNoteUtilities.h/cpp`)
Create a static helper class for musical conversions.

**Requirements:**
1.  `static juce::String getMidiNoteName(int noteNumber)`:
    *   Use `juce::MidiMessage::getMidiNoteName(noteNumber, true, true, 4)`.
    *   *Note:* The `4` argument ensures Middle C (60) is displayed as "C4".
2.  `static int getMidiNoteFromText(const juce::String& text)`:
    *   **Logic:**
        *   First, try to parse as a raw integer (`text.getIntValue()`). If the string contains only digits, return that number (clamped 0-127).
        *   If it contains letters, parse as Note Name.
        *   Find the Note Letter (A-G).
        *   Find Accidental (# or b).
        *   Find Octave number (e.g., -2 to 8).
        *   Calculate: `(Octave + 1) * 12 + NoteIndex`.
        *   *Validation:* Clamp result between 0 and 127.

### Step 2: Update `MappingInspector` (`Source/MappingInspector.cpp`)
We need to configure the sliders and update them dynamically based on the selection.

**Constructor Updates:**
1.  **Channel Slider:** `setRange(1, 16, 1)`.
2.  **Data 1/2 Sliders:** `setRange(0, 127, 1)`.
3.  **Text Box:** Ensure sliders allow text entry (`setTextBoxStyle`).

**`setSelection` Updates (The Context Logic):**
When selection changes, check the `type` property of the first selected tree.

1.  **If Type == "Note" (or 0):**
    *   Set `data1Label` text to "Note".
    *   **Install Smart Lambda:**
        ```cpp
        data1Slider.textFromValueFunction = [](double val) { return MidiNoteUtilities::getMidiNoteName((int)val); };
        data1Slider.valueFromTextFunction = [](juce::String text) { return MidiNoteUtilities::getMidiNoteFromText(text); };
        ```
    *   Call `data1Slider.updateText()` to refresh the view immediately.

2.  **If Type == "CC" (or 1):**
    *   Set `data1Label` text to "CC Number".
    *   **Remove Smart Lambda (Revert to default):**
        ```cpp
        data1Slider.textFromValueFunction = nullptr;
        data1Slider.valueFromTextFunction = nullptr;
        ```
    *   Call `data1Slider.updateText()`.

3.  **If Type == "Macro" (or 2):**
    *   Disable/Hide `data1Slider` (or label it "Macro ID").

### Step 3: Update `CMakeLists.txt`
Add `Source/MidiNoteUtilities.cpp` to `target_sources`.

---

**Generate code for: `MidiNoteUtilities`, updated `MappingInspector`, and the `CMakeLists.txt` snippet.**