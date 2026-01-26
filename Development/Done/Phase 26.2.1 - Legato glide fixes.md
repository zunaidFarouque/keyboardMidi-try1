Here is the prompt for **Phase 26.2.1**.

This focuses on fixing critical bugs in the Legato glide implementation discovered after Phase 26.2.

***

# 🤖 Cursor Prompt: Phase 26.2.1 - Legato Glide Bug Fixes

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 26.2 Complete. Legato glide implemented but has bugs.
*   **Issue 1:** Glide happens after a 5-second delay and sends MIDI every 5 seconds instead of smoothly.
*   **Issue 2:** When releasing a note in Legato mode, it resets PB instantly instead of gliding back to the previous note.

**Phase Goal:**
1. Fix the PortamentoEngine timer interval bug (5ms vs 5000ms).
2. Fix Legato glide back logic to handle notes that were only glided (not added to voices).
3. Ensure proper PB value tracking for smooth handoffs.

**Strict Constraints:**
1. **Timer Interval:** `HighResolutionTimer::startTimer()` expects **milliseconds**, not microseconds. The timer interval constant is already in milliseconds.
2. **Legato Release:** In Legato mode, new notes are added to the mono stack but NOT to `voices` (we return early after gliding PB). When releasing such notes, `releasedChannel` will be -1, so we must check the mono stack directly.
3. **PB Value Tracking:** Always use `portamentoEngine.getCurrentValue()` to get the current PB position, even when the engine is inactive (it preserves the last value).

---

### Step 1: Fix PortamentoEngine Timer Interval (`Source/PortamentoEngine.cpp`)

**Bug:** Timer was set to 5000ms instead of 5ms.

**Fix:**
*   In constructor, change:
    ```cpp
    startTimer(static_cast<int>(timerIntervalMs * 1000.0)); // WRONG - converts to microseconds
    ```
    to:
    ```cpp
    startTimer(static_cast<int>(timerIntervalMs)); // timerIntervalMs is already in milliseconds
    ```
*   `HighResolutionTimer::startTimer()` expects milliseconds, not microseconds.
*   `timerIntervalMs = 5.0` (5 milliseconds), so multiplying by 1000 incorrectly sets it to 5000ms.

### Step 2: Fix Legato Glide Back in `VoiceManager::handleKeyUp` (`Source/VoiceManager.cpp`)

**Bug:** When releasing a note in Legato mode that was only glided (not in `voices`), `releasedChannel` is -1, so the mono/Legato handling is skipped and PB resets instantly instead of gliding back.

**Fix:**
*   In the `if (releasedChannel < 0)` block (background key release handling):
    *   Check if the released source is in any mono stack.
    *   If found in a Legato mode channel:
        *   Remove the source from the stack.
        *   Get the previous note from the stack (if any).
        *   Calculate delta: `previousNote - currentNote` (where `currentNote` is the base note still playing).
        *   Look up target PB: `pbLookup[delta + 127]`.
        *   Get start PB: `portamentoEngine.getCurrentValue()` (always use this, not conditional on `isActive()`).
        *   Ensure glide speed is valid (at least 1ms, fallback to 50ms if needed).
        *   Only start glide if `abs(startPB - targetPB) > 1`.
        *   Call `portamentoEngine.startGlide(startPB, targetPB, returnGlideSpeed, channel)`.

**Key Logic:**
*   In Legato mode, when pressing a new note:
    *   PB glides from current position to new note's PB value.
    *   New note is added to mono stack but NOT to `voices` (we return early).
*   When releasing that note:
    *   No voice exists, so `releasedChannel = -1`.
    *   We must check mono stacks directly to find and handle the release.
    *   Calculate glide back: from current PB position (B's value) to previous note's PB (A's value = 8192 for base note).

### Step 3: Ensure Proper PB Value Usage

**Changes:**
*   Always use `portamentoEngine.getCurrentValue()` instead of conditional `isActive() ? getCurrentValue() : 8192`.
*   The portamento engine preserves `currentPbValue` even when `active = false` (it holds the last target value).
*   Add validation: ensure glide speed is at least 1ms before starting glide.

---

**Expected Behavior After Fix:**
1. **Timer Fix:** Glide starts immediately and sends smooth MIDI updates every 5ms during the glide.
2. **Glide Back Fix:** When releasing a note in Legato mode:
    *   PB smoothly glides back from the released note's position to the previous note's position (8192 for base note).
    *   No instant reset - the glide uses the stored glide speed from the zone.

**Files Modified:**
*   `Source/PortamentoEngine.cpp` - Timer interval fix
*   `Source/VoiceManager.cpp` - Legato glide back logic in `handleKeyUp`
