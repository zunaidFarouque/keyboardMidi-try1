# 🤖 Cursor Prompt: Phase 26 - Mono Engine & Portamento

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 25.1 Complete. The app is strictly Polyphonic.
*   **Phase Goal:** Implement **Mono Mode** (Last-Note Priority) and **Legato Mode** (Pitch Glide).
*   **Key Behavior:**
    1.  **Stack:** When a new note is pressed in Mono mode, it takes over. When released, the previous held note resumes.
    2.  **Glide:** In Legato mode, if the interval is within the `Global Pitch Bend Range`, we do **not** retrigger. We slide the Pitch Bend from the Old Note's pitch to the New Note's pitch.

**Strict Constraints:**
1.  **Performance:** Use `juce::HighResolutionTimer` for the glide.
2.  **Compiler Strategy:** `VoiceManager` must pre-calculate a lookup table (`std::array<int, 255>`) mapping `SemitoneDelta` -> `PitchBendValue` based on `SettingsManager`. This avoids math during the note transition.
3.  **State Logic:** Keep the `BaseNote` active during a Legato glide. Do not send `NoteOff` + `NoteOn` unless the interval exceeds the PB range.

---

### Step 1: Update `MappingTypes.h` & `Zone.h`
Define the modes.

**1. `MappingTypes.h`:**
*   `enum class PolyphonyMode { Poly, Mono, Legato };`

**2. `Zone.h`:**
*   Members: `PolyphonyMode polyphonyMode = PolyphonyMode::Poly;`, `int glideTimeMs = 50;`.
*   Serialization: Save/Load these properties.

### Step 2: `PortamentoEngine` (`Source/PortamentoEngine.h/cpp`)
The timing logic.

**Requirements:**
1.  **Inheritance:** `juce::HighResolutionTimer`.
2.  **Members:**
    *   `double currentPbValue;`, `double targetPbValue;`, `double step;`.
    *   `int midiChannel;`.
    *   `bool active = false;`.
    *   Ref to `MidiEngine`.
3.  **Methods:**
    *   `startGlide(int startVal, int endVal, int durationMs, int channel);`
    *   `stop();`
    *   `hiResTimerCallback`: Interpolate -> Send PitchBend -> Stop if reached.

### Step 3: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Implement the Stack and Lookup Table.

**1. Lookup Table:**
*   Member: `std::array<int, 255> pbLookup;` (Maps `Delta + 127` to `PB Value 0-16383`).
*   Method: `rebuildPbLookup();` (Called when `SettingsManager` changes).
*   Logic: For `i` from -127 to +127: Calculate PB value required to shift by `i` semitones given Global Range. If `i` exceeds range, store sentinel `-1`.

**2. The Stack:**
*   Member: `std::deque<ActiveVoice> monoStack;` (For Mono/Legato zones).
*   *Note:* You might need a map of Stacks if supporting multiple Mono zones at once. For MVP, assume one active Mono zone or map `ZoneID -> Stack`.

**3. Logic Update (`noteOn`):**
*   If `Poly`: Standard logic.
*   If `Mono/Legato`:
    *   Push to Stack.
    *   Calculate `delta = newNote - currentPlayingNote`.
    *   **Check Legato:** If `Mode == Legato` AND `pbLookup[delta]` is valid:
        *   **Do NOT NoteOn.**
        *   Call `portamentoEngine.startGlide(currentPB, targetPB, zone->glideTimeMs, ...)`.
    *   **Else (Retrigger):**
        *   NoteOff `currentPlayingNote`.
        *   Reset PB to Center (8192).
        *   NoteOn `newNote`.

**4. Logic Update (`handleKeyUp`):**
*   If `Poly`: Standard logic.
*   If `Mono/Legato`:
    *   Remove from Stack.
    *   If Stack empty: NoteOff current.
    *   If Stack has items:
        *   `back()` is the note we return to.
        *   Perform similar Legato/Retrigger check (Glide back or NoteOff+On).

### Step 4: Update `ZonePropertiesPanel`
UI Controls.

**Requirements:**
1.  **Polyphony Combo:** "Poly", "Mono (Retrigger)", "Legato (Glide)".
2.  **Glide Slider:** 0-500ms (Visible only if Legato).
3.  **Logic:** Update `currentZone`.

---

**Generate code for: `MappingTypes.h`, `Zone.h/cpp`, `PortamentoEngine.h/cpp`, `VoiceManager.h/cpp` (Logic heavy), and `ZonePropertiesPanel.cpp`.**