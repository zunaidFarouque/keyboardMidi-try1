# 🤖 Cursor Prompt: Phase 34 - Shutdown Crash Final Fix (UI Order)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Problem:** Persistent `_CrtIsValidHeapPointer` crash on exit.
*   **Diagnosis:** Incorrect member declaration order in Header files. Containers (`Viewport`, `TabbedComponent`) are being destroyed *after* their content, or vice-versa in a way that causes dangling pointer access during destruction.
*   **Goal:** Re-order member variables in all UI classes to strictly follow C++ destruction rules (Containers Last/Bottom, Content First/Top).

**Strict Constraints:**
1.  **Rule:** `ContentComponent` must be declared **ABOVE** `WrapperComponent` / `Viewport` / `TabbedComponent`.
2.  **Files to Check:** `MainComponent.h`, `ZoneEditorComponent.h`, `MappingEditorComponent.h`.

---

### Step 1: Fix `Source/ZoneEditorComponent.h`
Ensure `propertiesPanel` stays alive longer than the Viewport.

**Correct Order:**
```cpp
private:
    // 1. Data/Managers
    ZoneManager& zoneManager;
    DeviceManager& deviceManager;
    RawInputManager& rawInputManager;
    ScaleLibrary& scaleLibrary;

    // 2. Content Components (Must live longer than containers)
    ZoneListPanel listPanel;
    ZonePropertiesPanel propertiesPanel; // CONTENT
    GlobalPerformancePanel globalPerfPanel;

    // 3. Containers (Must die first)
    juce::Viewport propertiesViewport; // CONTAINER
    // ...
```

### Step 2: Fix `Source/MainComponent.h`
Ensure Tabs and Viewports die before their content.

**Correct Order:**
```cpp
private:
    // 1. Managers (Top)
    // ... (Existing correct order for managers) ...

    // 2. Content Components
    VisualizerComponent visualizer;
    MappingEditorComponent mappingEditor; // CONTENT for Tab
    ZoneEditorComponent zoneEditor;       // CONTENT for Tab
    SettingsPanel settingsPanel;          // CONTENT for Tab
    MappingInspector inspector;           // CONTENT for Viewport
    LogComponent logComponent;

    // 3. Containers / Wrappers (Bottom - Die First)
    juce::TabbedComponent leftPanelTabs; // WRAPPER
    juce::Viewport inspectorViewport;    // WRAPPER
    
    // Detachables
    DetachableContainer visualizerContainer;
    DetachableContainer inspectorContainer;
    DetachableContainer editorContainer;

    // 4. Layout & Windows
    // ...
```

### Step 3: Fix `Source/MainComponent.cpp` Destructor
Remove the manual `shutdown()` call for `rawInputManager`.
*   *Reason:* We made `~RawInputManager` robust in Phase 32. Calling `shutdown()` manually might be doing double-work or leaving the static pointer in a weird state if not implemented perfectly. Let RAII handle it.

**Update `~MainComponent`:**
```cpp
MainComponent::~MainComponent()
{
    stopTimer();
    
    // Remove this line if it exists:
    // if (rawInputManager) rawInputManager->shutdown(); 
    
    // Just force the save
    startupManager.saveImmediate();
    
    // Close windows
    if (miniWindow)
    {
        miniWindow->setVisible(false);
        miniWindow = nullptr;
    }
}
```

---

**Generate code for: Updated `ZoneEditorComponent.h`, Updated `MainComponent.h`, and Updated `MainComponent.cpp` (Destructor).**