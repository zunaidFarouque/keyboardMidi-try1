### 📋 Cursor Prompt: Phase 50.9 - Async Dynamic View & Follow Toggle

**Target Files:**
1.  `Source/InputProcessor.h`
2.  `Source/InputProcessor.cpp`
3.  `Source/VisualizerComponent.h`
4.  `Source/VisualizerComponent.cpp`

**Task:**
Implement the "Dynamic View" feature where the Visualizer follows the active device and layer.
1.  **Performance:** Use a "Mailbox" pattern (Atomics + Timer) so the Input Thread **never** waits for the UI.
2.  **UI:** Add an "Eye" Toggle Button to enable/disable this behavior.

**Specific Instructions:**

1.  **Update `InputProcessor` (State Helper):**
    *   Add `int getHighestActiveLayerIndex() const;`
    *   Implementation: Iterate `layers` from 8 down to 0. Return the index of the first one where `isActive() == true`. Return 0 if none found. Use `ScopedReadLock`.

2.  **Update `VisualizerComponent.h`:**
    *   Ensure it inherits from `juce::Timer`.
    *   **Remove** `vBlankAttachment` (we will use `Timer` at 30Hz for both logic and repainting).
    *   **Add Atomics:**
        ```cpp
        std::atomic<uintptr_t> lastInputDeviceHandle{0}; // Written by Input Thread
        std::atomic<bool> followInputEnabled{false};     // UI State
        ```
    *   **Add UI Element:**
        ```cpp
        juce::DrawableButton followButton{"FollowInput", juce::DrawableButton::ImageFitted};
        ```
    *   **Add Helper:** `void updateFollowButtonIcon();`

3.  **Update `VisualizerComponent.cpp`:**

    *   **Constructor:**
        *   Start Timer (`startTimer(33)`).
        *   **Setup Follow Button:**
            *   Create two `juce::Path` objects for "Eye Open" and "Eye Closed".
            *   Eye Open: An ellipse `(0, 4, 20, 12)` with a circle `(10, 10, 3)` inside.
            *   Eye Closed: The same ellipse with a line through it.
            *   Create `juce::DrawablePath` images from these paths.
            *   Set `followButton.setImages(&openImage, ...)` logic.
            *   *Simplification:* If drawing paths is too verbose, just use text "Follow" with `setColour(juce::TextButton::buttonOnColourId, juce::Colours::green)`. *Actually, use a TextButton with `setClickingTogglesState(true)` labeled "Follow Input" for code clarity, unless you are confident in drawing the Eye path.*
            *   `onClick`: `followInputEnabled = followButton.getToggleState();`

    *   **Positioning (`resized`):**
        *   Place `followButton` to the **left** of `viewSelector` (e.g., width 80).

    *   **`handleRawKeyEvent` (Input Thread - FAST PATH):**
        *   **Delete all existing logic** except `activeKeys` tracking.
        *   **Logic:**
            ```cpp
            if (isDown) {
                // Mailbox: Just drop the ID. No lookups.
                lastInputDeviceHandle.store(deviceHandle, std::memory_order_relaxed);
            }
            {
                juce::ScopedLock lock(keyStateLock);
                if (isDown) activeKeys.insert(keyCode);
                else activeKeys.erase(keyCode);
            }
            needsRepaint.store(true, std::memory_order_release);
            ```

    *   **`timerCallback` (UI Thread - LOGIC PATH):**
        *   **Step 1: Auto-Switching**
            ```cpp
            if (followInputEnabled.load()) {
                // A. Device Switching
                uintptr_t handle = lastInputDeviceHandle.load();
                if (handle != 0) {
                    // NOW it is safe to do string lookups
                    // Find alias hash for 'handle' using DeviceManager
                    // If aliasHash != currentViewHash -> update UI -> cacheValid = false
                }

                // B. Layer Switching
                if (inputProcessor) {
                    int layer = inputProcessor->getHighestActiveLayerIndex();
                    if (currentVisualizedLayer != layer) {
                        setVisualizedLayer(layer); // This invalidates cache
                    }
                }
            }
            ```
        *   **Step 2: Rendering**
            ```cpp
            // Check sustain state (polling)
            bool sus = voiceManager.isSustainActive();
            if (lastSustainState != sus) { lastSustainState = sus; needsRepaint = true; }

            if (!cacheValid) refreshCache();
            if (needsRepaint.exchange(false)) repaint();
            ```

**Summary:** The Input Thread is now wait-free. The "Eye" button controls whether the timer logic overwrites the user's manual view selection.