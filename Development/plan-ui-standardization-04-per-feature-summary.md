# UI Standardization Plan 04 – Per-Feature Summary

**Focus:** One major part of the program per section. Use this for quick reference when working on a specific feature. Details and step-by-step instructions are in plans 01–03.

---

## 1. Keyboard Mappings

### 1.1 Current state

| Layer | What lives there |
|-------|-------------------|
| **Core** | `MappingDefinition::getSchema(mapping, ...)` – schema (InspectorControl list). `KeyboardMappingInspectorLogic::applyComboSelectionToMapping(mapping, def, selectedId, undo)` – apply combo to mapping ValueTree. `MappingCompiler`, `PresetManager` (preset with mappings). |
| **UI** | `KeyboardMappingEditorComponent` – list/add/delete/duplicate mappings, layer selection. `MappingListPanel` – list of mappings. `LayerListPanel` – layer list. `KeyboardMappingInspector` – builds controls from schema, **calls** `KeyboardMappingInspectorLogic::applyComboSelectionToMapping` on combo change. |

### 1.2 Standardization status

- **Good:** Inspector uses Logic for apply; schema is in Core.  
- **No duplication:** Keyboard mapping apply path is already single (Logic).  
- **Contract (for visual overhaul):** Do not change `InspectorControl::propertyId` or option IDs in MappingDefinition; you may change layout and styling of KeyboardMappingInspector and related panels.

### 1.3 Action items

- None required for standardization. When changing the look of the Mappings tab, keep propertyIds and option semantics; only change layout/theme.

---

## 2. Zones

### 2.1 Current state

| Layer | What lives there |
|-------|-------------------|
| **Core** | `ZoneDefinition::getSchema(zone)` – schema (ZoneControl list). `ZonePropertiesLogic::setZonePropertyFromKey(zone, propertyKey, value)` – apply slider/combo/toggle (and some composites). `Zone`, `ZoneManager`, `PresetManager` (preset with zones). |
| **UI** | `ZoneEditorComponent` – zone list + ZonePropertiesPanel. `ZoneListPanel` – list of zones. `ZonePropertiesPanel` – builds controls from schema but **does not call** ZonePropertiesLogic; it duplicates logic in lambdas (direct `zone->rootNote = ...`, etc.). Also contains `aliasNameToHash(aliasName)` (device alias → hash). |

### 2.2 Standardization status

- **Gap:** Panel duplicates Logic; two sources of truth. Logic is tested, panel is not.  
- **Gap:** `aliasNameToHash` is business logic in the UI file.

### 2.3 Action items (see plan 01)

1. Move **aliasNameToHash** to Core (DeviceManager or DeviceAliasUtilities); add tests; panel calls Core.  
2. Extend **ZonePropertiesLogic** if needed (e.g. zoneColor); add tests.  
3. Refactor **ZonePropertiesPanel** so every control change calls `ZonePropertiesLogic::setZonePropertyFromKey` (with correct value conversion); remove all direct `zone->...` assignments.  
4. **Contract (for visual overhaul):** Do not change ZoneControl propertyKey or option IDs; you may reorder rows, move panels, theme, beautify.

---

## 3. Touchpad (layouts + touchpad mappings)

### 3.1 Current state

| Layer | What lives there |
|-------|-------------------|
| **Core** | `TouchpadLayoutDefinition::getSchema(type)`, `getCommonLayoutHeader()`, `getCommonLayoutControls()` – schema for layout editor. `MappingDefinition::getSchema(..., forTouchpadEditor: true)` – schema for touchpad mapping part. `TouchpadLayoutManager`, config types in `TouchpadLayoutTypes.h`. `KeyboardMappingInspectorLogic` used by TouchpadEditorPanel for **some** mapping combo applies. |
| **UI** | `TouchpadTabComponent` – tab containing list + editor. `TouchpadListPanel`, `TouchpadGroupsPanel`, `TouchpadEditorPanel`, `TouchpadVisualizerPanel`. **TouchpadEditorPanel** has all “apply value to config” and “apply value to mapping” logic **inline** (getConfigValue, applyConfigValue, getMappingValue, applyMappingValue) – no TouchpadEditorLogic. |

### 3.2 Standardization status

- **Gap:** No Logic layer for touchpad layout config or touchpad-specific mapping properties; panel is the only place that knows how to apply propertyId → config/mapping.  
- **Good:** Schema comes from Core (TouchpadLayoutDefinition, MappingDefinition).

### 3.3 Action items (see plan 01)

1. Add **TouchpadEditorLogic** in Core:  
   - `applyLayoutConfigProperty(config, propertyId, value)` for layout config.  
   - `applyMappingProperty(mapping, propertyId, value, undo)` for touchpad mapping (or reuse/extend KeyboardMappingInspectorLogic where shared).  
2. Add **unit tests** for TouchpadEditorLogic (layout and mapping).  
3. Refactor **TouchpadEditorPanel** to call Logic instead of inline branches; keep only widget build and layout in the panel.  
4. **Contract (for visual overhaul):** Do not change propertyIds or option semantics in TouchpadLayoutDefinition / MappingDefinition; you may change layout and styling of the Touchpad tab.

---

## 4. Settings

### 4.1 Current state

| Layer | What lives there |
|-------|-------------------|
| **Core** | `SettingsManager` – get/set for pitch bend range, visualizer opacity, toggle key, performance mode key, remember UI state, debug mode, delay MIDI, type colors, etc. Persistence (XML) can be in Core or called from StartupManager. |
| **UI** | `SettingsDefinition::getSchema()` – returns InspectorSchema for Settings tab (currently in **GUI** target). `SettingsPanel` – builds UI from schema, calls `settingsManager.set*(...)` / `settingsManager.get*(...)` directly in callbacks. |

### 4.2 Standardization status

- **Acceptable:** SettingsManager is the single source of truth; panel is thin (get/set only). No duplicated business rules.  
- **Optional:** Move SettingsDefinition to Core so all schema definitions live in Core; then only widget construction stays in UI.

### 4.3 Action items

- **Required:** None for correctness.  
- **Optional:** Move SettingsDefinition to MIDIQy_Core; update CMake and includes.  
- **Contract (for visual overhaul):** Do not change settings keys or types; you may reorder controls, add sections, theme.

---

## 5. Cross-cutting: Main, startup, visualizer

| Area | In Core | In UI | Note |
|------|---------|-------|------|
| **MainComponent** | – | Owns engines, managers, tabs, panels; wires listeners and callbacks | Keep wiring here; no business rules. |
| **StartupManager** | – | Load/save preset, autoload, debounced save, listens to preset/device/zone/touchpad changes | Could move to Core later for load/save tests; not required for UI standardization. |
| **Visualizer** | – | VisualizerComponent, TouchpadVisualizerPanel, MiniStatusWindow | Pure UI; no logic to extract. |
| **Device / alias** | DeviceManager (alias list, etc.) | DeviceSetupComponent, alias combos in panels | aliasNameToHash move to Core (see Zones). |

---

## 6. Quick reference: which plan to use when

| If you are… | Use |
|-------------|-----|
| Extracting logic from UI to Core | Plan 01 (and 03 for test order). |
| Changing folder structure or preparing for themes/layout | Plan 02. |
| Making sure nothing breaks | Plan 03. |
| Working on one feature (Zones, Touchpad, etc.) | This doc (04) + the relevant section in 01. |
