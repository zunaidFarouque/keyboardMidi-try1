# Manual Testing: Pitch Pad Config & Relative Mode

Use these steps to verify whether the pitch pad features work correctly in the app, so we can decide if the failing tests reflect broken features or outdated test expectations.

---

## Prerequisites

1. **Run the app** (build and launch MIDIQy).
2. **Assign a touchpad device**: In Settings, ensure at least one device is assigned to the "Touchpad" alias (or the app will use any device when touchpad layouts/mappings exist).
3. **MIDI output**: Either route MIDI to an external synth/DAW, or use a MIDI monitor (e.g. MIDI-OX on Windows) to see pitch bend messages.

---

## 1. Pitch Pad Config Compilation (GridCompilerTest)

**What the tests expect:** When you configure a pitch bend mapping with Output min = -2 and Output max = +2 semitones, the compiled config should have `minStep=-2`, `maxStep=2`.

**How to test:**

### Option A: Using "Bend (semitones)" (default mode)

1. Go to **Touchpad** tab.
2. Add a new touchpad mapping (or select an existing one).
3. Set **Type** = `Expression`.
4. Set **Target** = `Pitch Bend`.
5. Set **Bend (semitones)** = `2` (this is the `data2` slider).
6. In the **Pitch pad** section:
   - **Mode** = `Absolute` (simpler to verify first).
   - **Starting position** = `Center`.

**Expected behavior:** Swiping the touchpad from left to right should send pitch bend from center (0) up to about +2 semitones at the right edge. The visualizer (if shown) should display something like "+2.0 st" at the right edge.

**If it works:** The config path using `data2` (Bend semitones) is fine; tests may need to set `data2` or `pitchPadUseCustomRange` to match current behavior.

---

### Option B: Using "Custom min/max"

1. Same as above, but in the **Pitch pad** section:
2. Enable **Custom min/max** (the `pitchPadUseCustomRange` toggle).
3. Set **Pitch range min (semitones)** = `-2`.
4. Set **Pitch range max (semitones)** = `+2`.
5. **Mode** = `Absolute`, **Starting position** = `Center`.

**Expected behavior:** Same as Option A: left edge ≈ -2 semitones, center ≈ 0, right edge ≈ +2 semitones.

**If it works:** Custom range compilation is fine; tests may need to set `pitchPadUseCustomRange=true` when using `touchpadOutputMin`/`touchpadOutputMax`.

---

### How to verify

- **Visualizer**: The touchpad visualizer (left side when Touchpad tab is active) should show a pitch indicator (e.g. "+2.0 st") when you swipe to the right edge.
- **MIDI monitor**: Pitch bend messages (0xE0) should go from center (8192) to higher values when swiping right.
- **DAW/synth**: If connected, you should hear pitch rise when swiping right.

---

## 2. Relative Mode Pitch Bend (TouchpadPitchPadTest)

**What the tests expect:** In Relative mode, the first touch sets the "anchor" (zero point). Moving away from that point should apply pitch bend up to the configured range (±2).

**How to test:**

1. In the **Touchpad** tab, create or edit a pitch bend mapping.
2. Set **Target** = `Pitch Bend`.
3. Set **Bend (semitones)** = `2` (or use Custom min/max with -2 to +2).
4. In **Pitch pad**:
   - **Mode** = `Relative`.
   - Leave **Starting position** as Center (not used in Relative mode).

**Test sequence:**

1. **Anchor at center, then swipe right:**
   - Touch the center of the touchpad (x ≈ 0.5).
   - Without lifting, swipe to the right edge (x = 1.0).
   - **Expected:** Pitch bend should go from 0 to about +2 semitones.
   - **If broken:** Pitch bend stays at 0.

2. **Anchor at left, then swipe to 0.7:**
   - Touch the left area (x ≈ 0.2).
   - Swipe to x ≈ 0.7 (same distance as center→right in absolute mode).
   - **Expected:** Pitch bend should reach about +2 semitones.
   - **If broken:** Pitch bend stays at 0.

3. **Extrapolation (optional):**
   - In Settings, set **Pitch bend range** to 6 semitones.
   - Use Custom min/max with -2 to +2.
   - Anchor at left (x=0), swipe to right (x=1).
   - **Expected:** Pitch bend can exceed +2 (extrapolation) up to the global range.
   - **If broken:** Pitch bend stays at 0 or doesn’t exceed +2.

---

### How to verify

- **Visualizer**: The "+X.X st" label should change as you move your finger.
- **MIDI monitor**: Pitch bend values should change from 8192 (center) when you move from the anchor.
- **DAW/synth**: Pitch should change relative to where you first touched.

---

## 3. Pitch Pad "Start Left" (TouchpadTab_PitchPadStartLeft_ZeroAtLeftEdge)

**What the test expects:** With **Starting position** = `Left`, touching the left edge should send pitch bend = 0 (zero), not empty/no events.

**How to test:**

1. Create a pitch bend mapping.
2. Set **Mode** = `Absolute`.
3. Set **Starting position** = `Left`.
4. Set **Bend (semitones)** = 2 or Custom min/max -2 to +2.

**Test:** Touch the left edge of the touchpad.

**Expected:** Pitch bend events are sent with value ≈ center (8192), i.e. 0 semitones.
**If broken:** No pitch bend events, or wrong value.

---

## Summary: What to Report Back

After testing, please note:

1. **Pitch pad config (Option A and/or B):** Does pitch bend reach ±2 semitones when configured?
2. **Relative mode:** Does pitch bend change correctly when you move from the anchor?
3. **Start Left:** Do you get pitch bend events when touching the left edge with Start = Left?

Then we can decide:
- **Feature works** → Update tests to match current behavior (e.g. `pitchPadUseCustomRange`, `data2`).
- **Feature broken** → Fix the implementation.
