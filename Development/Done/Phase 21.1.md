# 🤖 Cursor Prompt: Phase 21.1 - Async UI & Performance Optimization

**Role:** Expert C++ Audio Developer (Real-time Systems Optimization).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 21 Complete.
*   **Problem:** Rapid key presses cause "Catch-up" lag. The system processes Input -> MIDI -> **String Formatting** -> **UI Repaint** synchronously. The UI/String overhead causes backpressure on the Input queue.
*   **Phase Goal:** Decouple the UI from the Input Thread. Implement "Fire-and-Forget" logging and "VSync-synced" visualization.

**Strict Constraints:**
1.  **Input Thread:** Must NEVER allocate strings, format text, or trigger expensive UI repaints directly. It should only push raw POD (Plain Old Data) structs to a queue.
2.  **Visualizer:** Must run on its own heartbeat (VBlank or Timer), independent of input events.
3.  **Logging:** Must happen on a low-priority Timer (30Hz), processing batches of events.

---

### Step 1: Update `VisualizerComponent` (`Source/VisualizerComponent.h/cpp`)
Switch from "Push" to "Pull" rendering.

**1. Header:**
*   Add member: `std::unique_ptr<juce::VBlankAttachment> vBlankAttachment;` (or use `juce::Timer` if VBlank is unavailable/overkill). Let's use `juce::VBlankAttachment` for smoothest animation.
*   Inherit `juce::VBlankAttachment::Listener`.

**2. CPP:**
*   **Constructor:** Initialize `vBlankAttachment` with `this`.
*   **Implement `vBlankCallback()`:** Simply call `repaint()`.
*   **Update `handleRawKeyEvent`:**
    *   **KEEP:** The logic that updates `std::set<int> activeKeys`. (This is fast).
    *   **REMOVE:** The call to `repaint()`. (Let VBlank handle it).
    *   *Note:* Ensure `activeKeys` access is thread-safe (use a CriticalSection if `RawInputManager` runs on a separate thread, otherwise the Message Thread is fine). Assuming RawInput might come from OS thread, add `juce::CriticalSection keyStateLock;` and use it in `paint` and `handleRawKeyEvent`.

### Step 2: Refactor Logging (`MainComponent`)
Move string formatting to a background timer.

**1. Define Data Structure:**
*   Inside `MainComponent.h` (private):
    ```cpp
    struct PendingEvent {
        uintptr_t device;
        int keyCode;
        bool isDown;
        // Do NOT store Strings here. Store raw data.
    };
    ```

**2. Members:**
*   `std::vector<PendingEvent> eventQueue;`
*   `juce::CriticalSection queueLock;`

**3. Refactor `handleRawKeyEvent`:**
*   **Old:** Formatted strings, called `logComponent`.
*   **New:**
    *   Acquire `queueLock`.
    *   `eventQueue.push_back({device, keyCode, isDown});`
    *   Release Lock.
    *   **Return Immediately.** (Input thread is done).

**4. Update `timerCallback`:**
*   (This timer already exists).
*   Acquire `queueLock`.
*   **Swap** `eventQueue` into a local temporary vector (`tempQueue`).
*   Clear `eventQueue`.
*   Release Lock. (Input thread is free to write again).
*   **Process `tempQueue`:**
    *   Loop through events.
    *   Perform the heavy `inputProcessor.simulateInput(...)`.
    *   Format the strings.
    *   Call `logComponent.addEntry`.

### Step 3: Cleanup
*   Verify `InputProcessor` logic is lightweight (it uses `ReadWriteLock`, which is good).
*   Ensure `VisualizerComponent` is not doing heavy math in `paint`.

---

**Generate code for: Updated `VisualizerComponent` (VBlank & Locking), and Updated `MainComponent` (Async Logging Logic).**