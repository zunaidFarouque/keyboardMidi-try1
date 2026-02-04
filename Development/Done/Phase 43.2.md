The crash in `StartupManager::initApp` (specifically when modifying the ValueTree) happens because **`StartupManager` is manipulating the ValueTree directly without setting the `isLoading` flag**.

This causes a storm of Listener callbacks (`InputProcessor`, `Visualizer`) for every single node addition/removal during the "Factory Default" creation or sanitization process. Since the app is still initializing, this leads to race conditions or performance overload, resulting in heap corruption.

**The Fix:**
We need to allow `StartupManager` to tell `PresetManager`: *"I am about to do a bulk update. Please silence all listeners until I am done."*

Here is the plan for **Phase 43.2**.

### 1. Update `PresetManager` (`Source/PresetManager.h/cpp`)
Expose the "Loading" state control.
*   **Method:** `void beginTransaction();` (Sets `isLoading = true`).
*   **Method:** `void endTransaction();` (Sets `isLoading = false` and sends a change message to trigger a single rebuild).

### 2. Update `StartupManager` (`Source/StartupManager.cpp`)
Wrap the logic in `initApp` and `createFactoryDefault` with these transactions.

***

# 🤖 Cursor Prompt: Phase 43.2 - Safe Startup (Transaction Logic)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** The app crashes in `StartupManager::initApp` with Heap Corruption (`_CrtIsValidHeapPointer`).
*   **Diagnosis:** `StartupManager` modifies the `PresetManager`'s ValueTree (e.g., `createFactoryDefault` or sanitization) **without** setting `isLoading`. This triggers hundreds of synchronous callbacks to `InputProcessor` and `Visualizer`, causing memory corruption during the startup sequence.
*   **Phase Goal:** Expose the `isLoading` flag via "Transaction" methods and wrap the startup logic in them.

**Strict Constraints:**
1.  **PresetManager:** Add `beginTransaction()` / `endTransaction()` methods to control `isLoading`.
2.  **StartupManager:** Wrap `initApp` logic and `createFactoryDefault` logic in these transactions.
3.  **InputProcessor:** Ensure `rebuildMapFromTree` is NOT called if `isLoading` is true (double check `changeListenerCallback` logic).

---

### Step 1: Update `Source/PresetManager.h` & `.cpp`

**Add Public Methods:**

```cpp
// Source/PresetManager.cpp

void PresetManager::beginTransaction()
{
    isLoading = true;
}

void PresetManager::endTransaction()
{
    isLoading = false;
    // Notify listeners that the bulk update is done
    sendChangeMessage(); 
}
```

### Step 2: Update `Source/StartupManager.cpp`

**Refactor `initApp`:**

```cpp
void StartupManager::initApp()
{
    // 1. Load Settings
    juce::File settingsFile = appDataFolder.getChildFile("MIDIQySettings.xml");
    if (settingsFile.exists())
    {
        settingsManager.loadFromXml(settingsFile);
    }

    // 2. SILENCE LISTENERS
    presetManager.beginTransaction();

    bool loadSuccess = false;
    if (autoloadFile.exists())
    {
        // loadFromFile internaly handles isLoading, but wrapping it is safe
        // logic: loadFromFile sets isLoading=true, does work, sets false, sends msg.
        // If we wrap it, we might nest flags. 
        // Better: Let loadFromFile handle itself, but wrap the CHECKS/Sanitization.
        
        // Actually, PresetManager::loadFromFile is self-contained.
        // The issue is likely the logic *after* load or the *fallback*.
        
        presetManager.loadFromFile(autoloadFile); 
        
        // Note: loadFromFile turns isLoading OFF at the end.
        // We should turn it back ON if we plan to modify things further.
        presetManager.beginTransaction(); 

        // Basic Integrity Check
        if (presetManager.getLayersNode().getNumChildren() > 0)
        {
            loadSuccess = true;
            // Load Zones here if needed
             zoneManager.restoreFromValueTree(
                 presetManager.getRootNode().getChildWithName("ZoneManagerData") // Example
             );
        }
    }

    // 3. Fallback (Factory Default)
    if (!loadSuccess)
    {
        DBG("StartupManager: Autoload missing or corrupt. Creating Defaults.");
        // Ensure transaction is open for this
        if (!presetManager.getIsLoading()) presetManager.beginTransaction();
        
        createFactoryDefault();
    }

    // 4. RESUME LISTENERS
    presetManager.endTransaction();
}
```

**Refactor `createFactoryDefault`:**

```cpp
void StartupManager::createFactoryDefault()
{
    // Note: We assume beginTransaction() is called by the caller (initApp).
    // But for safety, we can check.
    bool wasLoading = presetManager.getIsLoading();
    if (!wasLoading) presetManager.beginTransaction();

    // 1. Reset
    auto& root = presetManager.getRootNode();
    root.removeAllChildren(nullptr);
    
    // 2. Ensure Structure
    presetManager.ensureStaticLayers();
    
    // 3. Create Default Mapping (Example: Layer 0)
    auto baseLayer = presetManager.getLayerNode(0);
    // ... add mappings ...

    // 4. Reset Zones
    // zoneManager.createDefault();

    if (!wasLoading) presetManager.endTransaction();
}
```

### Step 3: Verify `InputProcessor.cpp`
Ensure `changeListenerCallback` respects the flag?
*   Actually, `PresetManager::endTransaction` calls `sendChangeMessage()`.
*   `InputProcessor::changeListenerCallback` receives this.
*   **Crucial Check:** Inside `changeListenerCallback`, `InputProcessor` should **NOT** check `isLoading` (because it's false now). It *should* rebuild.
*   **However**, inside `valueTreeChildAdded` (which fires during the transaction), it **MUST** check `isLoading` and return early. (This should already be in place from Phase 41.1).

---

**Generate code for: Updated `PresetManager.h/cpp` and Updated `StartupManager.cpp`.**