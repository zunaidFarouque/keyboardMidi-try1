# UI Standardization Plan 02 – UI Organization and Contract Stability

**Focus:** How to organize UI code (optional folder structure) and **what must stay stable** so that later visual overhauls (themes, moving buttons, beautification) do not break main logic.

---

## 1. Contract: What UI and Core Agree On

### 1.1 Zone properties

| Contract | Owner | Used by UI |
|----------|--------|------------|
| **Schema** | `ZoneDefinition::getSchema(zone)` (Core) | ZonePropertiesPanel builds rows from it. |
| **Property keys** | Same as in schema: `"rootNote"`, `"baseVelocity"`, `"playMode"`, etc. | Panel passes these to `ZonePropertiesLogic::setZonePropertyFromKey(zone, propertyKey, value)`. |
| **Value types** | Logic expects: slider → int/double (e.g. ghostVelocityScale 0.0–1.0), combo → int (option id), toggle → bool, color → string or Colour per Logic API. | Panel must pass values in that form (convert slider 0–100 to 0.0–1.0 if needed). |

**Stable for visual overhaul:**  
- Do **not** change propertyKey strings or the set of keys in ZoneDefinition.  
- Do **not** change the meaning of option IDs in ZoneControl options.  
- You **may** change: layout (order of rows, which panel holds what), sizes, colours, fonts, LookAndFeel, adding/removing decorative elements.

### 1.2 Keyboard / mapping inspector

| Contract | Owner | Used by UI |
|----------|--------|------------|
| **Schema** | `MappingDefinition::getSchema(mapping, pitchBendRange, forTouchpadEditor)` (Core) | KeyboardMappingInspector, TouchpadEditorPanel (for mapping part). |
| **Apply** | `KeyboardMappingInspectorLogic::applyComboSelectionToMapping(mapping, def, selectedId, undoManager)` (Core) | Inspector and TouchpadEditorPanel when user changes a combo. |
| **Property IDs** | `InspectorControl::propertyId` (e.g. `"data1"`, `"data2"`, type, release behavior, etc.) | Same in both editors. |

**Stable for visual overhaul:**  
- Do **not** change InspectorControl propertyId values or option ID semantics.  
- You **may** change: layout of controls, which tab they appear on, styling.

### 1.3 Touchpad layout / mapping (after TouchpadEditorLogic exists)

| Contract | Owner | Used by UI |
|----------|--------|------------|
| **Layout schema** | `TouchpadLayoutDefinition::getSchema(type)`, `getCommonLayoutHeader()`, `getCommonLayoutControls()` (Core) | TouchpadEditorPanel. |
| **Apply layout** | `TouchpadEditorLogic::applyLayoutConfigProperty(config, propertyId, value)` (Core) | TouchpadEditorPanel. |
| **Apply mapping** | `TouchpadEditorLogic::applyMappingProperty(...)` and/or `KeyboardMappingInspectorLogic` (Core) | TouchpadEditorPanel. |

**Stable for visual overhaul:** Same idea: keep propertyId and value semantics; change layout and look freely.

### 1.4 Settings

| Contract | Owner | Used by UI |
|----------|--------|------------|
| **Schema** | `SettingsDefinition::getSchema()` (currently GUI target; optional move to Core) | SettingsPanel. |
| **Storage** | `SettingsManager` get/set (Core) | SettingsPanel calls these in onChange. |

**Stable for visual overhaul:** Do not change settings keys or types; change layout and styling freely.

---

## 2. What You Can Change During a Visual Overhaul

Without breaking main logic, you **may**:

- **Themes:** Introduce a LookAndFeel, theme manager, or colour palette; change all colours/fonts from a single place.
- **Layout:** Move controls to different tabs, reorder rows, split into collapsible sections, move panels (e.g. Zone list left vs right).
- **Beautification:** Change component sizes, margins, rounded corners, icons, separators, spacing.
- **Structure:** Reorganize files into subfolders (e.g. Source/UI/Panels/), rename UI-only classes for clarity, as long as they still call the same Core/Logic APIs with the same contract.

You **must not** (without a coordinated change in Core and tests):

- Change **propertyKey** / **propertyId** strings or their meaning.  
- Change **option IDs** (combo id → meaning) in schema definitions.  
- Put business rules (e.g. “when root note changes, also update X”) in the UI; keep them in Core/Logic.

---

## 3. Optional: UI Folder Structure

Current state: all UI sources live under `Source/` next to Core. To make “UI vs Core” obvious and to group by feature, you can introduce a subfolder for UI only (Core stays in Source/ at top level so include paths stay simple).

### 3.1 Option A: Single UI folder

```
Source/
  UI/
    Panels/           # ZonePropertiesPanel, ZoneListPanel, MappingListPanel, ...
    Editors/          # KeyboardMappingEditorComponent, ZoneEditorComponent, TouchpadEditorPanel, ...
    Components/       # KeyChipList, DetachableContainer, MusicalKeyboardComponent, ...
    MainComponent.cpp
    Main.cpp          # or keep at Source/Main.cpp
  ZoneDefinition.cpp  # Core
  ...
```

- **CMake:** Add `Source/UI` to include path for MIDIQy target; or keep `Source` and use `#include "UI/Panels/ZonePropertiesPanel.h"` etc.  
- **Include paths:** Either `#include "ZonePropertiesPanel.h"` (if UI is in include path) or `#include "UI/Panels/ZonePropertiesPanel.h"`. Choose one and stick to it.

### 3.2 Option B: Feature-based UI folders

```
Source/
  UI/
    Keyboard/         # KeyboardMappingEditorComponent, MappingListPanel, LayerListPanel, KeyboardMappingInspector
    Zones/            # ZoneEditorComponent, ZoneListPanel, ZonePropertiesPanel
    Touchpad/         # TouchpadTabComponent, TouchpadListPanel, TouchpadEditorPanel, TouchpadGroupsPanel, TouchpadVisualizerPanel
    Settings/         # SettingsPanel, GlobalPerformancePanel
    Shared/           # DetachableContainer, KeyChipList, PercentageSplitLayout, LogComponent
    MainComponent.cpp
  ...
```

Same CMake/include considerations as Option A.

### 3.3 Recommendation

- **Do not** move Core files into UI; keep Core at `Source/*.cpp` so existing includes and tests keep working.  
- If you introduce `Source/UI/`, do it as a **refactor step**: move one component at a time, fix includes and CMake, build and test.  
- This is **optional**; the main benefit of standardization is “UI calls Logic only and respects the contract,” not the folder layout. Folder layout is for maintainability and clarity.

---

## 4. Checklist Before Starting a Visual Overhaul

Use this so that themes/layout/beautification don’t accidentally break logic:

- [ ] All Core/Logic refactors from plan 01 are done (or you know exactly which UI files still have inline logic and will not touch those code paths).
- [ ] You have run the full test suite and it passes (see plan 03).
- [ ] You know the **contract** for the screens you will change: propertyKey/propertyId and option IDs must stay; only layout, styling, and widget hierarchy may change.
- [ ] You will not add new “when user does X, do Y” rules in the UI; new rules go in Core/Logic.
- [ ] After UI changes, you will run the test suite again and a quick manual smoke (change one zone, one mapping, save/load).

---

## 5. Summary

- **Contract:** Schema and property keys/IDs and value types are owned by Core and must stay stable.  
- **Visual overhaul:** Change only presentation (layout, theme, beautification); do not change keys, option semantics, or business rules.  
- **Organization:** Optional UI subfolder (e.g. Source/UI/) for clarity; not required for correctness.
