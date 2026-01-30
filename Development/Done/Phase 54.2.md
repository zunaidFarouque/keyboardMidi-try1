### 📋 Cursor Prompt: Phase 54.2 - Honest Logging

**Target File:** `Source/MainComponent.cpp`

**Task:**
Update `logEvent` to respect `SettingsManager::isStudioMode()`. If Studio Mode is OFF, the logger should assume Device ID 0 (Global), matching the Audio Engine's behavior.

**Specific Instructions:**

1.  **Refactor `MainComponent::logEvent`:**
    *   **Get Settings:** Access `settingsManager`.
    *   **Logic:**
        ```cpp
        // 1. Determine Effective Device (Match InputProcessor logic)
        uintptr_t effectiveDevice = device;
        if (!settingsManager.isStudioMode()) {
            effectiveDevice = 0; // Force Global if Studio Mode is OFF
        }
        
        // 2. Lookup Alias Hash
        juce::String aliasName = deviceManager.getAliasForHardware(effectiveDevice);
        uintptr_t viewHash = 0;
        if (aliasName.isNotEmpty() && aliasName != "Unassigned") {
            viewHash = static_cast<uintptr_t>(std::hash<juce::String>{}(aliasName));
        }

        // 3. Get Active Layer
        int layer = inputProcessor.getHighestActiveLayerIndex();

        // 4. Lookup in Visual Grid using 'viewHash' and 'layer'
        // ... existing lookup logic ...
        ```

2.  **Add Visual Indicator:**
    *   If `effectiveDevice == 0` but `device != 0`, append `(Studio Mode OFF)` to the log string "Dev: [HANDLE]" part, so the user knows why their specific device mapping is being ignored.

**Action:**
Apply this fix. Then, in the GUI, **Enable Studio Mode** in the Settings tab. Your Layer Command should start working immediately.