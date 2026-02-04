Here is the prompt for **Phase 38**.

This connects the configuration (Colors) to the user experience (Visualizer). We need to ensure the Visualizer is smart enough to detect when a Manual Mapping overrides a Zone, and paint it with the correct color (e.g., Red for Command, Blue for Note).

***

# 🤖 Cursor Prompt: Phase 38 - Visualizing Manual Mappings

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 37 Complete. `SettingsManager` stores colors for Note, CC, Command, etc.
*   **Problem:** The Visualizer only shows Zone colors. Manual mappings (which override zones) are invisible until pressed.
*   **Phase Goal:** Update the Visualizer to render an underlay for Manual Mappings using the colors defined in Settings.

**Strict Constraints:**
1.  **Priority:** If a key has a Manual Mapping, draw that color. If not, draw the Zone color.
2.  **Caching:** Since the Visualizer uses an Image Cache (Phase 30.2), we must invalidate the cache (`cacheValid = false`) whenever Mappings change or Settings (Colors) change.
3.  **Lookup:** The Visualizer needs to ask `InputProcessor` if a mapping exists for a specific key (assuming "Master" / Hash 0 context for the visualization).

---

### Step 1: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Add a lookup helper for the UI.

**Method:** `std::optional<ActionType> getMappingType(int keyCode, uintptr_t aliasHash);`
*   **Logic:**
    *   Acquire `ScopedReadLock`.
    *   Check `keyMapping` for `{aliasHash, keyCode}`.
    *   If found, return `it->second.type`.
    *   Else, return `std::nullopt`.

### Step 2: Update `VisualizerComponent.h`
Add dependencies and listeners.

**Updates:**
1.  **Inheritance:** Add `public juce::ValueTree::Listener` (to watch for Mapping changes).
2.  **Members:**
    *   Add `InputProcessor& inputProcessor;`
    *   Add `SettingsManager& settingsManager;`
    *   Add `PresetManager& presetManager;`
3.  **Overrides:** Implement `valueTreeChildAdded`, `valueTreeChildRemoved`, `valueTreePropertyChanged`, `valueTreeParentChanged`. (All should set `cacheValid = false; needsRepaint = true;`).

### Step 3: Update `VisualizerComponent.cpp`
Implement the new Rendering Logic.

**1. Constructor:**
*   Register as `ValueTree::Listener` with `presetManager.getRootNode()`.
*   Register as `ChangeListener` with `settingsManager`.

**2. Destructor:**
*   Unregister from `presetManager`.
*   Unregister from `settingsManager`.

**3. Refactor `refreshCache` (The Paint Loop):**
*   Inside the key loop:
    *   **Check Manual Mapping First:**
        *   Call `inputProcessor.getMappingType(keyCode, 0)`. (Using 0 for Master/Global visual).
        *   If result exists:
            *   Get Color: `settingsManager.getTypeColor(*result)`.
            *   Set `underlayColor`.
    *   **Else (Zone Fallback):**
        *   (Existing logic) Call `zoneManager.getZoneColorForKey(...)`.

### Step 4: Update `MainComponent.cpp`
Pass the new references.

**Constructor:**
*   Update `visualizer` initialization list to include `inputProcessor`, `settingsManager`, `presetManager`.

---

**Generate code for: Updated `InputProcessor`, Updated `VisualizerComponent.h/cpp`, and Updated `MainComponent.cpp`.**