### 📋 Cursor Prompt: Phase 53.2 - Implementing Layer State Logic

**Target Files:**
1.  `Source/InputProcessor.h`
2.  `Source/InputProcessor.cpp`

**Task:**
Restore the Layer State Machine logic (`Latched` vs `Momentary`) and handle `ActionType::Command` in `processEvent`.

**Specific Instructions:**

1.  **Update `Source/InputProcessor.h`:**
    *   We need to distinguish between a layer being "Latched On" (Persistent) and "Momentary On" (Held Key).
    *   **Replace** `std::vector<bool> layerActiveState;` with:
        ```cpp
        // State Tracking
        std::vector<bool> layerLatchedState;      // 9 bools (Persistent)
        std::vector<int> layerMomentaryCounts;    // 9 ints (Ref counts for held keys)
        
        // Helper to recalculate which layers are actually active
        bool isLayerActive(int layerIdx) const;
        ```

2.  **Update `Source/InputProcessor.cpp`:**

    *   **Constructor:** Initialize vectors to size 9. `layerMomentaryCounts` to 0, `layerLatchedState` to false. Layer 0 is not treated specially here, logic handles it.

    *   **Implement `isLayerActive(int layerIdx)`:**
        ```cpp
        // Layer 0 is always active.
        if (layerIdx == 0) return true;
        if (layerIdx < 0 || layerIdx > 8) return false;
        // Active if Latched OR (Momentary Count > 0)
        return layerLatchedState[layerIdx] || (layerMomentaryCounts[layerIdx] > 0);
        ```

    *   **Update `getHighestActiveLayerIndex`:**
        Use `isLayerActive(i)` instead of the old check.

    *   **Update `processEvent` (The Fix):**
        Inside the `if (slot.isActive)` block:
        
        ```cpp
        const auto& action = slot.action; // Get from slot

        if (action.type == ActionType::Command) {
            int cmd = action.data1;
            int targetLayer = action.data2;
            
            // Handle Layer Commands
            if (cmd == static_cast<int>(OmniKey::CommandID::LayerMomentary)) {
                targetLayer = juce::jlimit(0, 8, targetLayer);
                if (isDown) {
                    layerMomentaryCounts[targetLayer]++;
                } else {
                    layerMomentaryCounts[targetLayer]--;
                    if (layerMomentaryCounts[targetLayer] < 0) layerMomentaryCounts[targetLayer] = 0;
                }
                sendChangeMessage(); // Notify Visualizer
            }
            else if (cmd == static_cast<int>(OmniKey::CommandID::LayerToggle)) {
                if (isDown) { // Toggle on press
                    targetLayer = juce::jlimit(0, 8, targetLayer);
                    layerLatchedState[targetLayer] = !layerLatchedState[targetLayer];
                    sendChangeMessage();
                }
            }
            else if (cmd == static_cast<int>(OmniKey::CommandID::LayerSolo)) {
                if (isDown) {
                    std::fill(layerLatchedState.begin(), layerLatchedState.end(), false);
                    targetLayer = juce::jlimit(0, 8, targetLayer);
                    layerLatchedState[targetLayer] = true;
                    sendChangeMessage();
                }
            }
            // ... Handle Sustain/Panic commands here (forward to VoiceManager) ...
            
            return; // Command handled, stop processing
        }
        
        // Handle Note/CC (Existing Logic)
        if (action.type == ActionType::Note) { ... }
        ```

**Why `layerMomentaryCounts`?**
If you map *two different keys* to "Layer 1 Momentary", and hold both, releasing *one* should not turn off the layer. A reference count handles this naturally.

**Goal:**
This should pass the `LayerMomentarySwitching` test we wrote in 53.1.