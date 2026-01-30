### 📋 Cursor Prompt: Phase 54.1 - Visual Polish (Contrast & Ghosts)

**Target Files:**
1.  `Source/MappingTypes.h`
2.  `Source/GridCompiler.cpp`
3.  `Source/VisualizerComponent.cpp`

**Task:**
Improve the Visualizer's rendering quality:
1.  **Smart Contrast:** Automatically choose Black or White text based on the key's background color.
2.  **Ghost Keys:** Visually distinguish keys that trigger "Ghost Notes" (quieter, passing tones) in Smart Zones.

**Specific Instructions:**

1.  **Update `Source/MappingTypes.h`:**
    *   Add `bool isGhost = false;` to the `KeyVisualSlot` struct.

2.  **Update `Source/GridCompiler.cpp`:**
    *   In `compileZonesForLayer`:
        *   When processing a key, you retrieve `chordNotes` (the vector of `ChordNote`).
        *   Check the **primary note** (the first one, or the one corresponding to the key press).
        *   If `!chordNotes.empty() && chordNotes[0].isGhost`, set `slot.isGhost = true;`.

3.  **Update `Source/VisualizerComponent.cpp`:**
    *   In `refreshCache` (inside the drawing loop):
        *   **Ghost Logic:**
            *   If `slot.isGhost && slot.state == VisualState::Active`, reduce `alpha` (e.g., multiply by `0.5f`).
            *   (Inherited ghost keys will be `0.3f * 0.5f` = very dim, which is correct).
        *   **Smart Contrast:**
            *   Calculate brightness: `float brightness = backColor.getPerceivedBrightness();`
            *   *Correction:* Use the *final* color logic (accounting for dark red conflict background).
            *   If `slot.state == VisualState::Conflict`: `textColor = juce::Colours::white;` (on Dark Red).
            *   Else: `textColor = (brightness > 0.5f) ? juce::Colours::black : juce::Colours::white;`
        *   **Draw Text:** Use `g.setColour(textColor)` before drawing the label.

**Goal:**
The interface should now look professional.
*   Yellow/White keys get Black text.
*   Blue/Red/Black keys get White text.
*   "Ghost" scale degrees (like passing notes in a Blues scale) appear visually distinct (dimmed).