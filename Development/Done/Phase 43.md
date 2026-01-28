# 🤖 Cursor Prompt: Phase 43 - Persistence Hardening (Crash Fix)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 42 Complete.
*   **The Bug:** App works on fresh install. After closing and reopening, it crashes or fails to open.
*   **Diagnosis:** `autoload.xml` or `OmniKeySettings.xml` contains invalid data (e.g., `pitchBendRange="0"`). This causes Division By Zero in `InputProcessor` or Array Out of Bounds during the startup load sequence.

**Phase Goal:** Implement aggressive data validation and a "Fail-Safe" loading mechanism.

**Strict Constraints:**
1.  **Math Safety:** In `InputProcessor`, checks for `range == 0` before dividing.
2.  **Settings Safety:** In `SettingsManager::loadFromXml`, clamp all values to valid ranges immediately.
3.  **Startup Safety:** If `StartupManager` detects a crash/failure during load (simulated via basic validation), it must discard the data and load defaults.

---

### Step 1: Update `Source/SettingsManager.cpp`
Validate inputs during load.

**Refactor `loadFromXml`:**
```cpp
void SettingsManager::loadFromXml(juce::File file)
{
    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr)
    {
        auto newTree = juce::ValueTree::fromXml(*xml);
        if (newTree.isValid())
        {
            rootNode.copyPropertiesFrom(newTree, nullptr);
            
            // --- VALIDATION (Sanitize Data) ---
            
            // 1. Pitch Bend Range (Prevent Divide by Zero)
            int pb = rootNode.getProperty("pitchBendRange", 12);
            if (pb < 1) 
            {
                pb = 12; 
                rootNode.setProperty("pitchBendRange", 12, nullptr);
            }
            
            // 2. Studio Mode
            // (Bool doesn't need clamping, but good to check existence)
            
            // 3. Last Device
            // (String is safe)
        }
    }
}
```

### Step 2: Update `Source/InputProcessor.cpp`
Fix the Math.

**Refactor `addMappingFromTree` (or where PB math happens):**
```cpp
// ... inside PB logic ...
int globalRange = settingsManager.getPitchBendRange();

// SAFETY: Prevent Div/0
if (globalRange < 1) globalRange = 12; 

double stepsPerSemi = 8192.0 / globalRange;
// ...
```

### Step 3: Update `Source/StartupManager.cpp`
Implement the Fail-Safe.

**Refactor `initApp`:**
```cpp
void StartupManager::initApp()
{
    // 1. Load Settings (Global)
    File settingsFile = appDataFolder.getChildFile("OmniKeySettings.xml");
    if (settingsFile.exists())
    {
        settingsManager.loadFromXml(settingsFile);
    }

    // 2. Load Preset (Autoload)
    bool loadSuccess = false;
    if (autoloadFile.exists())
    {
        // Try to load
        presetManager.loadFromFile(autoloadFile);
        
        // Basic Integrity Check: Does it have layers?
        if (presetManager.getLayersNode().getNumChildren() > 0)
        {
            loadSuccess = true;
            // Also load Zones
            // zoneManager.restoreFromValueTree(...)
        }
    }

    // 3. Fallback (If file missing OR corrupt/empty)
    if (!loadSuccess)
    {
        DBG("StartupManager: Autoload missing or corrupt. Creating Defaults.");
        createFactoryDefault();
    }
}
```

---

**Generate code for: Updated `SettingsManager.cpp`, Updated `InputProcessor.cpp`, and Updated `StartupManager.cpp`.**