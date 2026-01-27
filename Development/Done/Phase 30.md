# 🤖 Cursor Prompt: Phase 30 - UI Rendering Optimization (Dirty State)

**Role:** Expert C++ Audio Developer (JUCE Graphics Optimization).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 29 Complete. The app works well but consumes ~9% CPU when idle.
*   **Diagnosis:** `VisualizerComponent` uses `VBlankAttachment` to call `repaint()` every frame (60fps+), regardless of activity.
*   **Phase Goal:** Implement a **Dirty Flag** pattern to only repaint when visual data has actually changed.

**Strict Constraints:**
1.  **Atomic Flag:** Use `std::atomic<bool> needsRepaint` to coordinate between Input threads and the Message Thread.
2.  **State Tracking:** The Visualizer displays `VoiceManager` states (Sustain/Latch). Since `VoiceManager` doesn't broadcast changes, the Visualizer must **poll** these specific booleans in the VBlank callback and only trigger a repaint if they *differs* from the cached previous frame.
3.  **No Logic Change:** Do not change *how* it draws, only *when* it draws.

---

### Step 1: Update `VisualizerComponent.h`
Add state tracking members.

**Requirements:**
1.  `std::atomic<bool> needsRepaint { true };`
2.  **State Cache:**
    *   `bool lastSustainState = false;`
    *   (Optional) `int lastLatchedKeyCount = 0;` (To detect latch changes, though checking simply `needsRepaint` from key events might cover most, explicit polling is safer for internal state changes).

### Step 2: Update `VisualizerComponent.cpp`
Refactor the triggers.

**1. `vBlankCallback`:**
```cpp
void VisualizerComponent::vBlankCallback()
{
    // 1. Poll External States (VoiceManager)
    bool currentSustain = voiceManager.isSustainActive();
    
    if (currentSustain != lastSustainState)
    {
        lastSustainState = currentSustain;
        needsRepaint = true;
    }

    // 2. Check Dirty Flag
    if (needsRepaint.exchange(false)) // Atomic read-and-reset
    {
        repaint();
    }
}
```

**2. `handleRawKeyEvent`:**
*   Existing logic updates `activeKeys`.
*   **Add:** `needsRepaint = true;`

**3. `changeListenerCallback` (Zone/Device changes):**
*   **Add:** `needsRepaint = true;`

**4. `paint`:**
*   No changes needed, but ensure it doesn't trigger side effects.

---

**Generate code for: Updated `VisualizerComponent.h` and `VisualizerComponent.cpp`.**