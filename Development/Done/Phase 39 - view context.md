Here is the prompt for **Phase 39**.

This phase implements the **View Context** logic. It upgrades the `InputProcessor` to be "Self-Aware" about where a mapping comes from (Local vs. Global) and updates the Visualizer to let you filter the view.

***

# 🤖 Cursor Prompt: Phase 39 - Visualizer View Context & Override Indication

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 38 Complete. The Visualizer always shows the "Master" context (Device 0). It doesn't show what happens on specific devices.
*   **Phase Goal:**
    1.  Add a **View Selector** (ComboBox) to the Visualizer to choose a specific Device Alias.
    2.  Update `InputProcessor` to detect **Inheritance** (Local device using Global mapping) and **Overrides** (Local device blocking Global mapping).
    3.  Update Visualizer to draw a **Distinct Border Color** when a Local mapping overrides a Global one.

**Strict Constraints:**
1.  **Architecture:** Do not break existing playback logic. Create a helper struct `SimulationResult` in `MappingTypes.h` to pass rich data to the Visualizer.
2.  **Visuals:**
    *   **Override (Local shadows Global):** Draw **Orange** Border.
    *   **Inherited (Local uses Global):** Draw Standard Border (maybe slightly dimmer fill, but standard is fine).
    *   **Unique (Local only, no Global):** Standard Border.
3.  **UI:** The View Selector must automatically update when Aliases are renamed/added (Listen to `DeviceManager`).

---

### Step 1: Update `MappingTypes.h`
Create the rich return type.

```cpp
struct SimulationResult {
    std::optional<MidiAction> action;
    juce::String sourceDescription; // For Log
    bool isOverride = false;        // True if Specific masked a Global rule
    bool isInherited = false;       // True if using Global rule fallback
};
```

### Step 2: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Upgrade the simulation logic.

**Refactor `simulateInput`:**
*   **Signature:** `SimulationResult simulateInput(uintptr_t viewDeviceHash, int keyCode);`
*   **Logic:**
    1.  **Check Specific Manual:** `keyMapping.find({viewDeviceHash, key})`.
        *   If found: Check if `{0, key}` *also* exists (Manual) OR if `{0, key}` is covered by a Global Zone. If yes, `isOverride = true`. Return Result.
    2.  **Check Global Manual:** `keyMapping.find({0, key})`.
        *   If found: `isInherited = true`. Return Result.
    3.  **Check Specific Zone:** `zoneManager.handleInput({viewDeviceHash, key})`.
        *   If found: Check if `{0, key}` is covered by Global Zone. If yes, `isOverride = true`. Return Result.
    4.  **Check Global Zone:** `zoneManager.handleInput({0, key})`.
        *   If found: `isInherited = true`. Return Result.
    5.  **Else:** Empty result.

### Step 3: Update `VisualizerComponent.h/cpp`
Add the Selector and Paint Logic.

**1. UI Members:**
*   `juce::ComboBox viewSelector;`
*   `uintptr_t currentViewHash = 0;` (Default Master).

**2. Constructor:**
*   Populate `viewSelector`:
    *   ID 1: "Any / Master" (Hash 0).
    *   Loop `DeviceManager` aliases for others.
*   **Listener:** When `DeviceManager` changes, refresh the list.

**3. `paint` (Layer 3 - Border Update):**
*   Call `inputProcessor.simulateInput(currentViewHash, keyCode)`.
*   **Border Color Logic:**
    *   If `result.isOverride`: **Colours::orange**.
    *   Else: **Colours::grey** (Standard).
*   *(Note: Ensure you still draw the standard Key Body fill based on the resulting Action type).*

**4. `handleRawKeyEvent`:**
*   Ensure we repaint based on the *actual* input, but the visualizer *layout* remains locked to `currentViewHash`.

### Step 4: Update `MainComponent.cpp`
Fix the Logger.
*   Update `logEvent` to use the new `SimulationResult` struct fields (`sourceDescription`) instead of the old `std::pair`.

---

**Generate code for: `MappingTypes.h`, `InputProcessor.h/cpp`, `VisualizerComponent.h/cpp`, and `MainComponent.cpp`.**