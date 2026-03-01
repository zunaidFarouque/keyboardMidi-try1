# Manual Testing: Command Mapping Inspector (MappingInspectorLogic)

Use these steps to verify whether the Command mapping inspector works correctly when you change the command type (Panic, Transpose, Layer, Sustain). The failing tests expect that selecting a command from the dropdown correctly sets the mapping so the key triggers that command.

---

## Prerequisites

1. **Run the app** (build and launch MIDIQy).
2. **Mappings tab**: Go to the **Mappings** tab (or **Mapping / Zones**).
3. **At least one zone** with keys that produce notes (so you can hear sustain/panic effects).

---

## What the Tests Expect

When you select a command from the **Command** dropdown in the Mapping Inspector:

- **Panic** → Pressing the key should silence all notes (data1 = 4).
- **Transpose** → Pressing the key should change transpose (data1 = 6).
- **Layer** → Pressing the key should switch layers (data1 = 10).
- **Sustain** → Pressing the key should hold notes (data1 = 0).

---

## Test 1: Command Category – Panic

1. Go to **Mappings** tab.
2. Select **Layer 0** (or any layer).
3. Add a new mapping: click **Add mapping** or similar.
4. Set **Type** = `Command`.
5. In the **Command** dropdown, select **Panic**.
6. Assign a key (e.g. a key you can easily press, like `P` or `Space`).
7. Play some notes on the keyboard (from a zone) so they sustain.
8. Press the key you mapped to Panic.

**Expected:** All notes stop immediately (panic = All Notes Off).

**If broken:** Nothing happens, or the wrong action occurs.

---

## Test 2: Command Category – Transpose

1. Create a Command mapping (or add a new one).
2. Set **Type** = `Command`.
3. In the **Command** dropdown, select **Transpose**.
4. Set **Transpose** options (e.g. "Up (1 semitone)" or "Set (specific semitones)").
5. Assign a key.
6. Play a note (e.g. C4).
7. Press the transpose key.

**Expected:** The next note you play (or the transpose action) should change pitch by the configured amount (e.g. +1 semitone).

**If broken:** No pitch change, or transpose doesn’t work.

---

## Test 3: Command Category – Layer

1. Create a Command mapping.
2. Set **Type** = `Command`.
3. In the **Command** dropdown, select **Layer**.
4. Set **Target Layer** to Layer 1 (or another layer).
5. Set **Style** to "Hold to switch" or "Toggle layer".
6. **Layer 0**: Map a key (e.g. A) to play note 60.
7. **Layer 1**: Map the same key (A) to play note 72 (or a different note).
8. Press the key that switches to Layer 1.

**Expected:** The same key (A) now plays the Layer 1 note (e.g. 72) instead of 60.

**If broken:** Layer doesn’t switch, or the key still plays the Layer 0 note.

---

## Test 4: Command Category – Sustain

1. Create a Command mapping.
2. Set **Type** = `Command`.
3. In the **Command** dropdown, select **Sustain**.
4. Set **Style** to "Hold to sustain" (or another).
5. Assign a key (e.g. a pedal key or hold key).
6. Hold the sustain key, play a note, release the note (while still holding sustain).

**Expected:** The note keeps sounding until you release the sustain key.

**If broken:** Note stops immediately on release, or sustain doesn’t work.

---

## Test 5: Change Command Type on Existing Mapping

1. Create a Command mapping set to **Panic**.
2. Verify it works (press key → notes stop).
3. Change the **Command** dropdown to **Sustain**.
4. Verify sustain works (hold key, play note, release note → note sustains).

**Expected:** Changing the command updates the behavior correctly.

**If broken:** Old behavior persists, or wrong behavior after change.

---

## Summary: What to Report Back

For each command type (Panic, Transpose, Layer, Sustain):

1. Does selecting it from the dropdown and pressing the key trigger the correct action?
2. Does changing from one command to another update the behavior correctly?

Then we can decide:

- **Feature works** → Update tests to match current behavior (e.g. option IDs, schema).
- **Feature broken** → Fix the implementation.
