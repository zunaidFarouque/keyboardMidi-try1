# 🤖 Cursor Prompt: Phase 14 - Advanced Zone Logic (Grid Layout & Chip UI)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 13 Complete. Zones logic is strictly "Linear" (based on the order keys were added).
*   **Phase Goal:** Implement "Grid/Guitar Mode" for zones (calculating pitch based on X/Y row/col) and a "Chip List" UI to visualize/remove individual keys.

**Strict Constraints:**
1.  **Dependency:** `Zone.cpp` must utilize `KeyboardLayoutUtils::getLayout()` for grid calculations.
2.  **Thread Safety:** `Zone::processKey` runs on the audio thread. Ensure `KeyboardLayoutUtils` data is static/const and safe to read.
3.  **UI:** `KeyChipList` must use `juce::FlexBox` for automatic wrapping.

---

### Step 1: Update `Zone` (`Source/Zone.h/cpp`)
Enhance the logic model.

**Requirements:**
1.  **Enum:** `enum class LayoutStrategy { Linear, Grid };`
2.  **Members:**
    *   `LayoutStrategy layoutStrategy = LayoutStrategy::Linear;`
    *   `int gridInterval = 5;` (Default to 4ths/5 semitones).
3.  **Method:** `void removeKey(int keyCode);` (Removes from `inputKeyCodes`).
4.  **Update `processKey` Logic:**
    *   **If Linear:** (Existing logic) Find index in `inputKeyCodes`. `Degree = index`.
    *   **If Grid:**
        *   Look up `KeyGeometry` for the incoming `keyCode` using `KeyboardLayoutUtils`.
        *   Also look up `KeyGeometry` for the **First Key** in `inputKeyCodes` (This acts as the "Anchor" or "Zero Point").
        *   `deltaCol = currentKey.col - anchorKey.col`.
        *   `deltaRow = currentKey.row - anchorKey.row`.
        *   `Degree = deltaCol + (deltaRow * gridInterval)`.
        *   *Note:* This allows relative calculations. If Q is anchor, and A is pressed (Row+1), Degree increases by `gridInterval`.

### Step 2: `KeyChipList` (`Source/KeyChipList.h/cpp`)
A UI component to manage the bag of keys.

**Requirements:**
1.  **Inheritance:** `juce::Component`, `juce::ChangeListener` (optional, or just updated manually).
2.  **Inner Class `Chip`:**
    *   A small `Component` with rounded corners.
    *   Displays Key Name (via `KeyNameUtilities`).
    *   Has a small 'X' button area.
    *   Lambda `onRemove` callback.
3.  **Main Class:**
    *   `std::vector<std::unique_ptr<Chip>> chips;`
    *   `std::function<void(int keyCode)> onKeyRemoved;`
    *   `void setKeys(const std::vector<int>& keyCodes);` -> Rebuilds chips.
    *   `resized()` -> Use `juce::FlexBox` with `FlexWrap::wrap` to layout chips.

### Step 3: Update `ZonePropertiesPanel`
Connect the new features.

**Requirements:**
1.  **UI Additions:**
    *   `juce::ComboBox strategySelector;` ("Linear", "Grid").
    *   `juce::Slider gridIntervalSlider;` (Range -12 to 12).
    *   `KeyChipList chipList;` (Replaces the old text label).
2.  **Logic:**
    *   **Selection:** In `setZone`, populate the new controls and call `chipList.setKeys(zone->inputKeyCodes)`.
    *   **Changes:** When Strategy/Slider changes, update `Zone` members.
    *   **Removal:** When `chipList` triggers removal, call `zone->removeKey(...)` and refresh the list.
    *   **Capture:** When "Assign Keys" finishes (or updates), refresh `chipList`.

### Step 4: Update `CMakeLists.txt`
Add `Source/KeyChipList.cpp`.

---

**Generate code for: Updated `Zone.h/cpp`, `KeyChipList.h/cpp`, Updated `ZonePropertiesPanel.cpp`, and the `CMakeLists.txt` snippet.**