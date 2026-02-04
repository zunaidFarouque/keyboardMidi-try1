# 🤖 Cursor Prompt: Phase 10 - Zone Engine & Scale Logic

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 9.2 Complete. Inputs are processed via `InputProcessor` -> `DeviceManager` (Alias Lookup) -> `MappingTable`.
*   **Phase Goal:** Implement "Smart Zones" where a range of keys automatically maps to a musical scale, prioritizing this over manual mappings.

**Strict Constraints:**
1.  **Architecture:** `ScaleUtilities` must be a standalone helper.
2.  **Logic:** Zones must respect the `Device/Alias` system. A zone belongs to a specific Device Alias.
3.  **CMake:** Provide the snippet to add the new files.

---

### Step 1: `ScaleUtilities` (`Source/ScaleUtilities.h/cpp`)
Create a static helper class for musical math.

**Requirements:**
1.  **Enum:** `enum class ScaleType { Chromatic, Major, Minor, PentatonicMajor, PentatonicMinor, Blues }`.
2.  **Data:** Define the semitone intervals for these scales.
    *   Example Major: `{0, 2, 4, 5, 7, 9, 11}`.
3.  **Method:** `static int calculateMidiNote(int rootNote, ScaleType scale, int degreeIndex);`
    *   **Math:**
        *   `scaleSize` = number of intervals in scale.
        *   `octaveShift` = `degreeIndex / scaleSize`.
        *   `noteIndex` = `degreeIndex % scaleSize`.
        *   `result` = `rootNote + (octaveShift * 12) + intervals[noteIndex]`.

### Step 2: `Zone` (`Source/Zone.h/cpp`)
A logic container for a group of keys.

**Requirements:**
1.  **Members:**
    *   `juce::String name;`
    *   `uintptr_t targetAliasHash;` (The Alias this zone listens to).
    *   `std::vector<int> inputKeyCodes;` (The physical keys, ordered).
    *   `int rootNote;` (Base MIDI note).
    *   `ScaleUtilities::ScaleType scale;`
    *   `int chromaticOffset;` (Global transpose override).
    *   `int degreeOffset;` (Scale degree shift).
    *   `bool isTransposeLocked;` (If true, ignore global transpose).
2.  **Method:** `std::optional<MidiAction> processKey(InputID input, int globalChromTrans, int globalDegTrans);`
    *   **Check 1:** Does `input.deviceHandle` match `targetAliasHash`? If no, return `nullopt`.
    *   **Check 2:** Find index of `input.keyCode` in `inputKeyCodes`. If not found, return `nullopt`.
    *   **Math:**
        *   `effDegTrans` = `isTransposeLocked ? 0 : globalDegTrans`.
        *   `effChromTrans` = `isTransposeLocked ? 0 : globalChromTrans`.
        *   `finalDegree` = `index + degreeOffset + effDegTrans`.
        *   `baseNote` = `ScaleUtilities::calculateMidiNote(rootNote, scale, finalDegree)`.
        *   `finalNote` = `baseNote + chromaticOffset + effChromTrans`.
    *   **Result:** Return `MidiAction` (Type: Note, Channel: 1, Data1: `finalNote`, Data2: 100).

### Step 3: `ZoneManager` (`Source/ZoneManager.h/cpp`)
Manages the collection of zones.

**Requirements:**
1.  **Inheritance:** `juce::ChangeBroadcaster`.
2.  **Members:**
    *   `std::vector<std::shared_ptr<Zone>> zones;`
    *   `int globalChromaticTranspose = 0;`
    *   `int globalDegreeTranspose = 0;`
    *   `juce::ReadWriteLock zoneLock;` (Thread safety).
3.  **Methods:**
    *   `void addZone(std::shared_ptr<Zone> zone);`
    *   `std::optional<MidiAction> handleInput(InputID input);`
        *   Acquire Read Lock.
        *   Iterate zones. Call `processKey`.
        *   Return the first valid result found.
    *   `void setGlobalTranspose(int chromatic, int degree);`

### Step 4: Update `InputProcessor`
Integrate the Zone logic.

1.  **Members:** Add `ZoneManager zoneManager;`.
2.  **Logic Update (`processEvent`):**
    *   **Step A:** Process Input Alias (Resolve hardware ID to Alias Hash via `DeviceManager`).
    *   **Step B (Zone Priority):**
        *   Construct `InputID` with the **Alias Hash**.
        *   Call `zoneManager.handleInput(aliasInputID)`.
        *   If result valid: Trigger `voiceManager.noteOn(...)` and **RETURN** (skip Step C).
    *   **Step C (Fallback):**
        *   Perform the existing `keyMapping` lookup.

### Step 5: Update `MainComponent` (Verification)
In the constructor, create a hardcoded Test Zone to verify logic.
*   **Target:** "Master Input" (Alias Hash 0).
*   **Keys:** Q, W, E, R (Virtual codes: 0x51, 0x57, 0x45, 0x52).
*   **Scale:** Major.
*   **Root:** 60.

### Step 6: Update `CMakeLists.txt`
Add `Source/ScaleUtilities.cpp`, `Source/Zone.cpp`, and `Source/ZoneManager.cpp`.

---

**Generate code for: `ScaleUtilities`, `Zone`, `ZoneManager`, updated `InputProcessor`, updated `MainComponent`, and `CMakeLists.txt`.**