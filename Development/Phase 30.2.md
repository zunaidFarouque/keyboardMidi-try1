# 🤖 Cursor Prompt: Phase 30.2 - Visualizer Graphics Caching

**Role:** Expert C++ Audio Developer (JUCE Graphics Optimization).

**Context:**
*   **Current State:** `VisualizerComponent` redraws all ~104 keys every frame (60fps), even if they are idle.
*   **Problem:** High CPU usage due to redundant vector/text rendering of static elements.
*   **Phase Goal:** Implement an `Image Cache` system. Draw the static keyboard once to an image, then only draw active keys on top during the animation loop.

**Strict Constraints:**
1.  **Cache Validity:** Re-render the cache ONLY when: `resized()`, `changeListenerCallback` (Zone/Transpose changes), or `repaint()` is called with a specific "Force Refresh" flag (if needed).
2.  **Paint Logic:**
    *   Draw Cache (Background).
    *   Iterate `activeKeys` set.
    *   Draw *only* the active (Yellow) state for those specific keys on top.
3.  **Sustain/Latch:** Since Latch state changes visuals (Cyan), any change in Latch/Sustain needs to potentially update the visualization. For simplicity/safety in this phase, we can treat "Latched" keys as "Active" for the purpose of drawing on top, or invalidating the cache if needed. *Decision: Treat Latched/Active keys as dynamic layers drawn on top of the static cache.*

---

### Step 1: Update `VisualizerComponent.h`
Add caching members.

**Requirements:**
1.  `juce::Image backgroundCache;`
2.  `bool cacheValid = false;`
3.  `void refreshCache();` (Private helper).

### Step 2: Update `VisualizerComponent.cpp`
Implement the optimized pipeline.

**1. `refreshCache()`:**
*   Create `backgroundCache` matching component size (`Image::ARGB`, width, height, clear=true).
*   Create `Graphics g(backgroundCache)`.
*   **Run the existing loop (Iterate 104 keys):**
    *   Draw Underlay (Zone Color).
    *   Draw Body (Dark Grey "Off" State).
    *   Draw Border.
    *   Draw Text.
*   Set `cacheValid = true`.

**2. `paint(Graphics& g)`:**
*   **Check Cache:** `if (!cacheValid || backgroundCache.isNull()) refreshCache();`
*   **Draw Background:** `g.drawImageAt(backgroundCache, 0, 0);`
*   **Draw Dynamic Keys (The Overlay):**
    *   Iterate `activeKeys` (and check `voiceManager.isKeyLatched` to collect latched keys too).
    *   For each active/latched key:
        *   Calculate Bounds (reuse `KeyboardLayoutUtils`).
        *   Draw the "Pressed" (Yellow) or "Latched" (Cyan) state logic.
        *   **Important:** You must Redraw the Text on top of the yellow key, otherwise the highlight covers the label.

**3. Triggers (`resized`, `changeListenerCallback`):**
*   Set `cacheValid = false;`
*   `needsRepaint = true;`

---

**Generate code for: Updated `VisualizerComponent.h` and `VisualizerComponent.cpp`.**