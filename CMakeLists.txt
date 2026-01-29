cmake_minimum_required(VERSION 3.22)

# 1. Project Setup
project(OmniKey VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 2. Add JUCE
add_subdirectory(JUCE)

# 3. Create the GUI App
juce_add_gui_app(OmniKey
    PRODUCT_NAME "OmniKey"
    VERSION 1.0.0
    COMPANY_NAME "OmniKey"
    NEEDS_STANDALONE_SHELL FALSE
)

# --- THIS IS THE MISSING MAGIC LINE ---
juce_generate_juce_header(OmniKey)
# --------------------------------------

# 4. Source Files
target_sources(OmniKey PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/RawInputManager.cpp
    Source/MidiEngine.cpp
    Source/VoiceManager.cpp
    Source/InputProcessor.cpp
    Source/PresetManager.cpp
    Source/MappingEditorComponent.cpp
    Source/MappingInspector.cpp
    Source/LayerListPanel.cpp
    Source/LogComponent.cpp
    Source/KeyNameUtilities.cpp
    Source/MidiNoteUtilities.cpp
    Source/PointerInputManager.cpp
    Source/DeviceManager.cpp
    Source/DeviceSetupComponent.cpp
    Source/QuickSetupWizard.cpp
    Source/ScaleUtilities.cpp
    Source/ScaleLibrary.cpp
    Source/ScaleEditorComponent.cpp
    Source/MusicalKeyboardComponent.cpp
    Source/Zone.cpp
    Source/ZoneManager.cpp
    Source/ZoneListPanel.cpp
    Source/ZonePropertiesPanel.cpp
    Source/GlobalPerformancePanel.cpp
    Source/ZoneEditorComponent.cpp
    Source/KeyboardLayoutUtils.cpp
    Source/VisualizerComponent.cpp
    Source/DetachableContainer.cpp
    Source/KeyChipList.cpp
    Source/StartupManager.cpp
    Source/ChordUtilities.cpp
    Source/StrumEngine.cpp
    Source/ExpressionEngine.cpp
    Source/PortamentoEngine.cpp
    Source/RhythmAnalyzer.cpp
    Source/SettingsManager.cpp
    Source/SettingsPanel.cpp
    Source/MiniStatusWindow.cpp
    Source/GridCompiler.cpp
)

# 5. Definitions (Fixes Macro Hell globally)
target_compile_definitions(OmniKey PRIVATE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
    JUCE_WEB_BROWSER_PATH=""
)

# 6. Link Libraries
target_link_libraries(OmniKey PRIVATE
    juce::juce_core
    juce::juce_gui_basics
    juce::juce_gui_extra   # Required for ColourSelector
    juce::juce_graphics
    juce::juce_events
    user32
    hid         # <--- ADD THIS (Required for hidsdi.h)
    setupapi    # <--- ADD THIS (Often required for device enumeration)
    juce::juce_audio_basics   # Required for MidiMessage
    juce::juce_audio_devices  # Required for MidiOutput

)