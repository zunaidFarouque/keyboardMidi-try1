---
name: reorganize-command-options
overview: Reorganize Command-type options for both Mappings and Touchpad tabs to use shared command categories and keep keyboard and touchpad behavior aligned.
todos:
  - id: cmd-schema-update
    content: Refactor MappingDefinition command schema to expose the 8 command categories and rely on getCommandOptions for labels
    status: completed
  - id: cmd-logic-update
    content: Extend MappingInspectorLogic and touchpad editor to map new categories to underlying CommandIDs and properties without duplicating logic
    status: completed
  - id: cmd-tests-update
    content: Update MappingDefinition and TouchpadTab tests to cover the new command categories and ensure behavior parity between Mappings and Touchpad tabs
    status: completed
isProject: false
---

### Files to inspect/change

- `Source/MappingDefinition.cpp` / `MappingDefinition.h`
- `Source/MappingInspectorLogic.cpp` / `MappingInspectorLogic.h`
- `Source/TouchpadMixerEditorComponent.cpp`
- `Source/MappingInspector.cpp`
- Existing tests: `Source/Tests/MappingDefinitionTests.cpp`, `Source/Tests/TouchpadTabTests.cpp`

### High-level approach

- **Single source of option labels**: Use `MappingDefinition::getCommandOptions()` as the canonical map of CommandID -> label, and avoid duplicating command labels when building the Command combo for either tab.
- **Category-based UI**: Keep actual command IDs as they are (SustainMomentary, Panic, Transpose, GlobalRootUp,

