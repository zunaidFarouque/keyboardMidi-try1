# 🤖 Cursor Prompt: Phase 42 - Safe Initialization Architecture

**Role:** Expert C++ Audio Developer (System Architecture).

**Context:**
*   **Current State:** The app crashes instantly on startup with Heap Corruption.
*   **Diagnosis:** "Constructor Event Storm". Managers (`PresetManager`) manipulate data in their constructors, which triggers Listeners (`InputProcessor`) effectively *during* the construction of the object graph. This leads to race conditions or accessing uninitialized memory regions.
*   **Solution:** Move `addListener` logic OUT of constructors. Implement a **Two-Stage Initialization**:
    1.  **Construction:** Allocate memory, setup members. (No events).
    2.  **Initialization:** Connect listeners, load data, fire initial updates.

**Phase Goal:** Update `InputProcessor`, `MappingEditorComponent`, and `MainComponent` to use an explicit `initialize()` pattern.

**Strict Constraints:**
1.  **Constructors:** MUST NOT call `addListener` on external objects.
2.  **New Method:** Add `void initialize();` to components that listen to others.
3.  **MainComponent:** Must call these `initialize()` methods in the correct order at the *end* of its own constructor.

---

### Step 1: Update `InputProcessor`
**1. Header:** Add `void initialize();`
**2. Cpp:**
    *   **Remove** `presetManager.addListener(this)` from Constructor.
    *   **Remove** `rebuildMapFromTree()` from Constructor.
    *   **Implement `initialize()`:**
        ```cpp
        void InputProcessor::initialize()
        {
            presetManager.getRootNode().addListener(this);
            deviceManager.addChangeListener(this);
            zoneManager.addChangeListener(this);
            settingsManager.addChangeListener(this);
            
            // Initial Build
            rebuildMapFromTree();
        }
        ```

### Step 2: Update `MappingEditorComponent`
**1. Header:** Add `void initialize();`
**2. Cpp:**
    *   **Remove** `presetManager.getRootNode().addListener(this)` from Constructor.
    *   **Remove** `table.updateContent()` from Constructor.
    *   **Implement `initialize()`:**
        ```cpp
        void MappingEditorComponent::initialize()
        {
            presetManager.getRootNode().addListener(this);
            table.updateContent();
        }
        ```

### Step 3: Update `VisualizerComponent`
**1. Header:** Add `void initialize();`
**2. Cpp:**
    *   **Remove** listeners from Constructor.
    *   **Implement `initialize()`:**
        ```cpp
        void VisualizerComponent::initialize()
        {
            if (rawInputManager) rawInputManager->addListener(this); // Assuming rawInputManager is passed or set
            zoneManager.addChangeListener(this);
            settingsManager.addChangeListener(this);
            // etc...
        }
        ```

### Step 4: Update `MainComponent.cpp`
Orchestrate the startup.

**Refactor Constructor:**
At the very end of the constructor body:
```cpp
    // --- SAFE INITIALIZATION SEQUENCE ---
    // 1. Link Logic
    inputProcessor.initialize();
    
    // 2. Link UI
    mappingEditor.initialize();
    zoneEditor.initialize(); // If it has one
    visualizer.initialize();
    settingsPanel.initialize(); // If it listens to settingsManager

    // 3. START ENGINE (Load Data)
    // Now that listeners are ready, we load the XML.
    // Events will flow safely because all objects are fully constructed.
    startupManager.initApp(); 
    
    // 4. Start Input
    if (auto* top = getTopLevelComponent())
        if (auto* peer = top->getPeer())
             rawInputManager->initialize(peer->getNativeHandle());
```

---

**Generate code for: Updated `InputProcessor`, `MappingEditorComponent`, `VisualizerComponent`, and `MainComponent`.**