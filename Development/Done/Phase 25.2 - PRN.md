# 🤖 Cursor Prompt: Phase 25.2 - Send Pitch Bend Range (RPN)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26.2 Complete. The app relies on `Global Pitch Bend Range` for Legato and Smart Bends to work correctly.
*   **Problem:** If the external synth (e.g., HALion) is set to a different range (e.g., 2 semitones) than MIDIQy (e.g., 12), the pitch math will be wrong.
*   **Phase Goal:** Implement a button to send the **RPN (Registered Parameter Number)** sequence to automatically configure the synth's range.

**Strict Constraints:**
1.  **MIDI Standards:** The sequence for Pitch Bend Sensitivity is:
    *   CC 101 (RPN MSB) = 0
    *   CC 100 (RPN LSB) = 0
    *   CC 6 (Data Entry MSB) = Range (Semitones)
    *   CC 38 (Data Entry LSB) = 0 (Cents)
    *   *Cleanup:* CC 101 = 127, CC 100 = 127 (Null RPN) to close the editing state.
2.  **Broadcasting:** Since Zones can use any channel, the button should send this RPN to **All 16 MIDI Channels**.

---

### Step 1: Update `MidiEngine` (`Source/MidiEngine.h/cpp`)
Add the helper method.

**Method:**
`void sendPitchBendRangeRPN(int channel, int rangeSemitones);`

**Implementation:**
```cpp
void MidiEngine::sendPitchBendRangeRPN(int channel, int rangeSemitones)
{
    if (!currentOutput) return;

    // 1. Select RPN 00 00 (Pitch Bend Sensitivity)
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 101, 0));
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 100, 0));

    // 2. Set Value (MSB = Semitones, LSB = Cents)
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 6, rangeSemitones));
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 38, 0));

    // 3. Reset RPN (Null) to prevent accidental edits
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 101, 127));
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 100, 127));
}
```

### Step 2: Update `SettingsPanel` (`Source/SettingsPanel.cpp`)
Add the trigger button.

**Requirements:**
1.  **UI:** Add `juce::TextButton sendRpnButton;` ("Sync Range to Synth").
2.  **Dependencies:** Needs reference to `MidiEngine`. (If not present, pass it in constructor or access via `MainComponent` integration. *Correction: `MainComponent` holds `MidiEngine`. Pass `MidiEngine&` to `SettingsPanel` constructor.*)
3.  **Logic (`onClick`):**
    *   Get global range from `settingsManager`.
    *   Loop `ch = 1` to `16`.
    *   Call `midiEngine.sendPitchBendRangeRPN(ch, range)`.
    *   (Optional) Show a small "Sent!" text or toast.

### Step 3: Integration
*   Update `MainComponent` to pass `midiEngine` to `SettingsPanel`.

---

**Generate code for: Updated `MidiEngine.h/cpp`, Updated `SettingsPanel.h/cpp`, and Updated `MainComponent.cpp`.**