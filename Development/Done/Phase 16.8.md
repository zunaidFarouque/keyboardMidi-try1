# 🤖 Cursor Prompt: Phase 16.8 - True Piano Layout & Performance Compilation

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 16.6 Complete. Zones calculate notes on-the-fly using math.
*   **Goal 1 (Performance):** Refactor `Zone` to **pre-compile** mappings into a cache (`std::unordered_map`). `processKey` should effectively be a simple lookup, doing zero math during performance.
*   **Goal 2 (Feature):** Implement `LayoutStrategy::Piano`. This maps two rows of keys to a piano layout (Bottom Row = White Keys, Top Row = Black Keys).

**Strict Constraints:**
1.  **Architecture:** `Zone.cpp` must implement a `rebuildCache()` method called whenever properties change.
2.  **Piano Logic:**
    *   Ignore the selected Scale (Force Chromatic).
    *   Identify the two rows of keys based on Y-coordinates from `KeyboardLayoutUtils`.
    *   **White Keys:** Map Bottom Row keys to White Keys (C, D, E, F, G, A, B).
    *   **Black Keys:** Map Top Row keys to Black Keys (C#, D#, etc.).
    *   **Strict Mode:** If a Top Row key sits above a "gap" (e.g., between E and F), it produces **No Sound**.

---

### Step 1: Update `Zone` Header (`Source/Zone.h`)
Refactor the class to support caching.

**Changes:**
1.  Add `enum class LayoutStrategy { Linear, Grid, Piano };` (Add Piano).
2.  **New Member:** `std::unordered_map<int, int> keyToNoteCache;` (Key Code -> Relative Note Number).
3.  **New Method:** `void rebuildCache();`
4.  **Update:** `processKey` should no longer take global transpose args. It should take the `InputID`, look up the note in the cache, and *then* apply global transpose. (Or pre-calculate everything if possible, but keeping Global separate allows live transposing without rebuilding the whole cache).

### Step 2: Implement `Zone::rebuildCache()` (`Source/Zone.cpp`)
This is the "Compiler". Implement logic for all 3 strategies here.

**Logic Switch:**
1.  **Linear:** Iterate `inputKeyCodes`. Cache[key] = `ScaleUtilities::calculateMidiNote(..., index)`.
2.  **Grid:** Use existing math logic, but save results to Cache.
3.  **Piano (New Logic):**
    *   **Group Keys:** Use `KeyboardLayoutUtils` to get `KeyGeometry`. Group input keys by `row`.
    *   **Identify Rows:** Sort rows by index. Max Row = White Keys. Max-1 Row = Black Keys.
    *   **Sort Columns:** Sort keys in each row by `col` (x-position).
    *   **Mapping Loop:**
        *   Iterate White Keys (Bottom Row). Keep a running `diatonicIndex` (0, 1, 2...).
        *   For each White Key:
            *   Calculate Note: `ScaleUtilities::calculateMidiNote(root, MajorScale, diatonicIndex)`.
            *   Store in Cache.
            *   **Check Black Key:** Look for a key in the Top Row that is spatially aligned (e.g., `topKey.col` is roughly `whiteKey.col + 0.5`).
            *   **Musical Check:** Does this White Key have a sharp? (C, D, F, G, A).
            *   **If Yes & Top Key Exists:** Map Top Key to `WhiteNote + 1`.
            *   **If No & Top Key Exists:** Ignore Top Key (Strict Mode).
        *   *Result:* A perfect piano layout relative to the Root note.

### Step 3: Update `Zone::processKey`
Make it fast.

**Logic:**
1.  Check `targetAliasHash`.
2.  **Lookup:** `auto it = keyToNoteCache.find(input.keyCode);`
3.  If not found, return `nullopt`.
4.  **Finalize:** Apply `chromaticOffset` + Global Transpose (unless locked). Return `MidiAction`.

### Step 4: Update `ZonePropertiesPanel`
1.  Add "Piano" to the Layout Strategy ComboBox.
2.  **UI Logic:** When "Piano" is selected:
    *   Disable/Hide the "Scale" ComboBox (set it internally to Major or ignore it).
    *   Disable "Grid Interval".
    *   Show a help label: "Requires 2 rows of keys".

### Step 5: Update `VisualizerComponent`
No major changes needed, but ensure it calls `zoneManager->simulateInput` (which calls `processKey`), so it automatically benefits from the new Piano mapping logic.

---

**Generate code for: Updated `Zone.h`, `Zone.cpp` (Heavy logic in rebuildCache), and updated `ZonePropertiesPanel.cpp`.**