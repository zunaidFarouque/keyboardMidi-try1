### 📋 Cursor Prompt: Phase 53.3 - Fix Layer Lookup & UI Status

**Target File:** `Source/InputProcessor.cpp`

**Task:**
Update the main processing loop and helper functions to correctly respect the new `layerLatchedState` and `layerMomentaryCounts` vectors.

**Specific Instructions:**

1.  **Update `processEvent` Loop Condition:**
    Locate the main loop iterating `layerIdx` from 8 down to 0.
    **Current (Likely Broken):** It might still be checking the old boolean vector or `isActive()`.
    **Fix:** Use the `isLayerActive` helper.
    ```cpp
    // Inside processEvent, inside the loop:
    if (!isLayerActive(layerIdx)) continue;
    ```

2.  **Update `getActiveLayerNames` (Fixes Status Bar):**
    This function generates the text "LAYERS: Base | Layer 1" for the Visualizer. It needs to read the new vectors.
    ```cpp
    juce::StringArray InputProcessor::getActiveLayerNames() {
        juce::StringArray result;
        juce::ScopedReadLock lock(mapLock);
        
        // Iterate 0 to 8 (Order matters for display)
        for (int i = 0; i < 9; ++i) {
            if (isLayerActive(i)) {
                // Get name from PresetManager (or cache? PresetManager is safe here)
                auto layerNode = presetManager.getLayerNode(i);
                juce::String name = layerNode.getProperty("name", "Layer " + juce::String(i));
                
                // Optional: Add indicator for (Hold) vs (Latch)
                if (layerMomentaryCounts[i] > 0) name += " (Hold)";
                else if (i > 0) name += " (On)";
                
                result.add(name);
            }
        }
        return result;
    }
    ```

3.  **Verify `isLayerActive`:**
    Ensure this helper is robust.
    ```cpp
    bool InputProcessor::isLayerActive(int layerIdx) const {
        if (layerIdx == 0) return true; // Base always active
        if (layerIdx < 0 || layerIdx >= 9) return false;
        
        // Active if Latched OR Momentary Count > 0
        return layerLatchedState[layerIdx] || (layerMomentaryCounts[layerIdx] > 0);
    }
    ```

4.  **Add `sendChangeMessage()` call:**
    Ensure that inside `processEvent`, immediately after modifying `layerMomentaryCounts` or `layerLatchedState`, we call `sendChangeMessage()`. This triggers the Visualizer to repaint the Status Bar.

**Why this fixes it:**
*   **Audio:** The loop will now actually "see" Layer 1 when you hold the button, allowing it to find the mapping.
*   **Visuals:** The `getActiveLayerNames` function will now return "Layer 1 (Hold)", so the text on screen will update.