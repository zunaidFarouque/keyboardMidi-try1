### 📋 Cursor Prompt: Phase 56.1 - Core Refactor (Expression Consolidation)

**Target Files:**
1.  `Source/MappingTypes.h`
2.  `Source/PresetManager.cpp`
3.  `Source/GridCompiler.cpp`
4.  `Source/ExpressionEngine.h`
5.  `Source/ExpressionEngine.cpp`
6.  `Source/MappingDefinition.cpp` (Basic fix to allow compilation)

**Task:**
Consolidate `CC` and `Envelope` types into a single `ActionType::Expression`. Implement backward compatibility (migration) and the "Fast Path" optimization for simple CCs.

**Specific Instructions:**

1.  **Update `Source/MappingTypes.h`:**
    *   **Update Enum:** Remove `ActionType::CC` and `ActionType::Envelope`. Add `ActionType::Expression`.
    *   **Update Struct:** Add `bool useCustomEnvelope = false;` to `AdsrSettings` (or `MidiAction` if easier, but `AdsrSettings` is cleaner). Let's put it in `AdsrSettings`.

2.  **Update `Source/PresetManager.cpp`:**
    *   **Implement Migration:** inside `loadFromFile` (after XML parsing, before `rootNode.copyPropertiesFrom`):
    *   Iterate all children recursively. If a node is "Mapping":
        *   Get `type`.
        *   **If "CC" (or int 1):** Change `type` to "Expression". Set `useCustomEnvelope` to `false`. Set `adsrTarget` to "CC".
        *   **If "Envelope" (or int 4):** Change `type` to "Expression". Set `useCustomEnvelope` to `true`.
        *   *Note:* Ensure you handle legacy `data1` (CC Num) mapping to the new structure correctly (GridCompiler handles the mapping, but PresetManager just renames the type).

3.  **Update `Source/GridCompiler.cpp`:**
    *   **Update `compileMappingsForLayer`:**
        *   Remove `case CC` and `case Envelope`.
        *   Add `case Expression`:
            *   Read `adsrTarget` (default "CC").
            *   Read `useCustomEnvelope` (default `false`).
            *   **Logic:**
                *   If `!useCustomEnvelope`: Force ADSR to `0, 0, 1.0f, 0`. Force `isOneShot = false`.
                *   Else: Read ADSR sliders from ValueTree.
            *   **Target Logic:**
                *   If `adsrTarget == "CC"`: `ccNumber` comes from `data1`. `peak` comes from `data2`.
                *   If `adsrTarget == "PitchBend"`: `peak` comes from `data2` (or calculated 16383).
    *   **Update Visuals:**
        *   If `ActionType::Expression`: Label should be "Expr: [Target]".

4.  **Update `Source/ExpressionEngine.cpp`:**
    *   **Implement Fast Path:** In `triggerEnvelope`:
        *   Check `settings`: If `attackMs == 0` and `decayMs == 0` and `releaseMs == 0`:
            *   **Direct Send:**
                *   If CC: `midiEngine.sendCC(channel, ccNumber, peakValue);`
                *   If PB: `midiEngine.sendPitchBend(channel, peakValue);`
            *   **Return immediately** (Do not add to active envelopes list).
    *   **Update `releaseEnvelope`:**
        *   If it was a Fast Path CC (not in active list), verify if we need to send a release value.
        *   *Correction:* `ExpressionEngine` handles *curves*. Simple Note-Off handling for CC buttons is currently handled in `InputProcessor` (Phase 55.4).
        *   *Task:* Ensure `InputProcessor` calls `releaseEnvelope` for `Expression` type.
        *   *Optimization:* If `ExpressionEngine` doesn't find the envelope (because it was Fast Path), we still might need to send the "Release Value" (0 or specific). `InputProcessor` should handle the "Release Value" logic for Expressions just like it did for CCs in Phase 55.4.

5.  **Update `Source/MappingDefinition.cpp` & `MappingInspector.cpp`:**
    *   Replace `ActionType::CC` and `Envelope` references with `Expression` to fix compilation errors. (We will redesign the UI schema in the next Phase).

**Goal:**
The code compiles. Loading an old preset converts CCs to "Simple Expressions". Pressing a "Simple Expression" key triggers the Fast Path (no latency).