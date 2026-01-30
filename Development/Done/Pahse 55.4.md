Here is the Cursor prompt for **Phase 55.4**. This connects the new UI settings to the actual Audio Engine behavior.

### 📋 Cursor Prompt: Phase 55.4 - Audio Engine Logic Update

**Target Files:**
1.  `Source/MappingTypes.h`
2.  `Source/GridCompiler.cpp`
3.  `Source/InputProcessor.cpp`

**Task:**
Update the Audio Engine to respect the new properties defined in the Schema (`followTranspose`, `sendNoteOff`, `sendReleaseValue`).

**Specific Instructions:**

1.  **Update `Source/MappingTypes.h`:**
    *   Add fields to `MidiAction` struct:
        ```cpp
        // Note Options
        bool isOneShot = false; // If true, do not send NoteOff on key release

        // CC Options
        bool sendReleaseValue = false; // If true, send a specific value on key release
        int releaseValue = 0;
        ```

2.  **Update `Source/GridCompiler.cpp`:**
    *   In `compileMappingsForLayer`:
        *   **For `ActionType::Note`:**
            *   Read `followTranspose` (default false).
            *   If `true`, add `zoneMgr.getGlobalChromaticTranspose()` to `data1` (Pitch). *Clamp to 0-127.*
            *   Read `sendNoteOff` (default true).
            *   Set `action.isOneShot = !sendNoteOff;` (Inverted logic).
        *   **For `ActionType::CC`:**
            *   Read `sendReleaseValue` (default false).
            *   Read `releaseValue` (default 0).
            *   Set `action.sendReleaseValue` and `action.releaseValue`.

3.  **Update `Source/InputProcessor.cpp`:**
    *   In `processEvent`:
        *   **Handle Key Release (`!isDown`):**
            *   We currently have `auto [action, source] = lookupAction...`.
            *   **Check Note Release:**
                *   If `action.type == ActionType::Note`:
                *   If `action.isOneShot`, **return early** (Do not call `voiceManager.handleKeyUp`).
            *   **Check CC Release:**
                *   If `action.type == ActionType::CC`:
                *   If `action.sendReleaseValue`:
                    *   `voiceManager.sendCC(action.channel, action.data1, action.releaseValue);`
                *   *(Note: Previously CCs did nothing on release).*

**Outcome:**
*   **Notes:** If you toggle "Ignore Global Transpose" (default), the note stays fixed even if you change the Zone Transpose. If you uncheck "Send Note Off", the note acts like a "One Shot" sample trigger.
*   **CCs:** You can now configure a button to send 127 on Press and 0 on Release (Momentary CC).