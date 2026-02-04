# 🤖 Cursor Prompt: Phase 2 - MIDI Infrastructure

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 1 is complete. `RawInputManager` logs keyboard events. Architectural rules (`void*` isolation, no `hidpi.h`) are strictly enforced.
*   **Phase Goal:** Implement `MidiEngine` to send MIDI messages to external devices.

**Objective:**
Implement the MIDI Output infrastructure. The user should be able to select a MIDI Output device and trigger notes using specific keyboard keys.

---

### Step 1: Create `MidiEngine` Class
Create `Source/MidiEngine.h` and `Source/MidiEngine.cpp`.

**Requirements:**
1.  **Dependencies:** Use `<JuceHeader.h>`. Do **not** include Windows headers.
2.  **Member Variables:**
    *   `std::unique_ptr<juce::MidiOutput> currentOutput;`
    *   `juce::Array<juce::MidiDeviceInfo> deviceList;` (Cache the device list).
3.  **Methods:**
    *   `juce::StringArray getDeviceNames()`: Updates `deviceList` using `juce::MidiOutput::getAvailableDevices()` and returns names for the UI.
    *   `void setOutputDevice(int deviceIndex)`: Opens the device from the cached list using `juce::MidiOutput::openDevice()`.
    *   `void sendNoteOn(int channel, int note, float velocity)`: Sends `juce::MidiMessage::noteOn`.
    *   `void sendNoteOff(int channel, int note)`: Sends `juce::MidiMessage::noteOff`.
4.  **Safety:** Handle null pointers (if no device is selected).

### Step 2: Update `CMakeLists.txt` (CRITICAL)
*   Add `Source/MidiEngine.cpp` to the `target_sources` list.
*   *Do not change the compiler definitions or libraries (user32 is already linked).*

### Step 3: Update `MainComponent`
**UI Additions:**
1.  Add `juce::ComboBox midiSelector;` to the top.
2.  In constructor: `midiEngine.getDeviceNames()` -> Populate ComboBox -> Set `onChange` lambda to call `midiEngine.setOutputDevice`.

**Logic Integration:**
Modify the `RawInputManager` callback in `MainComponent`.
*   **Constraint:** Do **NOT** include `<windows.h>` here to get `VK_` codes. Use raw hex codes or integer literals.
*   **Logic:**
    *   `0x51` ('Q'): Send Note 60 (Middle C).
    *   `0x57` ('W'): Send Note 62 (D).
*   **Note Off:** Ensure `noteOff` is sent when `isDown` is false.

---

**Generate code for: `MidiEngine.h`, `MidiEngine.cpp`, `MainComponent.h` (update), `MainComponent.cpp` (update), and the snippet for `CMakeLists.txt`.**