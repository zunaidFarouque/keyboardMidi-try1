# 🤖 Cursor Prompt: Phase 26.1 - Adaptive Glide (Lock-Free Implementation)

**Role:** Expert C++ Audio Developer (Real-time Systems Optimization).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26 Complete. We have a static Legato engine.
*   **Phase Goal:** Implement **Adaptive Glide**. The system must calculate the user's playing speed (Inter-Onset Interval) and adjust the Portamento time dynamically (faster playing = tighter glide).

**Strict Constraints (Performance):**
1.  **Zero Allocation:** Use `std::array` (Stack memory) for the history buffer. No `std::vector`.
2.  **Lock-Free:** Use `std::atomic<int>` to store the calculated average. The Reader (`processEvent`) must never block waiting for the Writer.
3.  **Math:** Calculate the "Speed" as the Moving Average of the last 8 note intervals.

---

### Step 1: `RhythmAnalyzer` (`Source/RhythmAnalyzer.h`)
The high-performance analysis engine.

**Requirements:**
1.  **Members:**
    *   `std::array<int, 8> intervals;` (Circular buffer of deltas in ms).
    *   `int writeIndex = 0;`
    *   `int64 lastTimeMs = 0;`
    *   `std::atomic<int> currentAverageMs { 200 };` (The result).
2.  **Methods:**
    *   `void logTap();`
        *   Get `now`. Calculate `delta = now - lastTimeMs`.
        *   Ignore massive deltas (> 2000ms) to reset logic.
        *   Write to buffer. Update `writeIndex`.
        *   **Recalculate Average:** Sum buffer / 8. Store in `currentAverageMs`.
    *   `int getAdaptiveSpeed(int minMs, int maxMs);`
        *   Load `currentAverageMs`.
        *   Apply Safety Factor: `Target = Average * 0.7f`. (Glide should be faster than the notes).
        *   Return `jlimit(minMs, maxMs, Target)`.

### Step 2: Update `Zone` (`Source/Zone.h`)
Add Adaptive Settings.

**Requirements:**
1.  **Members:**
    *   `bool isAdaptiveGlide = false;`
    *   `int maxGlideTimeMs = 200;`
    *   *Note:* The existing `glideTimeMs` acts as the "Static Time" OR "Min Time" depending on mode.
2.  **Serialization:** Save/Load these.

### Step 3: Update `InputProcessor.cpp`
Integrate the Analyzer.

**1. Member:** Add `RhythmAnalyzer rhythmAnalyzer;`.
**2. Logic (`processEvent`):**
    *   **If Note On (Down):**
        *   Call `rhythmAnalyzer.logTap()`.
        *   **Calculate Speed:**
            *   If `zone->isAdaptiveGlide`: `speed = rhythmAnalyzer.getAdaptiveSpeed(zone->glideTimeMs, zone->maxGlideTimeMs);`
            *   Else: `speed = zone->glideTimeMs;`
        *   **Pass Speed:** Update `voiceManager.noteOn` to accept this `int glideSpeed` argument.

### Step 4: Update `VoiceManager.cpp`
Pass the data through.

**Update `noteOn`:**
*   Accept `int glideSpeed` argument.
*   Pass this specific value to `portamentoEngine.startGlide(...)` instead of reading `zone->glideTimeMs` directly.

### Step 5: Update `ZonePropertiesPanel.cpp`
UI Controls.

**Updates:**
1.  **Toggle:** "Adaptive" (Visible if Legato).
2.  **Sliders:**
    *   Existing Slider Label -> "Time / Min".
    *   New Slider -> "Max Time" (Visible if Adaptive).

---

**Generate code for: `RhythmAnalyzer` (Header/Implementation), `Zone` (Updates), `InputProcessor` (Logic), `VoiceManager` (Signature Update), and `ZonePropertiesPanel` (UI).**