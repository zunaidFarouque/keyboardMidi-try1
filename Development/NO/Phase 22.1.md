# 🤖 Cursor Prompt: Phase 22.1 - Modular UI Architecture & Header

**Role:** Expert C++ Audio Developer (JUCE UI Specialist).

**Context:**
*   **Current State:** `ZonePropertiesPanel` is a monolithic list of controls. It is hard to maintain.
*   **Phase Goal:** Refactor the panel into a **Section-Based Architecture** and migrate the "Header" (Basic Info) and "Input" controls.

**Strict Constraints:**
1.  **Architecture:** Do NOT use separate `.h/.cpp` files for sections yet (to keep compilation simple). Define `struct` groups inside `ZonePropertiesPanel.h`.
2.  **Layout:** Implement a `layoutSections()` method that iterates through the structs. If `section.isVisible`, place it and increment Y.
3.  **Preservation:** Do not delete the existing logic for Chords/Scale yet (keep them as loose variables at the bottom) so the app still compiles. We will move them in 22.2/22.3.

---

### Step 1: Define Structs (`Source/ZonePropertiesPanel.h`)

Create a `struct ZoneSection` (Base) or just grouped members.

**1. `struct HeaderSection`:**
*   `juce::Label title { "Zone Settings" };`
*   `juce::Label nameLabel, aliasLabel;`
*   `juce::TextEditor nameEditor;`
*   `juce::ComboBox aliasSelector;`
*   `juce::ToggleButton lockTranspose { "Lock Transpose" };`
*   `void setVisible(bool shouldShow);`

**2. `struct InputSection`:**
*   `juce::Label title { "Input & Layout" };`
*   `juce::ToggleButton assignKeys, removeKeys;`
*   `juce::Label keyCountLabel;`
*   `KeyChipList chipList;`
*   `juce::ComboBox strategySelector;`
*   `juce::Slider gridIntervalSlider;`

### Step 2: Implement Layout Engine (`Source/ZonePropertiesPanel.cpp`)

**1. `updateVisibility()`:**
*   Logic to show/hide specific controls based on state (e.g., Hide Grid Interval if Linear).
*   Call `resized()` at the end.

**2. `resized()`:**
*   `int y = 10;`
*   **Header Section:** Set bounds for name, alias, lock. `y += height`.
*   **Input Section:** Set bounds for keys, chips, strategy. `y += height`.
*   *(Temporary)*: Place the remaining old controls below `y`.
*   Store `totalHeight`.

### Step 3: Migration
*   Move the setup logic for Name, Alias, Assign Keys, and Layout Strategy into these new structs.
*   Ensure data binding (Listeners) still works with `currentZone`.

---

**Generate code for: Updated `ZonePropertiesPanel.h` and `ZonePropertiesPanel.cpp`.**