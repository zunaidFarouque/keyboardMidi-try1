# 🤖 Cursor Prompt: Phase 18.11 - Bass Notes & Roman Numerals

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.10 Complete. Voicing logic is solid.
*   **Phase Goal:**
    1.  **Bass Option:** Allow adding a specific Bass Note (Root shifted down by N octaves) to any chord zone.
    2.  **Roman Numerals:** Allow the Visualizer to show Scale Degrees (I, ii, V7) instead of Note Names (C, D, G).

**Strict Constraints:**
1.  **Compiler Strategy:** Both the Bass Note and the Roman Numeral String must be calculated in `Zone::rebuildCache`. Do not calculate them in the audio/paint loop.
2.  **Caching:** Add a new `keyToLabelCache` map to `Zone` to store the display text.
3.  **Bass Logic:** The Bass Note is *always* the Root of the chord, shifted down. It is NOT a ghost note (standard velocity).

---

### Step 1: Update `ScaleUtilities` (`Source/ScaleUtilities.h/cpp`)
We need logic to generate Roman Numerals based on the intervals.

**Method:** `static juce::String getRomanNumeral(int degree, const std::vector<int>& intervals);`

**Logic:**
1.  **Identify Intervals:**
    *   `root` = `intervals[degree]`.
    *   `third` = Find interval at `(degree + 2) % size`. Handle octave wrap.
    *   `fifth` = Find interval at `(degree + 4) % size`.
2.  **Determine Quality:**
    *   `diffThird` = `third - root` (handle wrap).
    *   `diffFifth` = `fifth - root` (handle wrap).
    *   **Major (M3):** `diffThird == 4`. Use Uppercase (e.g., "I").
    *   **Minor (m3):** `diffThird == 3`. Use Lowercase (e.g., "i").
    *   **Diminished:** `diffThird == 3` AND `diffFifth == 6`. Append "°" (e.g., "vii°").
    *   **Augmented:** `diffThird == 4` AND `diffFifth == 8`. Append "+" (e.g., "III+").
3.  **Return:** The Roman string (I, ii, iii, IV, V, vi, vii°).

### Step 2: Update `Zone` (`Source/Zone.h/cpp`)
Add features to the data model and compiler.

**1. Header:**
*   Members: `bool addBassNode = false;`, `int bassOctaveOffset = -1;`, `bool showRomanNumerals = false;`.
*   Cache: `std::unordered_map<int, juce::String> keyToLabelCache;`.
*   Methods: `toValueTree`, `fromValueTree`.

**2. `rebuildCache` (The Compiler):**
*   **Bass Logic:**
    *   If `addBassNode` is true:
    *   Calculate `bassPitch = baseNote + (bassOctaveOffset * 12)`.
    *   Add to `ChordNote` vector as a normal note (`isGhost = false`).
    *   Ensure `std::sort` still runs at the end so the bass is the first note played.
*   **Label Logic:**
    *   If `showRomanNumerals` is true:
        *   Call `ScaleUtilities::getRomanNumeral(degree, intervals)`.
        *   Store in `keyToLabelCache`.
    *   Else:
        *   Calculate Note Name (e.g. "C4").
        *   Store in `keyToLabelCache`.

**3. `getKeyLabel(int keyCode)`:**
*   Simple lookup: `return keyToLabelCache[keyCode]`.

### Step 3: Update `ZonePropertiesPanel`
Add the UI controls.

**UI Additions:**
1.  **Bass Section:**
    *   `juce::ToggleButton bassToggle` ("Add Bass").
    *   `juce::Slider bassOctaveSlider` ("Bass Octave", Range -3 to -1).
2.  **Display Section:**
    *   `juce::ComboBox displayModeSelector` ("Note Name", "Roman Numeral").
    *   *Note:* Map this combo to the `showRomanNumerals` boolean.

### Step 4: Update `VisualizerComponent`
Read from the new cache.

**Refactor `paint`:**
*   Instead of calling `MidiNoteUtilities::getMidiNoteName` manually...
*   Call `zone->getKeyLabel(keyCode)`.
*   This ensures the Visualizer respects the Zone's display setting ($O(1)$).

---

**Generate code for: `ScaleUtilities` (New Logic), `Zone` (Update), `ZonePropertiesPanel` (Update), and `VisualizerComponent` (Update).**