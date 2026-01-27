# 🤖 Cursor Prompt: Phase 31 - Shutdown Stability (Destruction Order)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** The app works, but crashes with `_CrtIsValidHeapPointer` when closing.
*   **Diagnosis:** This is a "Use After Free" during destruction. High probability that `InputProcessor` or UI components are trying to remove listeners from Managers (`PresetManager`, `ZoneManager`) that have already been destroyed because of incorrect declaration order in `MainComponent.h`.

**Phase Goal:** Reorganize `MainComponent.h` members to ensure a safe dependency chain during destruction and implement explicit cleanup.

**Strict Constraints:**
1.  **Declaration Order:** Re-order members in `MainComponent.h` so that Managers are at the top and Consumers/UI are at the bottom.
2.  **Explicit Shutdown:** In `MainComponent::~MainComponent()`, manually remove listeners and stop timers **before** allowing the default member destructors to run.

---

### Step 1: Refactor `MainComponent.h`
Re-order the private members to this exact sequence:

```cpp
private:
    // 1. Core Hardware & Config (Must die LAST)
    SettingsManager settingsManager;
    DeviceManager deviceManager;
    MidiEngine midiEngine;
    ScaleLibrary scaleLibrary;

    // 2. Logic Managers (Depend on Core)
    VoiceManager voiceManager; // Depends on MidiEngine
    ZoneManager zoneManager;   // Depends on ScaleLibrary
    PresetManager presetManager;

    // 3. Processors (Depend on Managers)
    InputProcessor inputProcessor; // Listens to Preset/Device/Zone

    // 4. Persistence Helper
    StartupManager startupManager; 

    // 5. UI Components (Depend on everything)
    VisualizerComponent visualizer; // Listens to RawInput/Voice/Zone
    MappingEditorComponent mappingEditor;
    LogComponent logComponent;
    
    // 6. Containers/Widgets
    juce::TabbedComponent leftPanelTabs; // Holds Mapping/Zone/Settings
    DetachableContainer visualizerContainer;
    DetachableContainer inspectorContainer;
    juce::Viewport inspectorViewport;
    MappingInspector inspector; // Inspector might need to be higher if container refs it? 
                               // Actually Inspector is just a component. 
                               // It refs PresetManager. Safe here.
    
    juce::TextButton saveButton;
    juce::TextButton loadButton;
    juce::TextButton deviceSetupButton;
    juce::TextButton clearButton;
    juce::ComboBox midiSelector;
    juce::ToggleButton performanceModeButton;

    // 7. Input Source (Raw Input)
    // Destroying this unhooks the Window. Do it early or late?
    // It calls listeners. Listeners must be alive. 
    // So Listeners (InputProcessor) must die BEFORE RawInputManager logic stops?
    // Actually, RawInputManager is a unique_ptr.
    std::unique_ptr<RawInputManager> rawInputManager;
    
    // 8. Layout
    juce::StretchableLayoutManager horizontalLayout;
    juce::StretchableLayoutResizerBar resizerBar;
    
    // 9. Windows
    std::unique_ptr<MiniStatusWindow> miniWindow;
    
    // ... helpers ...
```

### Step 2: Update `MainComponent.cpp`
Clean up safely.

**Refactor Destructor:**
```cpp
MainComponent::~MainComponent()
{
    // 1. Stop Events immediately
    stopTimer(); // Stop logging timer
    
    // 2. Stop Input
    if (rawInputManager)
    {
        rawInputManager->shutdown(); // Unsubclasses window, stops sending callbacks
    }

    // 3. Force Save
    startupManager.saveImmediate();

    // 4. Close Child Windows
    if (miniWindow)
    {
        miniWindow->setVisible(false);
        miniWindow = nullptr;
    }

    // 5. Implicit Destruction follows...
    // InputProcessor will die. It calls presetManager.removeListener().
    // PresetManager is still alive (because it's declared above). Safe.
}
```

### Step 3: Update `SettingsPanel` (Leak Check)
If `SettingsPanel` is inside `leftPanelTabs`, verify it doesn't hold dangling pointers. It should be fine as `TabbedComponent` owns it and dies in step 6.

---

**Generate code for: Updated `MainComponent.h` (Reordered) and `MainComponent.cpp` (Destructor Logic).**