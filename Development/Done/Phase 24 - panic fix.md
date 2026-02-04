# 🤖 Cursor Prompt: Phase 24 - Panic Fix & UI Resizing

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:**
    1.  `VoiceManager::panic()` might rely only on `CC 123` (All Notes Off), which some synths ignore, or it fails to clear internal "Sustained" states, leaving notes stuck in the engine logic.
    2.  `MainComponent` has a split layout (Left: Tabbed Editors, Right: Inspector/Log), but the divider is static. The user cannot drag it to resize the panels.

**Phase Goal:**
1.  **Hard Panic:** Update `VoiceManager` to manually send `NoteOff` messages for *every* active/sustained voice before clearing the state.
2.  **Resizable UI:** Implement `juce::StretchableLayoutResizerBar` in `MainComponent` to allow resizing the horizontal split.

**Strict Constraints:**
1.  **Panic Logic:**
    *   Iterate `voices`. Send `NoteOff` for **every** entry (Playing, Sustained, Latched).
    *   Clear `voices` container.
    *   Clear `activeNoteNumbers` map.
    *   Reset `globalSustainActive` and `globalLatchActive` to `false`.
    *   Send `CC 123` (All Notes Off) as a backup.
2.  **UI Layout:**
    *   Use `juce::StretchableLayoutManager` for the horizontal split in `MainComponent`.
    *   The Resizer Bar must be visible (e.g., 4-6px wide).

---

### Step 1: Update `VoiceManager` (`Source/VoiceManager.cpp`)
Fix the Panic.

**Refactor `panic()`:**
```cpp
void VoiceManager::panic()
{
    // 1. Manually kill every tracked note (Robust)
    for (const auto& voice : voices)
    {
        // Use the channel and note from the voice state
        auto msg = juce::MidiMessage::noteOff(voice.midiChannel, voice.noteNumber, 0.0f);
        midiEngine.sendMidiMessage(msg);
    }

    // 2. Clear Internal State
    voices.clear();
    activeNoteNumbers.clear();
    
    // 3. Reset Performance Flags
    globalSustainActive = false;
    globalLatchActive = false;

    // 4. Send MIDI Panic (Backup)
    midiEngine.sendAllNotesOff(1); // Should ideally loop channels 1-16
    for (int ch = 1; ch <= 16; ++ch) 
        midiEngine.sendAllNotesOff(ch);
        
    DBG("VoiceManager: HARD PANIC Executed.");
}
```

### Step 2: Update `MainComponent` (`Source/MainComponent.h`)
Add layout objects.

**Members:**
*   `juce::StretchableLayoutManager horizontalLayout;`
*   `juce::StretchableLayoutResizerBar resizerBar { &horizontalLayout, 1, true };` (Item index 1, vertical bar).

### Step 3: Update `MainComponent` (`Source/MainComponent.cpp`)
Implement the resize logic.

**1. Constructor:**
*   `addAndMakeVisible(resizerBar);`
*   **Setup Layout:**
    *   `horizontalLayout.setItemLayout(0, -0.1, -0.9, -0.7);` // Item 0 (Left/Editors): Min 10%, Max 90%, Preferred 70%
    *   `horizontalLayout.setItemLayout(1, 5, 5, 5);`          // Item 1 (Bar): Fixed 5px width
    *   `horizontalLayout.setItemLayout(2, -0.1, -0.9, -0.3);` // Item 2 (Right/Inspector): Min 10%, Max 90%, Preferred 30%

**2. `resized()`:**
*   Define the area for the bottom section (below Visualizer and Header).
*   `Component* comps[] = { &editorContainer, &resizerBar, &inspectorViewport };` (Assuming these are the wrappers).
*   `horizontalLayout.layOutComponents(comps, 3, bottomArea.getX(), bottomArea.getY(), bottomArea.getWidth(), bottomArea.getHeight(), false, true);`

---

**Generate code for: Updated `VoiceManager.cpp` and Updated `MainComponent.h/cpp`.**