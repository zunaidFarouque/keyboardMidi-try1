# 🤖 Cursor Prompt: Phase 16.7 - Anti-Ghosting & Autorepeat Filter

**Role:** Expert C++ Audio Developer (Win32 API).

**Context:**
We are building "MIDIQy".
*   **Current State:** The app works well, but physical computer keyboards send repeated "Down" signals when a key is held (Autorepeat).
*   **Problem:** This causes the `VoiceManager` to re-trigger the note repeatedly (machine-gun effect) and messes up the Visualizer.
*   **Goal:** Filter out these repeated events in `RawInputManager` so the app only hears the *first* Down event and the *final* Up event.

**Strict Constraints:**
1.  **Logic Location:** This must be solved in `RawInputManager`. Do not complicate the downstream processors.
2.  **Per-Device Tracking:** We must track key states *per device*. If I hold 'A' on Keyboard 1 and press 'A' on Keyboard 2, both should trigger.
3.  **Performance:** Use efficient lookups (e.g., `std::set` or `std::unordered_set`).

---

### Step 1: Update `RawInputManager.h`
We need a state tracker.

**Add Member:**
*   `std::map<uintptr_t, std::set<int>> deviceKeyStates;`
    *   Key: Device Handle.
    *   Value: Set of currently pressed virtual key codes.

**Update Method:**
*   `void resetState();` (Clears the map - useful for panic/reset).

### Step 2: Update `RawInputManager.cpp`
Implement the filtering logic inside the message processing loop.

**Logic Update (Inside `rawInputWndProc` or internal helper):**

1.  **Identify Event:** You have `deviceHandle`, `vKey`, and `isDown` (derived from `RI_KEY_BREAK` flag).
2.  **Access State:** Get the `std::set` for this `deviceHandle`.
3.  **Filter Logic:**
    *   **Case: Key Down**
        *   Check `stateSet.contains(vKey)`.
        *   **If True:** It is an autorepeat. **IGNORE** (Return immediately, do not broadcast).
        *   **If False:** It is a fresh press. Insert `vKey` into set. **BROADCAST**.
    *   **Case: Key Up**
        *   Check `stateSet.contains(vKey)`.
        *   **If True:** It is a valid release. Remove `vKey` from set. **BROADCAST**.
        *   **If False:** (Edge case, maybe app started with key held). Broadcast anyway to be safe (ensure NoteOff is sent).

### Step 3: Safety Cleanup
*   In `shutdown()`, clear the `deviceKeyStates` map.
*   (Optional) If `RI_KEY_E0` or `E1` flags complicate things (extended keys), ensure the `vKey` is normalized correctly before tracking (usually `MapVirtualKey` helps, but raw VKey from `RAWKEYBOARD` is usually sufficient for this specific filter).

---

**Generate code for: Updated `RawInputManager.h` and `RawInputManager.cpp`.**