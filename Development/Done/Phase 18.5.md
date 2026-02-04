# 🤖 Cursor Prompt: Phase 18.5 - Voicing Logic Debug Export

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.4 Complete. We have complex voicing logic (Smooth, Adaptive Guitar, etc.).
*   **Goal:** Verify the logic by exporting a comprehensive text report.
*   **Output:** A file `MIDIQy_Voicings.txt` containing the generated notes for every combination of Voicing, Chord Type, and Scale Degree.

**Strict Constraints:**
1.  **Test Conditions:**
    *   **Scale:** C Major (Standard Reference).
    *   **Zone Anchor/Root:** C4 (MIDI 60).
    *   **Input Root:** C4 (MIDI 60).
2.  **Formatting:** Output must be human-readable. Show MIDI Numbers AND Note Names (e.g., `[48, 60, 64] (C3, C4, E4)`).
3.  **Trigger:** Add a temporary button or menu item in `MainComponent` or just run it once on startup for this phase.

---

### Step 1: Update `ChordUtilities` (`Source/ChordUtilities.h/cpp`)
Add the reporter function.

**Static Method:**
`static void dumpDebugReport(juce::File targetFile);`

**Implementation Logic:**
1.  **Setup:**
    *   Define C Major Intervals `{0, 2, 4, 5, 7, 9, 11}`.
    *   Define Anchor `C4` (60).
2.  **Loops:**
    *   Iterate **Voicing Strategies** (Root, Smooth, Open, GuitarSpread).
    *   Iterate **Chord Types** (Triad, Seventh, Ninth).
    *   Iterate **Scale Degrees** (0 to 6).
3.  **Generation:**
    *   Calculate the base note for the degree (e.g., Degree 0 = C4, Degree 1 = D4).
    *   Call `generateChord(baseNote, intervals, degree, type, voicing, anchor)`.
    *   *Note:* Ensure you pass the `anchor` (C4) correctly to test the Adaptive/Smooth logic.
4.  **Formatting:**
    *   Convert vector `[60, 64, 67]` to string.
    *   Convert to Note Names using `MidiNoteUtilities::getMidiNoteName`.
    *   Append to a `juce::StringOutputStream`.
5.  **Write:** Save to `targetFile`.

### Step 2: Trigger (`MainComponent.cpp`)
For easy access, add a **"Debug" Menu** to the window or a temporary button.

**Implementation:**
*   In `MainComponent` constructor (or a MenuBarModel), add a call/button:
*   `"Export Voicing Report"` -> Calls `ChordUtilities::dumpDebugReport(juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("MIDIQy_Voicings.txt"));`

---

**Generate code for: `ChordUtilities.h/cpp` (New Method) and `MainComponent.cpp` (Trigger).**