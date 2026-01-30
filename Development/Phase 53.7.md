### 📋 Cursor Prompt: Phase 53.7 - Thread Safety & Logic Hardening

**Target Files:**
1.  `Source/InputProcessor.h`
2.  `Source/InputProcessor.cpp`

**Task:**
Fix the Data Race on layer state variables. Isolate the Mutable State (Layers) from the Immutable State (Grid Maps).

**Specific Instructions:**

1.  **Update `Source/InputProcessor.h`:**
    *   Add: `mutable juce::CriticalSection stateLock;`
    *   This lock will protect `layerMomentaryCounts` and `layerLatchedState`.

2.  **Update `Source/InputProcessor.cpp`:**

    *   **Refactor `getActiveLayerNames`:**
        *   **Remove** `juce::ScopedReadLock lock(mapLock);`
        *   **Add** `juce::ScopedLock lock(stateLock);`
        *   (We don't need the map to read layer names, we just need the state vectors).

    *   **Refactor `getHighestActiveLayerIndex`:**
        *   **Remove** `juce::ScopedReadLock lock(mapLock);`
        *   **Add** `juce::ScopedLock lock(stateLock);`

    *   **Refactor `isLayerActive`:**
        *   **Note:** This helper is called *inside* `processEvent`. We shouldn't lock *inside* the loop for performance.
        *   **Change:** Make `processEvent` take a **Copy** or **Snapshot** of the active layers at the start.

    *   **Refactor `processEvent` (The Big One):**
        ```cpp
        void InputProcessor::processEvent(InputID input, bool isDown) {
            // 1. Snapshot State (Fast, under State Lock)
            std::vector<bool> activeLayersSnapshot(9, false);
            {
                juce::ScopedLock sl(stateLock);
                // Check IsDown/IsUp updates for Held Keys here? 
                // No, key events update state inside the logic. 
                
                // Just snapshot valid layers for the SEARCH loop
                for(int i=0; i<9; ++i) {
                    activeLayersSnapshot[i] = (i==0) || layerLatchedState[i] || (layerMomentaryCounts[i] > 0);
                }
            }

            // 2. Grid Access (Under Map ReadLock)
            juce::ScopedReadLock ml(mapLock);
            auto ctx = activeContext;
            if (!ctx) return;

            // ... Determine effective device ...

            // 3. Layer Loop
            for (int layerIdx = 8; layerIdx >= 0; --layerIdx) {
                // Use Snapshot
                if (!activeLayersSnapshot[layerIdx]) continue;

                // ... Grid Lookup ...
                const auto& slot = (*grid)[input.keyCode];

                if (slot.isActive) {
                    // COMMAND HANDLING (Writes State)
                    if (slot.action.type == ActionType::Command) {
                        // We must re-acquire State Lock to modify
                        juce::ScopedLock sl(stateLock);
                        
                        int cmd = slot.action.data1;
                        int target = juce::jlimit(0, 8, slot.action.data2);

                        if (cmd == (int)OmniKey::CommandID::LayerMomentary) {
                            if (isDown) layerMomentaryCounts[target]++;
                            else if (layerMomentaryCounts[target] > 0) layerMomentaryCounts[target]--;
                            sendChangeMessage();
                        }
                        // ... Toggle/Solo logic ...
                        return; // Done
                    }
                    
                    // NOTE HANDLING
                    // ...
                    return; // Done
                }
            }
        }
        ```

**Logic Explanation:**
*   We snapshot "Which layers are active?" *before* the loop.
*   If we press a Layer Key, we find it (Layer 0 is in snapshot).
*   We execute the command, which updates the real vectors (under `stateLock`).
*   Next key press: Snapshot sees the new `layerMomentaryCounts`.

**Note:** Ensure `activeLayersSnapshot` handles Layer 0 correctly (always true).