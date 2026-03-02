# UI Standardization Plan 01 – Core/UI Separation and Logic Extraction

**Focus:** Ensure all business logic lives in Core and is invoked by UI via a single, testable path. Remove duplicated logic from UI components.

---

## 1. Current Inventory

### 1.1 Core library (`MIDIQy_Core`)

| Category | Files |
|----------|--------|
| **Logic namespaces** | `ZonePropertiesLogic.cpp/.h`, `KeyboardMappingInspectorLogic.cpp/.h` |
| **Schema (what to show)** | `ZoneDefinition.cpp/.h`, `MappingDefinition.cpp/.h`, `TouchpadLayoutDefinition.cpp/.h` |
| **Managers / state** | PresetManager, ZoneManager, DeviceManager, SettingsManager, TouchpadLayoutManager |
| **Engines** | MidiEngine, VoiceManager, InputProcessor, ExpressionEngine, PortamentoEngine, StrumEngine, RhythmAnalyzer |
| **Data & utilities** | Zone, MappingCompiler, ChordUtilities, ScaleUtilities, KeyNameUtilities, MidiNoteUtilities, KeyboardLayoutUtils, ColourContrast, PitchPadUtilities, TouchpadHidParser, RawInputManager, PointerInputManager, ScaleLibrary, CrashLogger |

Core links: `juce_core`, `juce_events`, `juce_audio_basics`, `juce_audio_devices`, `juce_graphics`, `juce_data_structures`. **No** `juce_gui_basics` or `juce_gui_extra`.

### 1.2 GUI app only (`MIDIQy`)

| Category | Files |
|----------|--------|
| **Main** | Main.cpp, MainComponent.cpp |
| **Panels** | ZonePropertiesPanel, ZoneListPanel, ZoneEditorComponent, MappingListPanel, LayerListPanel, KeyboardMappingEditorComponent, KeyboardMappingInspector, TouchpadListPanel, TouchpadGroupsPanel, TouchpadEditorPanel, TouchpadTabComponent, SettingsPanel, GlobalPerformancePanel, LogComponent |
| **Other UI** | VisualizerComponent, TouchpadVisualizerPanel, DeviceSetupComponent, DetachableContainer, KeyChipList, MiniStatusWindow, QuickSetupWizard, ScaleEditorComponent, MusicalKeyboardComponent, TouchpadRelayoutDialog, PercentageSplitLayout |
| **Schema / startup (in GUI target)** | SettingsDefinition.cpp/.h, StartupManager.cpp/.h |

### 1.3 Shared types used by both

- `MappingTypes.h`, `TouchpadLayoutTypes.h` (TouchpadTypes.h), `Zone.h`, `SettingsManager.h`, etc. are in Source/ and included by Core and/or GUI as needed.
- `InspectorControl` / `InspectorSchema` live in `MappingDefinition.h` (Core). `ZoneControl` / `ZoneSchema` in `ZoneDefinition.h` (Core). `SettingsDefinition` (GUI) returns `InspectorSchema` and depends on `MappingDefinition.h`.

---

## 2. Gaps: Logic That Lives in UI Today

### 2.1 ZonePropertiesPanel – does not use ZonePropertiesLogic

**Finding:** `ZonePropertiesPanel.cpp` builds UI from `ZoneDefinition::getSchema()` (correct) but **never calls `ZonePropertiesLogic::setZonePropertyFromKey()`**. Instead it duplicates the same rules in lambdas:

- Slider onChange: directly sets `zone->rootNote`, `zone->baseVelocity`, `zone->ghostVelocityScale`, etc. (lines ~398–426 and similar).
- ComboBox onChange: directly sets `zone->polyphonyMode`, `zone->playMode`, `zone->chordType`, etc. (lines ~627–660).
- Toggle onChange: directly sets `zone->useGlobalRoot`, `zone->useGlobalScale`, etc. (lines ~715–729).
- Composite rows (StrumTimingVariation, AddBassWithOctave, DelayRelease): directly set `zone->strumTimingVariationOn`, `zone->strumTimingVariationMs`, `zone->addBassNote`, `zone->bassOctaveOffset`, `zone->delayReleaseOn`, `zone->releaseDurationMs`.
- Color row: `zone->zoneColor = selector->getCurrentColour()` (line ~1098).

**Risk:** Logic can drift from `ZonePropertiesLogic` (which is tested); two sources of truth.

**Also in ZonePropertiesPanel:**  
- `static uintptr_t aliasNameToHash(const juce::String &aliasName)` (lines 8–14) – device alias → hash. This is business logic and should live in Core (e.g. DeviceManager or a small DeviceAliasUtilities).

### 2.2 TouchpadEditorPanel – no Logic layer

**Finding:** All “apply value to config/mapping” logic is inline in `TouchpadEditorPanel.cpp`:

- `getConfigValue` / `getMappingValue`: large if-chains by `propertyId` (region, layerId, layoutGroupId, numFaders, ccStart, midiChannel, quickPrecision, absRel, lockFree, drumPad*, harmonic*, chordPad*, fx*, etc.).
- `applyConfigValue` / `applyMappingValue`: same, with `currentConfig.* = ...` and `mapping.setProperty(...)`.

There is no `TouchpadEditorLogic` (or similar) in Core. The panel is the only place that knows how a combo ID or slider value maps to config/mapping state.

**Risk:** Hard to unit-test; any new touchpad layout type or property requires editing a large UI file; visual overhaul is risky because logic is mixed with layout.

### 2.3 SettingsPanel – direct SettingsManager get/set

**Finding:** SettingsPanel uses `SettingsDefinition::getSchema()` (GUI target) and in callbacks calls `settingsManager.setPitchBendRange()`, `settingsManager.setVisualizerXOpacity()`, etc. directly. There is no “SettingsLogic” namespace.

**Assessment:** SettingsManager is in Core and is a simple key/value style API. Direct get/set from the panel is acceptable **if** we consider SettingsManager the single source of truth and do not encode complex rules in the panel. Optional improvement: move `SettingsDefinition` into Core (next to MappingDefinition) so all schema definitions live in Core; then only the widget construction stays in the UI. Low priority compared to Zone and Touchpad.

### 2.4 StartupManager – in GUI target

**Finding:** StartupManager (load/save preset, autoload, debounced save, ValueTree listener) is in the GUI app. It uses Core types (PresetManager, ZoneManager, DeviceManager, TouchpadLayoutManager, SettingsManager).

**Assessment:** Could be moved to Core if we want “application lifecycle” to be testable without GUI (e.g. load/save round-trip tests). Not required for UI standardization; note for later if desired.

---

## 3. Logic Extraction Plan

### 3.1 ZonePropertiesPanel → use ZonePropertiesLogic only

**Objective:** Panel must not assign to `zone->*` itself. Every change must go through `ZonePropertiesLogic::setZonePropertyFromKey()` (or a new function for colour).

**Steps:**

1. **Extend ZonePropertiesLogic (Core)**  
   - Add handling for **composite** properties so the panel can call one Logic API per user action:
     - Ensure `setZonePropertyFromKey` already supports: `strumTimingVariationOn`, `strumTimingVariationMs`, `addBassNote`, `bassOctaveOffset`, `delayReleaseOn`, `releaseDurationMs` (already in ZonePropertiesLogic).
   - Add **colour:** e.g. `setZonePropertyFromKey(zone, "zoneColor", juce::var(zone->zoneColor.toString()))` or a dedicated `bool setZoneColor(Zone* zone, const juce::Colour& c)`. If using `var`, store as string (Colour::toString()); in Logic, parse and set `zone->zoneColor`. Add a unit test in ZonePropertiesLogicTests.

2. **Value conversion in panel**  
   - For sliders that use a different scale than Logic (e.g. `ghostVelocityScale` 0–100 in UI vs 0.0–1.0 in Logic): panel should pass the value Logic expects. Example: `ZonePropertiesLogic::setZonePropertyFromKey(zone, "ghostVelocityScale", sliderValue / 100.0)`.

3. **Replace all direct `zone->...` assignments in ZonePropertiesPanel**  
   - Slider: `ZonePropertiesLogic::setZonePropertyFromKey(zone, def.propertyKey, value)` (with value conversion where needed).  
   - Combo: same with `selectedId` or equivalent.  
   - Toggle: same with `getToggleState()`.  
   - Composite rows: one or two calls per row (e.g. strumTimingVariationOn + strumTimingVariationMs).  
   - Color: call new Logic API (e.g. setZonePropertyFromKey with "zoneColor" and var from Colour::toString()).  
   - After each change: call existing `rebuildIfNeeded()` / refresh as today.

4. **Tests**  
   - Run existing ZonePropertiesLogicTests; add tests for any new keys (e.g. zoneColor).  
   - Run full test suite after panel changes.

**Files to touch:**  
- `Source/ZonePropertiesLogic.cpp`, `ZonePropertiesLogic.h` (add zoneColor if not already covered).  
- `Source/ZonePropertiesPanel.cpp` (remove duplicate logic; call Logic only).  
- `Source/Tests/ZonePropertiesLogicTests.cpp` (new cases if needed).

---

### 3.2 aliasNameToHash → Core

**Objective:** Remove `static uintptr_t aliasNameToHash(const juce::String &aliasName)` from ZonePropertiesPanel and use a Core API instead.

**Steps:**

1. **Add to Core** (choose one):
   - **Option A:** `DeviceManager::aliasNameToDeviceHandleHash(const juce::String& aliasName)` that returns 0 for "Any / Master", "Global (All Devices)", "Global", "Unassigned", empty; otherwise returns a stable hash (e.g. `std::hash<juce::String>{}(aliasName)` as uintptr_t).  
   - **Option B:** New file `Source/DeviceAliasUtilities.cpp/.h` in Core with `DeviceAliasUtilities::aliasNameToHash(aliasName)` and same rules.

2. **Unit test** in a new or existing test file: a few names (empty, "Global", "Any / Master", "Unassigned", "Device A") → 0 or non-zero as expected.

3. **ZonePropertiesPanel:** Remove `aliasNameToHash`; call `DeviceManager::aliasNameToDeviceHandleHash(...)` or `DeviceAliasUtilities::aliasNameToHash(...)`.

**Files to touch:**  
- Core: `DeviceManager` or new `DeviceAliasUtilities`.  
- `Source/ZonePropertiesPanel.cpp`.  
- `Source/Tests/` (new or existing test file).

---

### 3.3 TouchpadEditorPanel → introduce TouchpadEditorLogic (Core)

**Objective:** Move all “propertyId + value → config or mapping” logic into a Core namespace so it can be tested and so the panel only builds UI and calls Logic.

**Scope:** TouchpadEditorPanel has two modes: editing a **layout config** (TouchpadLayoutConfig) and editing a **touchpad mapping** (ValueTree). Logic to extract:

- **Layout config:** given `propertyId` and value (var), apply to a `TouchpadLayoutConfig` (or the appropriate config struct).  
- **Mapping:** given `propertyId` and value, apply to a mapping ValueTree (with UndoManager). Some of this may overlap with `KeyboardMappingInspectorLogic::applyComboSelectionToMapping` for shared mapping properties; for touchpad-specific properties we need a single place (either extend KeyboardMappingInspectorLogic or add TouchpadEditorLogic that applies to mapping ValueTree).

**Suggested approach:**

1. **Create `TouchpadEditorLogic` namespace in Core** (new files `TouchpadEditorLogic.cpp/.h` in MIDIQy_Core).  
   - `applyLayoutConfigProperty(TouchpadLayoutConfig& config, const juce::String& propertyId, const juce::var& value)` – implement all current `applyConfigValue` branches.  
   - `applyMappingProperty(juce::ValueTree& mapping, const juce::String& propertyId, const juce::var& value, juce::UndoManager* undo)` – for touchpad mapping–specific properties; for shared ones, consider calling into existing MappingDefinition/InspectorLogic where possible so we don’t duplicate mapping apply rules.

2. **Add unit tests** for TouchpadEditorLogic (layout config and mapping) covering main propertyIds and value types. Use existing TouchpadLayoutConfig and ValueTree test fixtures where applicable.

3. **TouchpadEditorPanel:**  
   - In `applyConfigValue`: replace inline branches with `TouchpadEditorLogic::applyLayoutConfigProperty(currentConfig, propertyId, value)`.  
   - In `applyMappingValue`: replace inline branches with `TouchpadEditorLogic::applyMappingProperty(...)` (and existing KeyboardMappingInspectorLogic where it already applies).  
   - Keep in the panel only: widget creation, layout (resized), and reading current config/mapping for display (getConfigValue/getMappingValue can stay or be moved to Logic as “get” helpers; getting is less critical than “set” for a single source of truth).

4. **Iterate in small steps:** e.g. first move one group of layout properties (e.g. region, layerId, midiChannel), add tests, then do next group; same for mapping.

**Files to touch:**  
- New: `Source/TouchpadEditorLogic.cpp`, `Source/TouchpadEditorLogic.h`.  
- `CMakeLists.txt`: add TouchpadEditorLogic to MIDIQy_Core.  
- `Source/TouchpadEditorPanel.cpp`: remove inline apply logic; call Logic.  
- `Source/Tests/`: new `TouchpadEditorLogicTests.cpp` (or similar).

**Note:** TouchpadEditorPanel is large; do this in phases (e.g. layout first, then mapping) and run tests after each phase.

---

### 3.4 SettingsDefinition location (optional)

**Objective:** If we want all “schema” definitions in Core, move SettingsDefinition into Core.

**Steps:**

1. Move `SettingsDefinition.cpp/.h` from GUI to Core (add to MIDIQy_Core in CMakeLists; remove from MIDIQy target).  
2. SettingsDefinition only depends on MappingDefinition (InspectorSchema); MappingDefinition is already in Core.  
3. SettingsPanel (GUI) continues to call `SettingsDefinition::getSchema()` and build widgets; no change to contract.

**Priority:** Low; can be done after Zone and Touchpad logic extraction.

---

## 4. Dependency Rules (to preserve)

- **Core** must not depend on `juce_gui_basics` or `juce_gui_extra`. It may use `juce::String`, `juce::var`, `juce::ValueTree`, `juce::Colour` (from juce_graphics), etc.
- **UI** components may depend on Core and on JUCE GUI. They must not reimplement business rules that belong in Core.
- **Logic namespaces** (ZonePropertiesLogic, KeyboardMappingInspectorLogic, and later TouchpadEditorLogic) must be callable from tests without MessageManager or GUI.

---

## 5. Order of Implementation (recommended)

1. **aliasNameToHash** (small, low risk).  
2. **ZonePropertiesPanel → ZonePropertiesLogic** (Logic already exists and is tested; panel refactor only).  
3. **TouchpadEditorLogic** (larger; do layout first, then mapping, in small steps).  
4. **SettingsDefinition to Core** (optional, last).

After each step: build, run full test suite, and optionally run a quick manual smoke (e.g. change one zone property, one mapping, save/load).

---

## 6. Coordination with plan-naming-standardization.md

If you are also doing the renames from `plan-naming-standardization.md` (e.g. TouchpadMixer* → TouchpadLayout*), do **renames first** or in a dedicated branch, then do this logic extraction. That way TouchpadEditorLogic is written against the final type names (e.g. TouchpadLayoutConfig) and we avoid double churn in the same files.
