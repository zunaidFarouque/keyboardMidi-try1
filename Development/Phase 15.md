# 🤖 Cursor Prompt: Phase 15 - Startup State & Default Presets

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 14 Complete. The app has advanced features (Zones, Guitar Mode, Mappings), but they are lost immediately upon restart.
*   **Phase Goal:** Implement `StartupManager` to handle auto-loading state, creating a robust "Factory Default" experience, and debounced auto-saving.

**Strict Constraints:**
1.  **Architecture:** `StartupManager` orchestrates the saving/loading of other managers. It does not contain the logic for *how* to serialize a Zone, only *when* to do it.
2.  **Thread Safety:** Auto-save occurs via `juce::Timer` (Message Thread). Ensure data access during save is safe.
3.  **Paths:** Use `juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)`.

---

### Step 1: Update `Zone` & `ZoneManager` (Serialization)
We need to be able to save Zones to XML.

**1. `Zone` Class:**
*   `juce::ValueTree toValueTree();`: Serialize all members (name, root, scale, layout, inputKeys) to a Tree.
*   `static std::shared_ptr<Zone> fromValueTree(const juce::ValueTree& vt);`: Static factory to restore a Zone.

**2. `ZoneManager` Class:**
*   `juce::ValueTree toValueTree();`: Returns a parent node containing all Zone children + Global Transpose settings.
*   `void restoreFromValueTree(const juce::ValueTree& vt);`: Clears current zones and rebuilds them from the XML.

### Step 2: `StartupManager` (`Source/StartupManager.h/cpp`)
The persistence engine.

**Inheritance:**
*   `juce::Timer` (For auto-save debounce).
*   `juce::ValueTree::Listener` (To watch Preset/Mapping changes).
*   `juce::ChangeListener` (To watch Zone/Device changes).

**Members:**
*   References to `PresetManager`, `DeviceManager`, `ZoneManager`.
*   `juce::File appDataFolder;`
*   `juce::File autoloadFile;`

**Methods:**
*   `void initApp();`:
    *   Ensure AppData folder exists.
    *   Load `DeviceManager` config (Global settings).
    *   Check for `autoload.xml`.
    *   **If Exists:** Load XML. Pass relevant sub-nodes to `PresetManager` and `ZoneManager`.
    *   **If Missing:** Call `createFactoryDefault()`.
*   `void createFactoryDefault();`:
    *   Clear all managers.
    *   **Device:** Ensure "Master Input" alias exists.
    *   **Zone:** Create "Main Keys" (C Major, Linear, Keys Q->P). Add to `ZoneManager`.
    *   **Save:** Trigger an immediate save.
*   `void triggerSave();`: Starts the timer (2 seconds).
*   `timerCallback()`:
    *   Stop timer.
    *   Construct a Master XML (`<OmniKeySession>`).
    *   Add `PresetManager::getRootNode()`.
    *   Add `ZoneManager::toValueTree()`.
    *   Write to `autoload.xml`.

### Step 3: Integration (`MainComponent`)
1.  **Member:** Add `StartupManager startupManager;`.
2.  **Constructor:**
    *   Initialize `StartupManager` passing all other managers.
    *   Call `startupManager.initApp()`.
3.  **Destructor:**
    *   Call `startupManager.saveImmediate()` (Add this method to flush pending saves on close).

### Step 4: Update `CMakeLists.txt`
Add `Source/StartupManager.cpp`.

---

**Generate code for: `Zone` (updates), `ZoneManager` (updates), `StartupManager`, updated `MainComponent`, and `CMakeLists.txt` snippet.**