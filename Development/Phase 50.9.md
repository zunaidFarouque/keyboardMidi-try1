### 📋 Cursor Prompt: Phase 50.9 - Async Dynamic View (Performance Optimized)

**Target Files:**
1.  `Source/VisualizerComponent.h`
2.  `Source/VisualizerComponent.cpp`

**Task:**
Implement "Dynamic View" using a thread-safe, non-blocking architecture. The Visualizer must poll for state changes at 30Hz instead of reacting synchronously to input events.

**Specific Instructions:**

1.  **Update `VisualizerComponent.h`:**
    *   Change inheritance: Inherit from `juce::Timer` (if not already).
    *   **Remove** `std::unique_ptr<juce::VBlankAttachment> vBlankAttachment`. (We will use the Timer for 30fps capping).
    *   **Add Atomics (The Mailbox):**
        ```cpp
        std::atomic<uintptr_t> lastInputDeviceHandle{0}; // Written by Input Thread
        std::atomic<bool> dynamicViewEnabled{false};     // Cached toggle state
        ```
    *   **Add/Update Overrides:**
        ```cpp
        void timerCallback() override;
        ```

2.  **Update `VisualizerComponent.cpp`:**

    *   **Constructor:**
        *   Start the timer: `startTimer(33);` (approx 30 FPS).
        *   Remove `vBlankAttachment` initialization.
        *   Initialize `dynamicViewToggle`:
            *   `onClick`: Update `dynamicViewEnabled` atomic immediately.

    *   **Update `handleRawKeyEvent` (The Hot Path):**
        *   **CRITICAL:** Remove ALL logic except updating the atomic.
        *   Logic:
            ```cpp
            if (isDown) {
                lastInputDeviceHandle.store(deviceHandle, std::memory_order_relaxed);
            }
            // Existing activeKeys logic (keep it, it's fast enough if protected by lock)
            {
                juce::ScopedLock lock(keyStateLock);
                if (isDown) activeKeys.insert(keyCode);
                else activeKeys.erase(keyCode);
            }
            // Mark dirty, but DO NOT repaint here
            needsRepaint.store(true, std::memory_order_release);
            ```

    *   **Implement `timerCallback` (The UI Path):**
        *   **Step 1: Check Dynamic View Logic**
            ```cpp
            if (dynamicViewEnabled.load()) {
                // A. Device Switching
                uintptr_t handle = lastInputDeviceHandle.load();
                // Perform the HEAVY string lookup here (Message Thread), not in Input Thread
                // ... logic to find aliasHash from handle ...
                // If diff -> update currentViewHash -> cacheValid = false

                // B. Layer Switching
                if (inputProcessor) {
                    int activeLayer = inputProcessor->getHighestActiveLayerIndex();
                    if (currentVisualizedLayer != activeLayer) {
                        currentVisualizedLayer = activeLayer;
                        cacheValid = false;
                    }
                }
            }
            ```
        *   **Step 2: Check Dirty Flags**
            ```cpp
            // Check external Sustain state
            bool sus = voiceManager.isSustainActive();
            if (lastSustainState != sus) { lastSustainState = sus; needsRepaint = true; }

            // Rebuild Cache if invalid
            if (!cacheValid) refreshCache();

            // Repaint if needed
            if (needsRepaint.exchange(false)) {
                repaint();
            }
            ```

**Outcome:**
The Input Thread never waits for the UI. The UI updates smoothly at 30fps, handling all heavy lookups and drawing operations lazily.