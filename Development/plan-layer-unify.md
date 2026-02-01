# Plan: Combine Layer commands into one "Layer" option with Style dropdown

## Current state

- **Command dropdown** shows three separate entries:
  - **Layer Momentary** (data1 = 10) — hold to switch layer, release to revert
  - **Layer Toggle** (data1 = 11) — press to flip layer on/off
  - ~~Layer Solo (data1 = 12)~~ — **removed** (unnecessary for musical performance)
- Each uses **data2** = target layer ID (0–8); "Target Layer" combo is shown when 10 or 11 is selected.
- **InputProcessor** and **GridCompiler** use data1 (10/11) and data2 directly.

## Goal

- **Single "Layer" entry** in the Command dropdown (like Sustain, Panic, Transpose).
- When "Layer" is selected, show:
  1. **Style** dropdown: choose behavior (**Hold to switch** / **Toggle layer** only; Solo removed).
  2. **Target Layer** dropdown: unchanged (data2 = layer ID).
- **No backward compatibility** — project is not published yet.

---

## Implementation plan

### 1. MappingDefinition.cpp

- **Unify Layer in Command list**
  - Add `isLayer = (cmdId == 10 || cmdId == 11)`.
  - When `isSustain`: use virtual `commandCategory` and show "Sustain" (100).
  - When **isLayer**: use virtual `commandCategory` and show **one** "Layer" entry (110):
    - Remove options **10**, **11**, **12** from the Command dropdown.
    - Add one option: **110 = "Layer"** (sentinel ID).
  - Command dropdown options: 100=Sustain, 3=Latch Toggle, 4=Panic, 6=Transpose, 8=Global Mode Up, 9=Global Mode Down, **110=Layer** (no 10/11/12).
- **Schema when Command is "Layer" (data1 10 or 11)**
  - Use **commandCategory** for the main Command control when `isLayer`.
  - Add **Style** dropdown (virtual **layerStyle**):
    - Combo ID 1 → "Hold to switch" (data1 = 10)
    - Combo ID 2 → "Toggle layer" (data1 = 11)
  - Keep **Target Layer** (data2) combo unchanged.
- **Label for Command row**: When `isLayer`, use label "Layer".

### 2. MappingInspector.cpp

- **commandCategory display**: If data1 is **10 or 11** → show "Layer" (110).
- **commandCategory onChange**: When user selects **110** ("Layer") → set **data1 = 10** (default).
- **layerStyle (new virtual property)**
  - **Display**: from data1 → combo: 10→1, 11→2 (1-based).
  - **OnChange**: combo ID 1→ data1=10; 2→11. **actualProp = "data1"**.
  - Trigger **rebuildUI** when layerStyle changes.
- **data2 (Target Layer)**: No change.

### 3. GridCompiler.cpp

- Remove **LayerSolo** case from `getCommandLabel` and `isLayerCommand`.

### 4. InputProcessor.cpp

- Remove **LayerSolo** branch (data1 == 12 handling).

### 5. MappingTypes.h

- Remove `LayerSolo = 12` from CommandID enum.

### 6. Tests

- Remove **LayerSoloClearsOtherLayers** test.
- Remove **LayerSoloNotInherited** test (or repurpose to LayerToggle).
- Add **LayerUnifiedUI** test: mapping with data1=10, data2=1 behaves as Layer Momentary → Layer 1.

---

## Summary table

| Area                 | Change |
|----------------------|--------|
| MappingTypes.h       | Remove LayerSolo. |
| MappingDefinition    | Add isLayer (10,11); remove 10/11/12, add 110="Layer"; when Layer: commandCategory + layerStyle (Hold/Toggle) + data2. Remove Layer Solo from type options. |
| MappingInspector     | commandCategory: 10/11→110; on select 110 set data1=10. Add layerStyle: 10→1, 11→2; onChange set data1=10/11. |
| GridCompiler         | Remove LayerSolo from getCommandLabel and isLayerCommand. |
| InputProcessor       | Remove LayerSolo branch. |
| Tests                | Remove LayerSoloClearsOtherLayers, LayerSoloNotInherited; add LayerUnifiedUI. |

---

## Edge cases

- **Multi-selection**: commandCategory shows 110 only if all selected have data1 in 10–11.
- **Picking "Layer" for the first time**: Sets data1 = 10 (Hold to switch); user can then change Style to Toggle.
