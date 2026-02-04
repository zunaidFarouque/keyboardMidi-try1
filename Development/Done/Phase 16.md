# 🤖 Cursor Prompt: Phase 16 - Dynamic Scale System

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 15.2 Complete. `ScaleUtilities` uses a hardcoded `enum ScaleType`.
*   **Phase Goal:** Replace the hardcoded enum with a dynamic `ScaleLibrary` that supports Factory and User-defined scales.

**Strict Constraints:**
1.  **Architecture:** `ScaleLibrary` is the Source of Truth. `Zone` only stores a `String` (the scale name).
2.  **Math:** `ScaleUtilities` must correctly handle negative scale degrees (moving down octaves) using proper modulo arithmetic.
3.  **Persistence:** User scales must be saved/loaded (XML/ValueTree).

---

### Step 1: `ScaleLibrary` (`Source/ScaleLibrary.h/cpp`)
Manage the collection of scales.

**Requirements:**
1.  **Struct:** `struct Scale { juce::String name; std::vector<int> intervals; bool isFactory; };`
2.  **Class:** `ScaleLibrary` inherits `juce::ChangeBroadcaster`, `juce::ValueTree::Listener`.
3.  **Storage:** `juce::ValueTree rootNode { "ScaleLibrary" };`
    *   *Structure:* `<Scale name="Major" intervals="0,2,4,5,7,9,11" factory="1"/>`
4.  **Methods:**
    *   `void loadDefaults();` (Populate Major, Minor, Pentatonics, Blues, Dorian, Mixolydian, etc.).
    *   `void createScale(String name, std::vector<int> intervals);`
    *   `void deleteScale(String name);` (Prevent deletion if `isFactory`).
    *   `std::vector<int> getIntervals(String name);` (Returns Major intervals if name not found).
    *   `StringArray getScaleNames();`
    *   `void saveToXml(File file);` / `void loadFromXml(File file);`

### Step 2: Refactor `ScaleUtilities`
Remove the Enum and switch statement. Make it generic.

**Update `calculateMidiNote`:**
*   **Signature:** `static int calculateMidiNote(int rootNote, const std::vector<int>& intervals, int degreeIndex);`
*   **Logic:**
    *   `int size = intervals.size();`
    *   `int octaves = std::floor((float)degreeIndex / size);`
    *   `int noteIndex = degreeIndex % size;`
    *   **Negative Fix:** `if (noteIndex < 0) { noteIndex += size; }` (Ensures index is always 0..size-1).
    *   `return rootNote + (octaves * 12) + intervals[noteIndex];`

### Step 3: Update `Zone` & `ZoneManager`
The Zone object no longer knows *what* the scale is, only its name.

1.  **`Zone.h`:** Change `ScaleType scale;` to `juce::String scaleName;`.
    *   Update `processKey` signature: `processKey(..., const std::vector<int>& intervals, ...);`
2.  **`ZoneManager.h`:**
    *   Add `ScaleLibrary& scaleLibrary;` member (pass in constructor).
    *   In `handleInput`: Look up `std::vector<int> intervals = scaleLibrary.getIntervals(zone->scaleName);`
    *   Pass `intervals` to `zone->processKey`.

### Step 4: `ScaleEditorComponent` (`Source/ScaleEditorComponent.h/cpp`)
A UI to define custom intervals.

**Requirements:**
1.  **Layout:**
    *   Left: `ListBox` of existing scales.
    *   Right: Name Editor + 12 Toggle Buttons (labeled "Root", "m2", "M2", ... "M7").
    *   Bottom: "Save Scale", "Delete" buttons.
2.  **Interaction:**
    *   "Root" toggle is disabled and always ON.
    *   Clicking "Save" iterates the toggles (0 to 11), builds the `vector<int>`, and calls `scaleLibrary.createScale`.

### Step 5: Update `ZonePropertiesPanel`
1.  **Dependency:** Pass `ScaleLibrary&`.
2.  **Scale Combo:** Populate from `scaleLibrary.getScaleNames()`.
3.  **Edit Button:** Add a button next to the combo. `onClick` opens `ScaleEditorComponent` (use `juce::DialogWindow::LaunchOptions`).
4.  **Listener:** Listen to `ScaleLibrary`. When changed, refresh the ComboBox.

### Step 6: Integration & CMake
1.  Update `MainComponent` to hold `ScaleLibrary` and pass it to `ZoneManager` and `StartupManager`.
2.  Update `StartupManager` to save/load the `ScaleLibrary` XML to/from AppData.
3.  Update `CMakeLists.txt` to add `Source/ScaleLibrary.cpp` and `Source/ScaleEditorComponent.cpp`.

---

**Generate code for: `ScaleLibrary`, `ScaleUtilities` (refactor), `Zone` (refactor), `ZoneManager` (refactor), `ScaleEditorComponent`, `ZonePropertiesPanel` (update), and `MainComponent` (wiring).**