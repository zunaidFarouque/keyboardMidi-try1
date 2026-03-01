# Display Strings Audit – Naming Standardization

Audit of all user-facing and display strings to check alignment with the naming standardization (Touchpad Layout, Keyboard Mappings, etc.).

**Status: All changes applied** (Keyboard Mapping used instead of Mapping for clarity, since we have both keyboard and touchpad mappings.)

---

## 1. Main UI (MainComponent.cpp)

| Location | String | Status |
|----------|--------|--------|
| Line 68 | `"Keyboard & Zones"` | ✅ Applied |
| Line 283 | `"Keyboard Mappings"` (tab) | ✅ Applied |
| Line 1127 | `"Keyboard & Zones"` (menu) | ✅ Applied |

**Other tabs:** "Zones", "Touchpad", "Settings" – already correct.

---

## 2. Touchpad List Panel (TouchpadListPanel.cpp)

| Location | String | Status |
|----------|--------|--------|
| Line 238 | `"Empty layout"` | ✅ OK |
| Line 240 | `"Empty touchpad mapping"` | ✅ OK |
| Line 283 | `def.name = "Touchpad Layout"` | ✅ Applied |
| Line 287 | `cfg.name = "Touchpad Mapping"` | ✅ OK (touchpad mapping) |
| Line 397 | `label = juce::String(layouts[...].name)` | Uses stored name |
| Line 401 | `"[Map] " + juce::String(m.name)` | ✅ OK |

**Menu submenus:** "Mixers", "Drum Pads", "Controllers", "Pitch Bend", "Combos" – OK (Mixer is a touchpad type).

---

## 3. Keyboard Mapping Editor (KeyboardMappingEditorComponent.cpp)

| Location | String | Status |
|----------|--------|--------|
| Line 16 | `"Press any key to add keyboard mapping..."` | ✅ Applied |
| Line 153 | `"Please select a keyboard mapping to duplicate."` | ✅ Applied |
| Line 179 | `"Please select one or more keyboard mappings to move."` | ✅ Applied |
| Line 210 | `"Please select one or more keyboard mappings to delete."` | ✅ Applied |
| Line 217 | `"Delete the selected keyboard mapping?"` | ✅ Applied |
| Line 219–220 | `"Delete N selected keyboard mappings?"` | ✅ Applied |
| Line 224 | `"Delete Keyboard Mappings"` (dialog title) | ✅ Applied |
| Line 241 | `undoManager.beginNewTransaction("Delete Keyboard Mappings")` | ✅ Applied |
| Line 289 | `"No keyboard mappings. Click '+' to add."` | ✅ Applied |

---

## 4. Touchpad Editor Panel (TouchpadEditorPanel.cpp)

| Location | String | Status |
|----------|--------|--------|
| Line 1221 | `createSeparator("Touchpad Mapping", ...)` | ✅ Applied |

---

## 5. Touchpad Layout Manager (TouchpadLayoutManager.cpp)

| Location | Current String | Status | Proposed |
|----------|----------------|--------|----------|
| Line 249 | `"Touchpad Layout"` (default when loading) | ✅ Updated | – |
| Line 372 | `"Touchpad Mapping"` (fallback for mapping name) | ✅ OK | – |

---

## 6. Touchpad Layout Types (TouchpadLayoutTypes.h)

| Location | Current String | Status | Proposed |
|----------|----------------|--------|----------|
| Line 90 | `std::string name = "Touchpad Layout"` | ✅ Updated | – |

---

## 7. Touchpad Layout Definition (TouchpadLayoutDefinition.cpp)

| Location | Current String | Status | Proposed |
|----------|----------------|--------|----------|
| Line 20 | `"Mixer"` (type option) | ✅ OK | Keep – TouchpadType::Mixer |
| Line 21 | `"Drum Pad / Launcher"` | ✅ OK | – |
| Line 22 | `"Chord Pad"` | ✅ OK | – |
| Line 36 | `"Touchpad group"` | ✅ OK | – |

---

## 8. GridCompiler (GridCompiler.cpp)

| Location | Current String | Status | Proposed |
|----------|----------------|--------|----------|
| Line 750 | `"Mapping: " + aliasName` or `"Mapping"` | – | Debug/slot labels; not primary UI |
| Line 1409 | Same | – | Same |

---

## 9. Tests (must match new defaults)

| File | Location | String | Status |
|------|----------|--------|--------|
| TouchpadTabTests.cpp | 16 | `def.name = "Touchpad Layout"` | ✅ Applied |
| TouchpadTabTests.cpp | 23 | `cfg.name = "Touchpad Mapping"` | ✅ OK |
| TouchpadTabTests.cpp | 60 | `EXPECT_EQ(..., "Touchpad Layout")` | ✅ Applied |
| TouchpadTabTests.cpp | 79 | `EXPECT_EQ(m.name, "Touchpad Mapping")` | ✅ OK |
| TouchpadTabTests.cpp | 320 | `EXPECT_EQ(r.name, "Touchpad Mapping")` | ✅ OK |

---

## Summary: All Changes Applied

- **MainComponent.cpp**: "Keyboard & Zones", "Keyboard Mappings" tab, menu item
- **TouchpadListPanel.cpp**: "Touchpad Layout" default for empty layout
- **KeyboardMappingEditorComponent.cpp**: All "mapping" → "keyboard mapping" (overlay, empty state, delete dialogs, duplicate/move alerts)
- **TouchpadEditorPanel.cpp**: Separator "Mapping" → "Touchpad Mapping"
- **TouchpadTabTests.cpp**: "Touchpad Layout" in makeEmptyLayout and expectations
- **MainComponent.cpp, SettingsManager.cpp**: Comments "Mappings=0" → "Keyboard Mappings=0"

---

## Strings That Stay As-Is

- **"Mapping"** (ValueTree type) – serialization identifier; do not change.
- **"Mappings"** (ValueTree child type) – serialization; do not change.
- **"Touchpad Mapping"** – correct term for a touchpad mapping config; keep.
- **"Mixer"** – TouchpadType::Mixer; keep.
- **"Mixers"** submenu – refers to mixer-type layouts; keep.
- **"Drum Pad + Mixer strip"** – describes combo; keep.
