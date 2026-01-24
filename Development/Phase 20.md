# 🤖 Cursor Prompt: Phase 20 - Performance Stability & Global Control

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 19 Complete.
*   **Issue 1 (Sustain Bug):** If a "Sustain Inverse" mapping is deleted, Sustain stays ON forever.
*   **Issue 2 (Global Control):** Users cannot map keys to change the Global Transpose (Pitch/Degree) or see these values in the UI.

**Phase Goal:**
1.  Implement a "State Flush" mechanism when mappings change to reset/re-evaluate Performance states.
2.  Add Global Transpose commands to the Mapping system.
3.  Update the Visualizer HUD to display Global Transpose stats.

**Strict Constraints:**
1.  **State Logic:** When `InputProcessor` rebuilds the map:
    *   First, Reset Sustain/Latch to `false`.
    *   Then, Scan the new map. If **ANY** key is mapped to `SustainInverse`, immediately set Sustain to `true`.
2.  **Thread Safety:** `VoiceManager` state changes happen on the Message Thread (during Config) or Input Thread (during Play). Use existing locks if necessary, but `rebuildMap` usually happens under lock.

---

### Step 1: Update `MappingTypes.h`
Add new Command IDs.

**Update `CommandID` Enum:**
*   Add: `GlobalPitchUp` (Chromatic +1)
*   Add: `GlobalPitchDown` (Chromatic -1)
*   Add: `GlobalModeUp` (Degree +1)
*   Add: `GlobalModeDown` (Degree -1)

### Step 2: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Add a reset capability.

**Requirements:**
1.  **Method:** `void resetPerformanceState();`
    *   Set `globalSustainActive = false;`
    *   Set `globalLatchActive = false;`
    *   (Optional) Kill "Sustained" voices, but keep "Playing" voices? For safety, just resetting the flags is usually enough, `handleNoteOff` will handle the rest naturally, or you can call `panic()` if you want a hard reset. *Decision: Just reset flags.*

### Step 3: Update `InputProcessor` (`Source/InputProcessor.cpp`)
Implement the Logic Fix and New Commands.

**1. Update `rebuildMapFromTree`:**
*   Call `voiceManager.resetPerformanceState()`.
*   **Scan Logic:** Iterate over `keyMapping`.
    *   Check if any value is `Command` AND `SustainInverse`.
    *   If found: `voiceManager.setSustain(true);` and `break`.

**2. Update `processEvent` (Command Handling):**
*   **Case `GlobalPitchUp`:** `zoneManager.setGlobalTranspose(chrom + 1, deg)`
*   **Case `GlobalPitchDown`:** `zoneManager.setGlobalTranspose(chrom - 1, deg)`
*   **Case `GlobalModeUp`:** `zoneManager.setGlobalTranspose(chrom, deg + 1)`
*   **Case `GlobalModeDown`:** `zoneManager.setGlobalTranspose(chrom, deg - 1)`
    *   *Note:* You need getters in `ZoneManager` to read current values first.

### Step 4: Update `ZoneManager` (`Source/ZoneManager.h`)
Ensure accessors exist.
*   `int getGlobalChromaticTranspose() const;`
*   `int getGlobalDegreeTranspose() const;`

### Step 5: Update `VisualizerComponent` (`Source/VisualizerComponent.cpp`)
Enhance the HUD.

**Update `paint`:**
*   In the Header Bar (Top area), draw text:
    *   `"TRANSPOSE: [Pitch: +0] [Scale: +0]"`
    *   Use `zoneManager.getGlobalChromaticTranspose()` and `getGlobalDegreeTranspose()`.
    *   Draw this on the Left side (Sustain is on the Right).

### Step 6: Update `MappingInspector.cpp`
*   Add the new Commands to the `commandSelector` list in the constructor so users can select them.

---

**Generate code for: `MappingTypes.h`, `VoiceManager.h/cpp` (Reset logic), `InputProcessor.cpp` (Rebuild fix & Commands), `ZoneManager.h`, `VisualizerComponent.cpp`, and `MappingInspector.cpp`.**