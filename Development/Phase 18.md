
# 🤖 Cursor Prompt: Phase 18 - Performance Engine (Sustain/Latch Backend)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 17.3 Complete. Notes stop immediately upon Key Up.
*   **Phase Goal:** Implement the backend logic for **Sustain** (holding notes), **Latch** (toggling notes), and **Panic**, driven by a unified Mapping system.

**Strict Constraints:**
1.  **Architecture:** Do not create a separate `CommandManager`. Use the existing `keyMapping` system. A key can map to a Note *or* a Command.
2.  **Zone Safety:** `VoiceManager` must respect a new per-zone flag `allowSustain` (so Drums don't sustain).
3.  **Thread Safety:** `VoiceManager` state is accessed by Input Thread and Audio Thread. Ensure `noteOn`/`noteOff` logic is robust.

---

### Step 1: Update `MappingTypes.h` (`Source/MappingTypes.h`)
Expand the data model to support Commands.

**Requirements:**
1.  **Update `ActionType`:** Add `Command`.
2.  **New Enum `CommandID`:**
    *   `SustainMomentary` (Press=On, Release=Off).
    *   `SustainToggle` (Press=Flip).
    *   `SustainInverse` (Press=Off, Release=On - Palm Mute).
    *   `LatchToggle` (Global Latch Mode).
    *   `Panic` (All Notes Off).
    *   `PanicLatch` (Kill only Latched notes).

### Step 2: Update `Zone` (`Source/Zone.h/cpp`)
Add the permission flag.

**Requirements:**
1.  **Member:** `bool allowSustain = true;`
2.  **Serialization:** Save/Load this boolean in `toValueTree`/`fromValueTree`.

### Step 3: Refactor `VoiceManager` (`Source/VoiceManager.h/cpp`)
Implement the State Machine.

**1. Data Structure:**
*   Change `activeNoteNumbers` to a `struct ActiveVoice`.
    *   `int noteNumber;`
    *   `int midiChannel;`
    *   `InputID source;`
    *   `bool allowSustain;`
    *   `enum State { Playing, Sustained, Latched } state;`
*   Store these in a `std::vector<ActiveVoice> voices;` (Vector is fine for <128 items).

**2. State Flags:**
*   `bool globalSustainActive = false;`
*   `bool globalLatchActive = false;`

**3. Logic Update (`noteOn`):**
*   **Latch Check:** If `globalLatchActive` is true:
    *   Check if this Note/Channel is already playing/latched from this Source.
    *   If yes: Kill it (NoteOff) and **RETURN** (Toggle behavior).
    *   If no: Add as `State::Playing`.
*   **Normal:** Add voice as `State::Playing`. Store `allowSustain` passed from Zone.

**4. Logic Update (`handleKeyUp`):**
*   Find voice(s) for this InputID.
*   **If `globalLatchActive`:** Do nothing. Change state to `Latched`.
*   **Else If `globalSustainActive` AND `voice.allowSustain`:** Do nothing. Change state to `Sustained`.
*   **Else:** Send NoteOff. Remove voice.

**5. Logic Update (`setSustain(bool active)`):**
*   Update `globalSustainActive`.
*   **If going FALSE (Pedal Release):**
    *   Iterate voices. If state is `Sustained`, send NoteOff and remove.

**6. Logic Update (`panic`):**
*   Clear everything. Send `allNotesOff` to MIDI engine.

### Step 4: Update `InputProcessor` (`Source/InputProcessor.cpp`)
Handle the Command execution.

**Logic Update (`processEvent`):**
1.  Lookup mapping.
2.  **If Type == Command:**
    *   `int cmd = action.data1;`
    *   **SustainMomentary:** `voiceManager.setSustain(isDown);`
    *   **SustainToggle:** If `isDown`, `voiceManager.setSustain(!voiceManager.isSustainActive());`
    *   **SustainInverse:** `voiceManager.setSustain(!isDown);` (Default On, Press=Off).
    *   **LatchToggle:** If `isDown`, toggle latch.
    *   **Panic:** If `isDown`, call `voiceManager.panic()`.
3.  **If Type == Note:**
    *   Pass `zone->allowSustain` to `voiceManager.noteOn`.

---

**Generate code for: `MappingTypes.h`, `Zone` (updates), `VoiceManager` (complete refactor), and `InputProcessor.cpp`.**