Here is the Cursor prompt for **Phase 55.1**. This sets up the Data-Driven UI architecture and implements the schema for "Note" mappings.

### 📋 Cursor Prompt: Phase 55.1 - Mapping Definition Engine

**Target Files:**
1.  `CMakeLists.txt`
2.  `Source/MappingDefinition.h` (New File)
3.  `Source/MappingDefinition.cpp` (New File)
4.  `Source/Tests/MappingDefinitionTests.cpp` (New File)

**Task:**
Create the `MappingDefinition` system. This allows the Core logic to define the UI controls for a mapping dynamically. Implement the schema generation for **ActionType::Note**.

**Specific Instructions:**

1.  **Update `CMakeLists.txt`:**
    *   Add `Source/MappingDefinition.cpp` to `MIDIQy_Core`.
    *   Add `Source/Tests/MappingDefinitionTests.cpp` to `MIDIQy_Tests`.

2.  **Create `Source/MappingDefinition.h`:**
    *   Include `<JuceHeader.h>` and `"MappingTypes.h"`.
    *   Define the UI Data Structures:
    ```cpp
    struct InspectorControl {
        juce::String propertyId;      // ValueTree property (e.g. "data1")
        juce::String label;           // Display Label
        bool showSeparatorBefore = false;

        enum class Type { Slider, ComboBox, Toggle, LabelOnly, Color };
        Type controlType = Type::Slider;
        bool isEnabled = true;

        // Slider Constraints
        double min = 0.0;
        double max = 127.0;
        double step = 1.0;
        juce::String suffix;

        // Formatting
        enum class Format { Integer, NoteName, CommandName, LayerName };
        Format valueFormat = Format::Integer;

        // ComboBox Options (ID -> Text)
        std::map<int, juce::String> options;
    };

    using InspectorSchema = std::vector<InspectorControl>;

    class MappingDefinition {
    public:
        // Factory: Inspects the mapping state and returns the UI schema
        static InspectorSchema getSchema(const juce::ValueTree& mapping);
        
        // Helper to get friendly name for ActionType (e.g. "Note", "CC")
        static juce::String getTypeName(ActionType type);
    };
    ```

3.  **Create `Source/MappingDefinition.cpp`:**
    *   Implement `getSchema`.
    *   **Step 1:** Create common controls (Type Selector).
        *   Prop: "type", Type: ComboBox, Options: Note, CC, Command, etc.
    *   **Step 2:** Check `mapping.getProperty("type")`.
    *   **Step 3:** Implement logic for **"Note"** type:
        *   **Channel:** Slider (1-16).
        *   **Data1 (Pitch):** Slider (0-127), Label "Note", Format `NoteName`.
        *   **Data2 (Velocity):** Slider (0-127), Label "Velocity".
        *   **Vel Random:** Slider (0-64), Label "Vel Random +/-".
        *   **Transpose:** Toggle, Prop "ignoreTranspose", Label "Ignore Global Transpose", Separator Before = true. (Note: Logic inversion - we usually want "Follow" default, but let's stick to simple props for now. Let's call prop `followTranspose` default true?). *Correction:* User said "by default it should not follow global transpose". So prop `followTranspose` default `false`.
        *   **Release Behavior:** Toggle, Prop "sendNoteOff", Label "Send Note Off on Release", default true. (Simple start).
    *   **Return:** The vector.

4.  **Create `Source/Tests/MappingDefinitionTests.cpp`:**
    *   **Test 1: Schema Generation.**
        *   Create ValueTree with type "Note".
        *   Call `getSchema`.
        *   Assert vector size > 0.
        *   Find control with property "data1". Assert `label == "Note"` and `valueFormat == NoteName`.
        *   Find control with property "type". Assert it is a ComboBox.

**Goal:**
Verify that the Core can successfully describe the UI requirements for a Note mapping without the GUI code existing yet.