# 🤖 Cursor Prompt: Phase 33 - Fix Shutdown Crashes (Double Delete & Order)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** The app works but crashes on exit with Heap Corruption.
*   **Diagnosis 1:** `VisualizerComponent` is destroyed *after* `RawInputManager` because of incorrect declaration order, causing a Use-After-Free in the destructor.
*   **Diagnosis 2:** `TabbedComponent` is deleting member components (`MappingEditor`, etc.) because `addTab` defaults to taking ownership, causing a Double Delete when the member destructor runs.

**Phase Goal:** Fix the declaration order and the Tab ownership logic.

**Strict Constraints:**
1.  **Order:** `RawInputManager` MUST be declared **above** any component that listens to it (`Visualizer`, `MappingEditor`, `MainComponent`).
2.  **Tabs:** When calling `leftPanelTabs.addTab(...)`, the last argument `deleteComponentWhenNotNeeded` MUST be **`false`** for any component that is a member variable of `MainComponent`.

---

### Step 1: Update `Source/MainComponent.h`
Re-order the members logic.

**New Order:**
```cpp
private:
    // 1. Core Engines & Inputs (Must die LAST)
    SettingsManager settingsManager;
    DeviceManager deviceManager;
    MidiEngine midiEngine;
    ScaleLibrary scaleLibrary;
    std::unique_ptr<RawInputManager> rawInputManager; // MOVED UP (Was #7)

    // 2. Logic Managers
    VoiceManager voiceManager;
    ZoneManager zoneManager;
    PresetManager presetManager;

    // 3. Processors
    InputProcessor inputProcessor;

    // 4. Persistence
    StartupManager startupManager;

    // 5. UI Components (Depend on Inputs/Managers)
    VisualizerComponent visualizer; 
    MappingEditorComponent mappingEditor;
    ZoneEditorComponent zoneEditor; // Ensure this is a member if used in tabs
    SettingsPanel settingsPanel;    // Ensure this is a member if used in tabs
    LogComponent logComponent;

    // 6. Containers
    juce::TabbedComponent leftPanelTabs;
    // ... rest of UI ...
```

### Step 2: Update `Source/MainComponent.cpp`
Fix the Tab adding logic.

**Refactor Constructor:**
Find where `leftPanelTabs.addTab` is called. Change it to:

```cpp
// 4th argument is 'deleteComponentWhenNotNeeded'. Set to FALSE for members.
leftPanelTabs.addTab("Mappings", juce::Colours::darkgrey, &mappingEditor, false); 
leftPanelTabs.addTab("Zones", juce::Colours::darkgrey, &zoneEditor, false);
leftPanelTabs.addTab("Settings", juce::Colours::darkgrey, &settingsPanel, false);
```

### Step 3: Verify Destructor
Ensure `rawInputManager->shutdown()` is still called explicitly, but now the pointer is safe until the very end.

---

**Generate code for: Updated `MainComponent.h` (Reordered) and Updated `MainComponent.cpp` (Tab Logic).**