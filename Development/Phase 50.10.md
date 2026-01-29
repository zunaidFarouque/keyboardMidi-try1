### 📋 Cursor Prompt: Phase 50.10 - Fix Visual States (Conflicts & Overrides)

**Target Files:**
1.  `Source/GridCompiler.cpp`
2.  `Source/VisualizerComponent.cpp`

**Task:**
Fix the Visual State logic.
1.  **Fix Overrides:** Ensure "Active" zones are not marked as "Override" unless they actually overlap a mapping from a lower layer or Global scope.
2.  **Fix Conflicts:** Ensure conflicts (multiple mappings on the same key in the same layer) are rendered with a bright red backdrop and text.

**Specific Instructions:**

1.  **Update `Source/GridCompiler.cpp`:**
    Refactor the `applyLayerContent` lambda and the main loop to ensure strict state transitions.

    *   **Loop Logic (Stacking):**
        When creating a new layer grid from the previous layer:
        ```cpp
        // If L > 0, copy L-1. 
        // CRITICAL: Convert 'Active'/'Override'/'Conflict' from L-1 to 'Inherited' in L.
        // 'Empty' remains 'Empty'.
        for (auto& slot : *grid) {
            if (slot.state != VisualState::Empty) {
                slot.state = VisualState::Inherited;
            }
        }
        ```

    *   **Loop Logic (Device Merge):**
        When creating a Device Grid at Layer L:
        *   Start with a copy of `visualLookup[0][L]` (The Global Grid at this layer).
        *   **Do NOT** change states here. If Global was `Active`, Device sees `Active`. If Global was `Inherited`, Device sees `Inherited`.
        *   *Then* apply Device Mappings.

    *   **`compileZonesForLayer` / `compileMappingsForLayer` Logic:**
        Use `std::vector<bool>& touchedKeys` to detect conflicts.
        
        **State Transition Logic:**
        ```cpp
        // 1. Conflict Check
        if (touchedKeys[keyCode]) {
            slot.state = VisualState::Conflict;
            slot.displayColor = juce::Colours::red; // Force Red
            slot.label = "(!)"; // Optional warning
            continue;
        }
        
        // 2. Mark Touched
        touchedKeys[keyCode] = true;

        // 3. Determine State
        if (slot.state == VisualState::Empty) {
            slot.state = VisualState::Active;
        } else {
            // It was Inherited (from Layer N-1) OR Active (from Global at Layer N)
            // In both cases, writing here is an Override.
            slot.state = VisualState::Override;
        }
        ```

2.  **Update `Source/VisualizerComponent.cpp` (`refreshCache`):**
    Explicitly handle the `Conflict` rendering style to be aggressive.

    ```cpp
    // Inside the drawing loop...
    juce::Colour backColor = slot.displayColor;
    juce::Colour borderColor = juce::Colours::grey;
    juce::Colour textColor = juce::Colours::white;
    float borderThick = 1.0f;
    float alpha = 1.0f;

    switch (slot.state) {
        case VisualState::Empty:
            // Draw nothing or placeholder
            break;
        case VisualState::Inherited:
            alpha = 0.3f; // Dim
            break;
        case VisualState::Active:
            alpha = 1.0f; // Full brightness
            break;
        case VisualState::Override:
            borderColor = juce::Colours::orange;
            borderThick = 2.5f;
            break;
        case VisualState::Conflict:
            backColor = juce::Colours::red;
            borderColor = juce::Colours::yellow; // High contrast
            textColor = juce::Colours::white;
            borderThick = 3.0f;
            alpha = 1.0f;
            break;
    }

    // Draw background
    if (slot.state != VisualState::Empty) {
        g.setColour(backColor.withAlpha(alpha));
        g.fillRect(fullBounds);
    }
    
    // Draw Border
    g.setColour(borderColor);
    g.drawRoundedRectangle(keyBounds, 6.0f, borderThick);
    
    // Draw Text
    g.setColour(textColor);
    // ...
    ```

**Correction on "False Positive Override":**
The issue likely was that `Device` grid initialization didn't respect the `Active` -> `Active` preservation from Global.
If `Global[L]` has a zone (`Active`), and we copy it to `Device[L]`.
Then we apply `Device` mappings.
If we *don't* map that key in Device, it stays `Active`. **Correct.**
If we *do* map that key in Device, it sees `Active` and becomes `Override`. **Correct.**

The false positive likely came from the **Vertical Stack** step:
If Layer 0 is `Active`, Layer 1 becomes `Inherited`.
If we view Layer 1, we see `Inherited` (Dim).
If the user claimed "All zone showing override", they might be confusing "Active" (Normal) with "Override".
**Check:** Ensure `VisualState::Active` draws with a standard Grey border, NOT Orange. Only `Override` gets Orange.