
### 📋 Cursor Prompt: Phase 51.1 - Architectural Split (Core vs App)

**Target File:** `CMakeLists.txt`

**Task:**
Refactor the build system to separate the "Business Logic" from the "GUI Application". This allows us to link the Logic into a Test Suite later.

**Specific Instructions:**

1.  **Define a Static Library (`MIDIQy_Core`):**
    *   Create a new target `juce_add_binary_data` (if needed) or just `add_library(MIDIQy_Core STATIC ...)`.
    *   **Move these Source Files** from the `MIDIQy` target to `MIDIQy_Core`:
        *   `Source/GridCompiler.cpp`
        *   `Source/InputProcessor.cpp`
        *   `Source/PresetManager.cpp`
        *   `Source/DeviceManager.cpp`
        *   `Source/ZoneManager.cpp`
        *   `Source/Zone.cpp`
        *   `Source/ScaleLibrary.cpp`
        *   `Source/SettingsManager.cpp`
        *   `Source/MidiEngine.cpp`
        *   `Source/VoiceManager.cpp`
        *   `Source/RawInputManager.cpp`
        *   `Source/PointerInputManager.cpp`
        *   `Source/ExpressionEngine.cpp`
        *   `Source/PortamentoEngine.cpp`
        *   `Source/StrumEngine.cpp`
        *   `Source/RhythmAnalyzer.cpp`
        *   `Source/ChordUtilities.cpp`
        *   `Source/ScaleUtilities.cpp`
        *   `Source/KeyNameUtilities.cpp`
        *   `Source/MidiNoteUtilities.cpp`
        *   `Source/KeyboardLayoutUtils.cpp`
    *   **Link Libraries to Core:**
        *   `juce::juce_core`
        *   `juce::juce_events`
        *   `juce::juce_audio_basics`
        *   `juce::juce_audio_devices`
        *   `user32`, `hid`, `setupapi`

2.  **Update the GUI App (`MIDIQy`):**
    *   **Keep only UI/Main files:**
        *   `Source/Main.cpp`
        *   `Source/MainComponent.cpp`
        *   `Source/VisualizerComponent.cpp`
        *   `Source/MappingEditorComponent.cpp`
        *   `Source/ZoneEditorComponent.cpp`
        *   `Source/SettingsPanel.cpp`
        *   `Source/LayerListPanel.cpp`
        *   `Source/ZoneListPanel.cpp`
        *   `Source/ZonePropertiesPanel.cpp`
        *   `Source/GlobalPerformancePanel.cpp`
        *   `Source/LogComponent.cpp`
        *   `Source/DeviceSetupComponent.cpp`
        *   `Source/MappingInspector.cpp`
        *   `Source/DetachableContainer.cpp`
        *   `Source/KeyChipList.cpp`
        *   `Source/MiniStatusWindow.cpp`
        *   `Source/QuickSetupWizard.cpp`
        *   `Source/StartupManager.cpp` (Keep here for now as it orchestrates everything)
    *   **Link:** Add `MIDIQy_Core` to `target_link_libraries` for `MIDIQy`.

3.  **Include Paths:**
    Ensure `target_include_directories` is set so `MIDIQy` can find headers in `Source/` (which is now shared).

**Goal:**
The app should compile and run exactly as before, but internally the logic is now in a reusable `.lib` file.