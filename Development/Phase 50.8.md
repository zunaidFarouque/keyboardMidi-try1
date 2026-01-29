### 📋 Cursor Prompt: Phase 50.8 - Conflict Detection & Visualization

**Target Files:**
1.  `Source/GridCompiler.cpp`
2.  `Source/VisualizerComponent.cpp`

**Task:**
Implement logic to detect when a key is assigned multiple times within the same layer and device scope (e.g., Zone + Mapping, or Mapping + Mapping). When a conflict is detected, mark the slot as `VisualState::Conflict` and render it with a red warning style.

**Specific Instructions:**

1.  **Update `Source/GridCompiler.cpp`:**
    Modify the `applyLayerContent` lambda (or the logic inside the main loop) to track which keys have been written to *during the current layer pass*.

    *   **Logic:**
        Create a `std::vector<bool> touchedKeys(256, false);` at the start of `applyLayerContent`.
    *   **Inside `compileZonesForLayer` and `compileMappingsForLayer`:**
        When writing to a `KeyVisualSlot`:
        1.  Check `if (touchedKeys[keyCode])`.
        2.  **If true (Conflict):**
            *   Set `slot.state = VisualState::Conflict;`
            *   Set `slot.displayColor = juce::Colours::darkred;` (Dark Red Backdrop)
            *   Set `slot.label += " (!)";` (Optional indicator)
        3.  **If false (First write):**
            *   Set `touchedKeys[keyCode] = true;`
            *   Determine state:
                *   If `slot.state == VisualState::Empty`, set `Active`.
                *   If `slot.state != VisualState::Empty` (inherited from below), set `Override`.
            *   Write Color and Label normally.

    *   *Note:* You may need to refactor `compileZonesForLayer` / `compileMappingsForLayer` to accept the `touchedKeys` vector by reference so they can share the tracking state.

2.  **Update `Source/VisualizerComponent.cpp`:**
    Modify the `refreshCache` method to handle the `Conflict` state rendering.

    *   **Inside the key iteration loop:**
        ```cpp
        // Determine styling based on state
        juce::Colour backColor = slot.displayColor;
        juce::Colour borderColor = juce::Colours::grey;
        juce::Colour textColor = juce::Colours::white;
        float borderThick = 1.0f;
        float alpha = 1.0f;

        if (slot.state == VisualState::Inherited) {
            alpha = 0.3f;
        } 
        else if (slot.state == VisualState::Override) {
            borderColor = juce::Colours::orange;
            borderThick = 2.0f;
        }
        else if (slot.state == VisualState::Conflict) {
            // "Red Backdrop, Red Text" style
            backColor = juce::Colours::darkred; // Darker red background
            borderColor = juce::Colours::red;   // Bright red border
            textColor = juce::Colours::red;     // Bright red text
            borderThick = 2.5f;
            alpha = 1.0f;
        }

        // Apply Alpha to background
        if (!backColor.isTransparent()) {
            g.setColour(backColor.withAlpha(alpha));
            g.fillRect(fullBounds);
        }
        
        // ... Draw Border using borderColor & borderThick ...
        // ... Draw Text using textColor ...
        ```

**Goal:**
If the user assigns "Q" to a Zone AND adds a Manual Mapping for "Q" in the same layer, the Visualizer must immediately turn that key Red to indicate the ambiguity.