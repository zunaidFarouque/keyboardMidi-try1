### 📋 Cursor Prompt: Phase 50.4 - Visual Baking & Layer Stacking

**Target File:** `Source/GridCompiler.cpp`

**Task:**
Implement the visual "baking" logic in `GridCompiler`. We need to pre-calculate the final visual state for every layer so the UI can just read it.

**Context:**
Currently, the compiler iterates layers 0-9 arbitrarily.
We need to change this to a **Stacking Approach**:
1.  Layer 0 is compiled from scratch.
2.  Layer 1 starts as a **copy** of Layer 0 (inheritance).
3.  Layer 1 applies its own mappings on top (overrides).
4.  Repeat for Layers 2-8.

**Specific Instructions:**

1.  **Add Helper: `getColorForType`**
    Add this private helper function (or lambda inside `compile`) to resolve colors:
    ```cpp
    static juce::Colour getColorForType(ActionType type, SettingsManager& settings) {
        // Use settings.getTypeColor(type) 
        // Fallback to hardcoded defaults if settings are missing
        return settings.getTypeColor(type);
    }
    ```

2.  **Refactor Main Loop (The Stacking Logic):**
    Replace the existing layer iteration in `compile` with this specific structure.
    *Note: We use `visualLookup` to store the state of the grid *at that specific layer*.*

    ```cpp
    // 1. Initialize Visual Lookup Vectors
    for (auto& pair : context->visualLookup) {
        pair.second.resize(9); // Ensure slots 0-8 exist
    }

    // 2. Iterate Layers Bottom-Up (Stacking)
    for (int layerId = 0; layerId < 9; ++layerId) {
        
        // A. Base Initialization (Inheritance)
        if (layerId == 0) {
            // Layer 0 starts empty
            for (auto& pair : context->visualLookup) {
                pair.second[layerId] = std::make_shared<VisualGrid>(); // Empty grid
            }
        } else {
            // Higher layers start as a COPY of the layer below
            for (auto& pair : context->visualLookup) {
                auto prevGrid = pair.second[layerId - 1];
                auto newGrid = std::make_shared<VisualGrid>(*prevGrid); // Copy constructor
                
                // Mark everything copied as INHERITED
                for (auto& slot : *newGrid) {
                    if (slot.state == VisualState::Active || slot.state == VisualState::Override) {
                        slot.state = VisualState::Inherited;
                    }
                }
                pair.second[layerId] = newGrid;
            }
        }

        // B. Apply Zones for this Layer
        // (Call your compileZones helper here, passing layerId)
        // Ensure compileZones writes to context->visualLookup[alias][layerId]
        // If writing to a slot that is NOT VisualState::Empty, set state = VisualState::Override.
        // Otherwise set state = VisualState::Active.

        // C. Apply Manual Mappings for this Layer
        // (Call your compileMappings helper here, passing layerId)
        // Same Override logic applies.
    }
    ```

3.  **Update `compileMappings` (Visual Logic):**
    When writing to `KeyVisualSlot`:
    *   **Color:** Use `getColorForType`.
    *   **Label:**
        *   If `type == ActionType::Note`, use `MidiNoteUtilities::getMidiNoteName(data1)`.
        *   If `type == ActionType::CC`, use `juce::String("CC ") + juce::String(data1)`.
        *   If `type == ActionType::Command`, use a string representation of the Command ID (e.g., "Sustain").
    *   **State:**
        *   Check the *existing* state in the slot (which came from the layer below).
        *   If `existing.state != VisualState::Empty`, set new `state = VisualState::Override`.
        *   Else, `state = VisualState::Active`.

4.  **Handling Generic Modifiers (Visuals):**
    If a mapping is for `VK_SHIFT` (0x10), you must write to **both** `0xA0` (Left) and `0xA1` (Right) in the Visual Grid, using the same Override logic.

**Visual Verification:**
This logic ensures that if I map "Q" on Layer 0, and view Layer 5, I see "Q" as `Inherited`. If I then map "Q" on Layer 5, I see the new action as `Active` (or `Override`).