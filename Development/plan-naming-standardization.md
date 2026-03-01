# Naming Standardization Plan

**Goal:** Rename components, types, and identifiers for clarity. **No logic changes.**

**Principle:** Use consistent prefixes (Keyboard, Touchpad, Zone) and suffixes (Panel, Inspector, Manager) so AI/humans can quickly understand what each thing does.

---

## Phase 0: Pre-flight

- [ ] Create a git branch: `git checkout -b refactor/naming-standardization`
- [ ] Ensure all tests pass: `ctest` or build and run MIDIQy_Tests
- [ ] Delete or move `autoload.xml` if present (fresh start; old presets won't load after serialization renames)

---

## Phase 1: Touchpad Types & Config (Foundation)

**Order:** Types first, then everything that depends on them.

### 1.1 Rename `TouchpadMixerConfig` → `TouchpadLayoutConfig`

**Caution:** Do NOT replace `TouchpadMixerQuickPrecision`, `TouchpadMixerAbsRel`, `TouchpadMixerLockFree` – those stay.

| File | Change |
|------|--------|
| `Source/TouchpadMixerTypes.h` | `struct TouchpadMixerConfig` → `struct TouchpadLayoutConfig` |
| `Source/TouchpadMixerTypes.h` | Default name `"Touchpad Mixer"` → `"Touchpad Layout"` (optional) |
| All files referencing `TouchpadMixerConfig` | Replace with `TouchpadLayoutConfig` |

**Files that reference TouchpadMixerConfig:** (search `TouchpadMixerConfig` exactly)
- TouchpadMixerManager.h, .cpp
- TouchpadMixerListPanel.h, .cpp
- TouchpadMixerEditorComponent.h, .cpp
- TouchpadMixerDefinition.h, .cpp
- TouchpadGroupsPanel.h, .cpp
- GridCompiler.cpp, .h
- InputProcessor.cpp, .h
- TouchpadVisualizerPanel.cpp
- Tests: GridCompilerTests.cpp, TouchpadTabTests.cpp, UiStatePersistenceTests.cpp
- .cursor/rules/touchpad-layout-guide.mdc

### 1.2 Rename Mixer-specific enums (optional; keep if used widely)

These are Mixer-type-specific and could stay: `TouchpadMixerQuickPrecision`, `TouchpadMixerAbsRel`, `TouchpadMixerLockFree`. They apply only when `type == TouchpadType::Mixer`. **Recommendation:** Keep as-is for now; they're correctly scoped to Mixer behavior.

### 1.3 Rename file `TouchpadMixerTypes.h` → `TouchpadLayoutTypes.h`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/TouchpadMixerTypes.h` → `Source/TouchpadLayoutTypes.h` |
| 2 | Update all `#include "TouchpadMixerTypes.h"` → `#include "TouchpadLayoutTypes.h"` |

**Files with include:** TouchpadMixerManager, TouchpadMixerListPanel, TouchpadMixerEditorComponent, TouchpadMixerDefinition, TouchpadGroupsPanel, GridCompiler, InputProcessor, TouchpadVisualizerPanel, MappingTypes, SettingsDefinition, Tests (GridCompilerTests, TouchpadTabTests, InputProcessorTests, UiStatePersistenceTests)

---

## Phase 2: Touchpad Definition & Manager

### 2.1 Rename `TouchpadMixerDefinition` → `TouchpadLayoutDefinition`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/TouchpadMixerDefinition.h` → `Source/TouchpadLayoutDefinition.h` |
| 2 | Rename file: `Source/TouchpadMixerDefinition.cpp` → `Source/TouchpadLayoutDefinition.cpp` |
| 3 | In header: `class TouchpadMixerDefinition` → `class TouchpadLayoutDefinition` |
| 4 | Update all includes and references |
| 5 | Update `CMakeLists.txt`: `TouchpadMixerDefinition.cpp` → `TouchpadLayoutDefinition.cpp` |

**Files referencing TouchpadMixerDefinition:** TouchpadMixerEditorComponent, TouchpadMixerDefinition (self), MappingDefinition, SettingsDefinition

### 2.2 Rename `TouchpadMixerManager` → `TouchpadLayoutManager`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/TouchpadMixerManager.h` → `Source/TouchpadLayoutManager.h` |
| 2 | Rename file: `Source/TouchpadMixerManager.cpp` → `Source/TouchpadLayoutManager.cpp` |
| 3 | In header/cpp: `class TouchpadMixerManager` → `class TouchpadLayoutManager` |
| 4 | Update all includes and references (e.g. `TouchpadMixerManager *` → `TouchpadLayoutManager *`) |
| 5 | Update `CMakeLists.txt`: `TouchpadMixerManager.cpp` → `TouchpadLayoutManager.cpp` |

**Files referencing TouchpadMixerManager:** MainComponent, MappingEditorComponent, MappingInspector, TouchpadTabComponent, TouchpadMixerListPanel, TouchpadMixerEditorComponent, TouchpadGroupsPanel, StartupManager, PresetManager, VisualizerComponent, InputProcessor, DeviceManager, GridCompiler, TouchpadRelayoutDialog, Tests, Benchmarks

### 2.3 Rename serialization identifiers (no backward compat)

| Step | Action |
|------|--------|
| 1 | In `TouchpadLayoutManager.cpp`: `kTouchpadMixers("TouchpadMixers")` → `kTouchpadData("TouchpadData")` |
| 2 | In `TouchpadLayoutManager.cpp`: `kTouchpadMixer("TouchpadMixer")` → `kTouchpadLayout("TouchpadLayout")` |
| 3 | In `StartupManager.cpp`: `getChildWithName("TouchpadMixers")` → `getChildWithName("TouchpadData")` |
| 4 | In `PresetManager.h/cpp`: `getTouchpadMixersNode()` → `getTouchpadDataNode()` |
| 5 | In `MainComponent.cpp`: `getTouchpadMixersNode()` → `getTouchpadDataNode()` |

---

## Phase 3: Touchpad UI Components

### 3.1 Rename `TouchpadMixerListPanel` → `TouchpadListPanel`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/TouchpadMixerListPanel.h` → `Source/TouchpadListPanel.h` |
| 2 | Rename file: `Source/TouchpadMixerListPanel.cpp` → `Source/TouchpadListPanel.cpp` |
| 3 | In header: `class TouchpadMixerListPanel` → `class TouchpadListPanel` |
| 4 | Update all includes and references |
| 5 | Update `CMakeLists.txt`: `TouchpadMixerListPanel.cpp` → `TouchpadListPanel.cpp` |
| 6 | In `TouchpadTabComponent`: `TouchpadMixerListPanel listPanel` → `TouchpadListPanel listPanel` |

**Files referencing TouchpadMixerListPanel:** TouchpadTabComponent, Tests (TouchpadTabTests, UiStatePersistenceTests)

### 3.2 Rename `TouchpadMixerEditorComponent` → `TouchpadEditorPanel`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/TouchpadMixerEditorComponent.h` → `Source/TouchpadEditorPanel.h` |
| 2 | Rename file: `Source/TouchpadMixerEditorComponent.cpp` → `Source/TouchpadEditorPanel.cpp` |
| 3 | In header: `class TouchpadMixerEditorComponent` → `class TouchpadEditorPanel` |
| 4 | Update all includes and references |
| 5 | Update `CMakeLists.txt`: `TouchpadMixerEditorComponent.cpp` → `TouchpadEditorPanel.cpp` |
| 6 | In `TouchpadTabComponent`: `TouchpadMixerEditorComponent editorPanel` → `TouchpadEditorPanel editorPanel` |

**Files referencing TouchpadMixerEditorComponent:** TouchpadTabComponent, Tests (TouchpadTabTests), .cursor/rules/touchpad-layout-guide.mdc

---

## Phase 4: Keyboard Mapping Components

### 4.1 Rename `MappingInspector` → `KeyboardMappingInspector`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/MappingInspector.h` → `Source/KeyboardMappingInspector.h` |
| 2 | Rename file: `Source/MappingInspector.cpp` → `Source/KeyboardMappingInspector.cpp` |
| 3 | In header: `class MappingInspector` → `class KeyboardMappingInspector` |
| 4 | Update all includes and references |
| 5 | Update `CMakeLists.txt`: `MappingInspector.cpp` → `KeyboardMappingInspector.cpp` |
| 6 | In `MappingEditorComponent`: `MappingInspector inspector` → `KeyboardMappingInspector inspector` |

**Files referencing MappingInspector:** MappingEditorComponent, MappingDefinition, MappingDefaults, MappingTypes, SettingsDefinition, ZonePropertiesLogic, DeviceManager, GridCompiler, InputProcessor, PresetManager, SettingsPanel, VisualizerComponent, Tests (MappingInspectorLogicTests, InputProcessorTests, ChordUtilitiesTests)

### 4.2 Rename `MappingInspectorLogic` → `KeyboardMappingInspectorLogic` (optional)

If you want full consistency, rename the logic namespace too. **Recommendation:** Do it for consistency.

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/MappingInspectorLogic.h` → `Source/KeyboardMappingInspectorLogic.h` |
| 2 | Rename file: `Source/MappingInspectorLogic.cpp` → `Source/KeyboardMappingInspectorLogic.cpp` |
| 3 | In header/cpp: `namespace MappingInspectorLogic` → `namespace KeyboardMappingInspectorLogic` |
| 4 | Update all `MappingInspectorLogic::` → `KeyboardMappingInspectorLogic::` |
| 5 | Update `CMakeLists.txt` |
| 6 | Rename test file: `MappingInspectorLogicTests.cpp` → `KeyboardMappingInspectorLogicTests.cpp` |

**Files referencing MappingInspectorLogic:** MappingInspector (→ KeyboardMappingInspector), MappingDefinition, Tests

### 4.3 Rename `MappingEditorComponent` → `KeyboardMappingEditorComponent`

| Step | Action |
|------|--------|
| 1 | Rename file: `Source/MappingEditorComponent.h` → `Source/KeyboardMappingEditorComponent.h` |
| 2 | Rename file: `Source/MappingEditorComponent.cpp` → `Source/KeyboardMappingEditorComponent.cpp` |
| 3 | In header: `class MappingEditorComponent` → `class KeyboardMappingEditorComponent` |
| 4 | Update all includes and references |
| 5 | Update `CMakeLists.txt`: `MappingEditorComponent.cpp` → `KeyboardMappingEditorComponent.cpp` |
| 6 | In `MainComponent`: `MappingEditorComponent` → `KeyboardMappingEditorComponent`, `mappingEditor` → `keyboardMappingEditor` (or keep `mappingEditor` as member name for brevity) |

**Files referencing MappingEditorComponent:** MainComponent, Tests (UiStatePersistenceTests)

---

## Phase 5: PresetManager & StartupManager

### 5.1 Update PresetManager

| Step | Action |
|------|--------|
| 1 | In `PresetManager.h`: `getTouchpadMixersNode()` → `getTouchpadDataNode()` |
| 2 | In `PresetManager.cpp`: Implement `getTouchpadDataNode()` returning `getChildWithName("TouchpadData")` |
| 3 | Update `saveToFile` / load logic if it references TouchpadMixers by name |

### 5.2 Update StartupManager

| Step | Action |
|------|--------|
| 1 | Constructor param: `TouchpadMixerManager *` → `TouchpadLayoutManager *` |
| 2 | Member: `touchpadMixerManager` → `touchpadLayoutManager` |
| 3 | In `initApp`: `getChildWithName("TouchpadMixers")` → `getChildWithName("TouchpadData")` |
| 4 | In `saveSession`: ensure `touchpadLayoutManager->toValueTree()` is used (root will now be TouchpadData) |

---

## Phase 6: User-Facing Strings (Optional)

| Location | Current | Proposed |
|----------|---------|----------|
| Tab label | "Mappings" | "Keyboard" or "Keyboard Mappings" |
| Container title | "Mapping / Zones" | "Keyboard & Zones" or "Mappings & Zones" |
| Menu item | "Mapping / Zones" | Same as container |

**Files:** `MainComponent.cpp` – `addTab`, `editorContainer` ctor, `getMenuForIndex`

---

## Phase 7: Cursor Rules & Documentation

### 7.1 Update touchpad-layout-guide.mdc

| Change |
|--------|
| `TouchpadMixerConfig` → `TouchpadLayoutConfig` |
| `TouchpadMixerTypes.h` → `TouchpadLayoutTypes.h` |
| `TouchpadMixerDefinition` → `TouchpadLayoutDefinition` |
| `TouchpadMixerEditorComponent` → `TouchpadEditorPanel` |
| `TouchpadMixerManager` → `TouchpadLayoutManager` |

### 7.2 Update ARCHITECTURE_CONTEXT.md (if it exists)

Apply same renames in any architecture docs.

---

## Phase 8: Verification

- [ ] Full rebuild: `cmake --build build --config Debug`
- [ ] Run tests: `ctest -C Debug` or run MIDIQy_Tests
- [ ] Run app: verify all tabs load, touchpad/zone/keyboard editors work
- [ ] Create new preset, save, reload – verify no serialization errors
- [ ] Search for any remaining `TouchpadMixer` (except enum values like `TouchpadMixerQuickPrecision`), `MappingInspector`, `MappingEditorComponent` in Source/

---

## Execution Order Summary

| Phase | Description |
|-------|-------------|
| 0 | Pre-flight (branch, tests, backup) |
| 1 | TouchpadMixerConfig → TouchpadLayoutConfig; TouchpadMixerTypes.h → TouchpadLayoutTypes.h |
| 2 | TouchpadMixerDefinition → TouchpadLayoutDefinition; TouchpadMixerManager → TouchpadLayoutManager; serialization IDs |
| 3 | TouchpadMixerListPanel → TouchpadListPanel; TouchpadMixerEditorComponent → TouchpadEditorPanel |
| 4 | MappingInspector → KeyboardMappingInspector; MappingInspectorLogic → KeyboardMappingInspectorLogic; MappingEditorComponent → KeyboardMappingEditorComponent |
| 5 | PresetManager, StartupManager updates |
| 6 | User-facing strings (optional) |
| 7 | Cursor rules & docs |
| 8 | Verification |

---

## Quick Reference: New Names

| Old | New |
|-----|-----|
| TouchpadMixerConfig | TouchpadLayoutConfig |
| TouchpadMixerTypes.h | TouchpadLayoutTypes.h |
| TouchpadMixerDefinition | TouchpadLayoutDefinition |
| TouchpadMixerManager | TouchpadLayoutManager |
| TouchpadMixerListPanel | TouchpadListPanel |
| TouchpadMixerEditorComponent | TouchpadEditorPanel |
| MappingInspector | KeyboardMappingInspector |
| MappingInspectorLogic | KeyboardMappingInspectorLogic |
| MappingEditorComponent | KeyboardMappingEditorComponent |
| "TouchpadMixers" (serialization) | "TouchpadData" |
| "TouchpadMixer" (serialization) | "TouchpadLayout" |
| getTouchpadMixersNode() | getTouchpadDataNode() |

---

## Notes

- **TouchpadMixerQuickPrecision, TouchpadMixerAbsRel, TouchpadMixerLockFree:** Keep as-is; they are Mixer-type-specific.
- **TouchpadMappingConfig:** No change; already correct.
- **MappingListPanel:** Optional rename to `KeyboardMappingListPanel` for consistency; currently correct.
- **Clean build:** After file renames, run `cmake --build build --config Debug` from a clean or reconfigured build to avoid stale object files.
- **ZonePropertiesPanel, ZoneEditorComponent, etc.:** No change; already correct.
