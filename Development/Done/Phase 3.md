# 🤖 Cursor Prompt: Phase 3 - Event Processing & Voice Management

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 2 Complete. Input and Output work, but logic is hardcoded in `MainComponent`.
*   **Phase Goal:** Implement `InputProcessor` (Mapping Logic) and `VoiceManager` (Note-Off Safety).

**Strict Constraints:**
1.  **No Windows Headers:** `MappingTypes.h` must not include `<windows.h>`. Store device handles as `uintptr_t` or `uint64_t`.
2.  **CMake:** You **must** provide the snippet to update `CMakeLists.txt` with the new source files.

---

### Step 1: Define Data Structures (`Source/MappingTypes.h`)
Create a header file to define the app's data types.

**Requirements:**
1.  **`enum class ActionType`**: `Note`, `CC`, `Macro`.
2.  **`struct MidiAction`**:
    *   `ActionType type;`
    *   `int channel;`, `int data1;` (Note/CC), `int data2;` (Vel/Val).
3.  **`struct InputID`**:
    *   `uintptr_t deviceHandle;` (Use `uintptr_t` to store the raw pointer safely without OS headers).
    *   `int keyCode;`
    *   **Crucial:** Implement `operator==` and a `std::hash<InputID>` specialization so this can be used as a `std::unordered_map` key.

### Step 2: Implement `VoiceManager` (`Source/VoiceManager.h/cpp`)
This class prevents stuck notes by tracking what is currently playing.

**Requirements:**
1.  **Dependency:** Needs a reference to `MidiEngine` (to send messages).
2.  **Storage:** `std::unordered_multimap<InputID, juce::MidiMessage> activeNotes;`
    *   *Note:* Stores the **Note Off** message required to stop the sound.
3.  **Methods:**
    *   `void noteOn(InputID source, int note, int vel, int channel)`: Sends NoteOn via engine, creates corresponding NoteOff, adds to map.
    *   `void handleKeyUp(InputID source)`: Finds ALL entries for this `InputID`, sends their stored NoteOff messages, and removes them.
    *   `void panic()`: Sends `allNotesOff` and clears map.

### Step 3: Implement `InputProcessor` (`Source/InputProcessor.h/cpp`)
The brain that looks up keys and tells the VoiceManager what to do.

**Requirements:**
1.  **Dependency:** Needs reference to `VoiceManager`.
2.  **Storage:** `std::unordered_map<InputID, MidiAction> keyMapping;`
3.  **Methods:**
    *   `void processEvent(InputID input, bool isDown)`:
        *   If `isDown`: Look up `input`. If found, trigger `voiceManager.noteOn(...)`.
        *   If `!isDown`: Call `voiceManager.handleKeyUp(input)`.
    *   `void loadDebugMappings()`: Manually insert mappings for testing.
        *   *Use raw hex codes for keys (e.g., 0x51 for 'Q') to avoid Windows headers.*

### Step 4: Update `CMakeLists.txt` (CRITICAL)
Add `Source/VoiceManager.cpp` and `Source/InputProcessor.cpp` to `target_sources`.

### Step 5: Integration (`MainComponent`)
1.  **Members:** Add `VoiceManager` and `InputProcessor` instances.
    *   *Order matters:* `MidiEngine` -> `VoiceManager` -> `InputProcessor`.
2.  **Constructor:**
    *   Pass `midiEngine` to `VoiceManager`.
    *   Pass `voiceManager` to `InputProcessor`.
    *   Call `inputProcessor.loadDebugMappings()`.
3.  **Callback:** Update `RawInputManager` callback:
    *   Convert `void* dev` to `uintptr_t`.
    *   Construct `InputID`.
    *   Call `inputProcessor.processEvent(...)`.

---

**Generate the code for all new files and the updates for MainComponent and CMakeLists.**