### 📋 Cursor Prompt: Phase 50.3 - Zone Compilation & Chord Baking

**Target Files:**
1.  `Source/GridCompiler.h`
2.  `Source/GridCompiler.cpp`

**Task:**
Extend the `GridCompiler` to handle Zones. We need to "bake" the dynamic Zone logic (Scales, Chords, Transpositions) into the static `AudioGrid` and `VisualGrid`.

**Logic Change:**
To ensure correct priority (Manual Mappings > Zones, and Later Zones > Earlier Zones), we will modify the `compile` flow:
1.  Clear Grids.
2.  **Process Zones First.**
3.  **Process Manual Mappings Second** (Overwriting any Zone data for that key).

**Specific Instructions:**

1.  **Update `GridCompiler.h`:**
    *   Update `compile` signature to include `SettingsManager&` (needed for colors later, but good to add now).
    *   Add private helper: `static void compileZones(CompiledMapContext& context, ZoneManager& zoneMgr);`

2.  **Update `GridCompiler.cpp`:**
    *   **Refactor `compile`:** Move the "Manual Mapping" loop *after* the new `compileZones` call.
    *   **Implement `compileZones`:**
        1.  Iterate through all Zones from `zoneMgr.getZones()`.
        2.  For each Zone:
            *   Get `layerID` and `targetAliasHash`.
            *   Get `inputKeyCodes` (the physical keys).
            *   **Iterate Keys:** For each key code:
                *   **Calculate Audio:** Call `zone->getNotesForKey(key, ...)` using the global transpose values from `zoneMgr`.
                *   **Chord Pooling:**
                    *   If result is **Empty**: Do nothing.
                    *   If result has **1 Note**: Write directly to `KeyAudioSlot` (`action`, `isActive=true`, `chordIndex=-1`).
                    *   If result has **>1 Notes**:
                        *   Create a `std::vector<MidiAction>` representing the chord.
                        *   Add to `context.chordPool`.
                        *   Write to `KeyAudioSlot` (`isActive=true`, `chordIndex = pool.size()-1`).
                *   **Visuals:**
                    *   Write to `KeyVisualSlot` in the correct Layer/Alias grid.
                    *   `displayColor` = `zone->zoneColor`.
                    *   `label` = `zone->getKeyLabel(key)`.
                    *   `sourceName` = "Zone: " + `zone->name`.
                    *   `state` = `VisualState::Active`.

3.  **Handle Targeting Logic (Critical):**
    *   If `zone->targetAliasHash == 0` (Global Zone):
        *   Write to `globalGrid`.
        *   **AND** write to *every* `deviceGrid` (unless that device grid already has a specific value for this key? *Decision: Zones overwrite lower zones. Since we iterate zones in order, just overwrite.*)
    *   If `zone->targetAliasHash != 0` (Specific Zone):
        *   Write only to the specific `deviceGrid` and `visualLookup` entry.

4.  **Handling Modifiers in Zones:**
    *   Just like Manual Mappings, if a Zone uses a Generic Modifier key (unlikely but possible), replicate it to L/R slots in the Grid.

**Important:**
Ensure `KeyAudioSlot` logic handles the `chordIndex` correctly. If `chordIndex >= 0`, the `action` field in the slot can be used to store the *Root Note* of the chord (useful for simple visualization or fallback), while the actual playback uses the pool.