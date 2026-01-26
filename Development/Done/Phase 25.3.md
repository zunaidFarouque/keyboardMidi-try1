# 🤖 Cursor Prompt: Phase 25.3 - Fix RPN Sync (MIDI Flood Protection)

**Role:** Expert C++ Audio Developer (MIDI Protocol).

**Context:**
*   **Current State:** Pitch Bend Range sync fails for ranges > 2. The synth stays at 2.
*   **Diagnosis:** The "Sync" button sends ~100 MIDI messages instantly (16 channels x 6 CCs). This likely overflows the VST's input buffer, causing it to drop the critical "Data Entry" messages.
*   **Goal:** Refactor `MidiEngine::sendPitchBendRangeRPN` to be more robust.

**Strict Constraints:**
1.  **Ordering:** Send LSB (`100`) then MSB (`101`).
2.  **Optimization:** Do **not** send the "Reset RPN" (Null) messages immediately after setting. While technically correct spec-wise, it doubles the message count and increases the risk of the "Set Value" message being dropped or overwritten before processing.
    *   *Correction:* We will remove the Null RPN reset for now to ensure the Data Entry sticks.
3.  **API:** Keep the signature simple.

---

### Step 1: Update `MidiEngine.cpp`

Refactor `sendPitchBendRangeRPN`.

**New Logic:**
```cpp
void MidiEngine::sendPitchBendRangeRPN(int channel, int rangeSemitones)
{
    if (!currentOutput) return;

    // RPN Setup: Pitch Bend Sensitivity is 00 00
    // Order: LSB (100) then MSB (101) is often safer for legacy/strict parsers
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 100, 0));
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 101, 0));

    // Data Entry: Set the Range
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 6, rangeSemitones));
    currentOutput->sendMessageNow(juce::MidiMessage::controllerEvent(channel, 38, 0)); // Cents = 0

    // NOTE: We are intentionally SKIPPING the "Null RPN" reset (101=127, 100=127).
    // Sending it immediately can sometimes interrupt the Data Entry processing in some VSTs 
    // if the buffer is processed fast. Leaving RPN 00 selected is harmless.
}
```

### Step 2: Update `SettingsPanel.cpp`

Refactor the Button Click to be slightly slower?
*   Actually, with the reduced message count (4 per channel instead of 6), the total is 64 messages. This is manageable.
*   **Add Logging:**
    ```cpp
    DBG("Sending RPN Range " + String(range) + " to all channels...");
    ```

---

**Generate code for: Updated `MidiEngine.cpp` and `SettingsPanel.cpp`.**