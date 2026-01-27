# 🤖 Cursor Prompt: Phase 30.1 - Background UI Suspension

**Role:** Expert C++ Audio Developer (Optimization).

**Context:**
*   **Current State:** Phase 30 Complete. UI uses Dirty Flags.
*   **Problem:** When the app is minimized (running in background), spamming keys still causes high CPU usage (~9%).
*   **Diagnosis:** `VisualizerComponent::vBlankCallback` and `MainComponent::timerCallback` (Logging) continue to run logic and string formatting even when the window is hidden.
*   **Phase Goal:** Suspend all UI-related processing when the window is Minimized (`isIconic`).

**Strict Constraints:**
1.  **Detection:** Use `getPeer()->isIconic()` to detect the minimized state on Windows. Ensure `getPeer()` is checked for null.
2.  **Logging:** If minimized, the pending event queue in `MainComponent` must be **Cleared** (discarded) to prevent it from growing infinitely while the user plays. Do NOT process strings.
3.  **Visualizer:** If minimized, `vBlankCallback` should return immediately.

---

### Step 1: Update `VisualizerComponent.cpp`
Stop the loop.

**Refactor `vBlankCallback`:**
Add this to the very top:
```cpp
void VisualizerComponent::vBlankCallback()
{
    // OPTIMIZATION: Stop all processing if window is minimized
    if (auto* peer = getPeer())
        if (peer->isIconic())
            return;

    // ... existing polling and repaint logic ...
}
```

### Step 2: Update `MainComponent.cpp`
Stop the logger.

**Refactor `timerCallback`:**
```cpp
void MainComponent::timerCallback()
{
    // Check Minimization State
    bool isMinimized = false;
    if (auto* peer = getPeer())
        isMinimized = peer->isIconic();

    // 1. Swap Queue (Thread Safe)
    std::vector<PendingEvent> tempQueue;
    {
        juce::ScopedLock sl(queueLock);
        tempQueue.swap(eventQueue); 
        // eventQueue is now empty, Input Thread is free to write
    }

    // 2. OPTIMIZATION: If minimized, discard data and abort.
    // We do not want to format strings or update TextEditors for invisible logs.
    if (isMinimized) 
        return; 

    // 3. Process Queue (Only if Visible)
    for (const auto& ev : tempQueue)
    {
        // ... existing logEvent / simulateInput logic ...
    }
}
```

---

**Generate code for: Updated `VisualizerComponent.cpp` and `MainComponent.cpp`.**