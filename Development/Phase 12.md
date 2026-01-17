# 🤖 Cursor Prompt: Phase 12 - Real-Time Visualizer (HUD)

**Role:** Expert C++ Audio Developer (JUCE Framework & Graphics).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 11 Complete. We have a robust `ZoneManager`, but no way to "see" the virtual layout.
*   **Phase Goal:** Create a graphical Keyboard Visualizer that shows active zones, physical key presses, and dynamic note labels (e.g., showing "C#4" on the 'Q' key).

**Strict Constraints:**
1.  **Architecture:** `KeyboardLayoutUtils` is a standalone helper.
2.  **Performance:** `paint()` must be efficient.
3.  **CMake:** Provide the snippet to add `KeyboardLayoutUtils.cpp` and `VisualizerComponent.cpp`.

---

### Step 1: `KeyboardLayoutUtils` (`Source/KeyboardLayoutUtils.h/cpp`)
Define the physical geometry of a keyboard.

**Requirements:**
1.  **Struct:** `struct KeyGeometry { int row; float col; float width; String label; };`
2.  **Data:** Create a `static const std::map<int, KeyGeometry>& getLayout();`
    *   **Populate this map** with the standard ANSI layout:
    *   **Row 0:** `1` through `=` (Virtual Codes `0x31`..`0xBB`).
    *   **Row 1:** `Q` through `]` (Codes `0x51`..`0xDD`). Offset 1.5.
    *   **Row 2:** `A` through `'` (Codes `0x41`..`0xDE`). Offset 1.8.
    *   **Row 3:** `Z` through `/` (Codes `0x5A`..`0xBF`). Offset 2.3.
    *   **Row 4:** Spacebar.
3.  **Helper:** `static juce::Rectangle<float> getKeyBounds(int keyCode, float keySize, float padding);`
    *   Calculates screen rect based on row/col/width.

### Step 2: Update `ZoneManager` (Simulation)
We need to calculate results without playing audio.

**Requirements:**
1.  **Method:** `std::optional<MidiAction> simulateInput(int keyCode, uintptr_t aliasHash);`
    *   **Logic:** Similar to `handleInput`, but takes explicit arguments instead of `InputID`.
    *   Iterate zones.
    *   Call `zone->processKey(...)`.
    *   Return result if found.

### Step 3: `VisualizerComponent` (`Source/VisualizerComponent.h/cpp`)
The main display.

**Inheritance:**
*   `juce::Component`
*   `RawInputManager::Listener` (To highlight pressed keys).
*   `juce::ChangeListener` (To listen to `ZoneManager` for transpose updates).

**Members:**
*   `std::set<int> activeKeys;` (Keys currently held down).
*   References to `ZoneManager`, `DeviceManager`.

**Drawing Logic (`paint`):**
1.  **Iterate** through all keys in `KeyboardLayoutUtils`.
2.  **Layer 1 (Zone Backgrounds):**
    *   Ask `ZoneManager` if this key (for the "Master" alias or active aliases) maps to anything.
    *   If yes, fill the background with a generic "Zone Color" (e.g., `Colours::blue.withAlpha(0.2f)`).
3.  **Layer 2 (Keys):**
    *   Draw the rounded rectangle outline.
    *   If `activeKeys.contains(keyCode)`, fill with **Bright Yellow**.
    *   Else, fill with Dark Grey.
4.  **Layer 3 (Labels):**
    *   **Top Left:** Draw physical key name (e.g., 'Q').
    *   **Center:** Call `zoneManager->simulateInput`.
        *   If result is a **Note**, use `MidiNoteUtilities` to draw "C#4".
        *   If result is a **Root Note** of the zone, draw it in **Bold/Gold**.

**Input Handling:**
*   Implement `handleRawKeyEvent`. Update `activeKeys` and call `repaint()`.
*   Implement `changeListenerCallback`. Call `repaint()` (updates labels when transpose changes).

### Step 4: Integration
*   Add `VisualizerComponent` to `MainComponent`. Place it at the very top (height ~150px).
*   Pass required references.

---

**Generate code for: `KeyboardLayoutUtils`, updated `ZoneManager`, `VisualizerComponent`, updated `MainComponent`, and `CMakeLists.txt`.**