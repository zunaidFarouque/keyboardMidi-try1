# UI Standardization Plan 03 – Testing and Regression

**Focus:** How to ensure that standardization (and later visual overhauls) do not break any feature or logic. Safe order of operations and what to run after each step.

---

## 1. Test Assets You Already Have

### 1.1 Unit tests (Core, no GUI)

- **ZonePropertiesLogicTests** – `setZonePropertyFromKey` for rootNote, baseVelocity, playMode, chordType, polyphonyMode, etc., and unknownKey/null.
- **MappingInspectorLogicTests** – `applyComboSelectionToMapping` for type, release behavior, ADSR target, command categories, sustain/layer/panic/transpose, keyboard group, etc.
- **MappingCompilerTests** – compilation, inheritance, touchpad layouts, zones, expression, etc.
- **MappingDefinitionTests** – schema content, visibility, rebuild flags.
- **ZoneTests** – ZoneDefinition schema, schema signature.
- **TouchpadTabTests** – TouchpadLayoutDefinition schema, MappingDefinition for touchpad.
- **UiStatePersistenceTests** – SettingsManager and UI state round-trip (tab index, selections, etc.).
- **Others** – ChordUtilities, ColourContrast, InputProcessor, MidiPerformanceMode, PitchPadUtilities, CrashLogger, etc.

All run via: `build\Debug\MIDIQy_Tests.exe` (or `build/Debug/MIDIQy_Tests.exe` on Unix). See [AI-build-and-test-guide.md](AI-build-and-test-guide.md).

### 1.2 What is not covered by unit tests

- **UI wiring:** That the correct control in ZonePropertiesPanel actually calls `ZonePropertiesLogic::setZonePropertyFromKey` with the right key and value (until we refactor the panel to do so).  
- **End-to-end:** Load preset → change zone property → save → load again → property is correct.  
- **Visual:** Layout, themes, LookAndFeel.

So: unit tests protect **Core and Logic**. After we move all apply logic to Core and have the panel call Logic, the same tests protect behavior; we then rely on a small manual smoke for “UI really calls Logic.”

---

## 2. Baseline and Run Commands

### 2.1 Record a baseline

Before starting any standardization:

1. Build: `cmake --build build --config Debug`  
2. Run tests: `build\Debug\MIDIQy_Tests.exe --gtest_brief=1`  
3. Note: total tests, any disabled, and that exit code is 0.  
4. (Optional) Run the app once, open a preset, change one zone and one mapping, save, reload, and confirm values.

### 2.2 After every refactor step

1. **Build:** `cmake --build build --config Debug`  
2. **Tests:** `build\Debug\MIDIQy_Tests.exe --gtest_brief=1`  
3. **Expect:** Exit code 0, same or higher number of passing tests (new tests may be added).  
4. If anything fails: fix before the next step; do not accumulate failing tests.

### 2.3 Optional: filter tests during development

- Run only Logic tests: `--gtest_filter=*Logic*`  
- Run only Zone-related: `--gtest_filter=*Zone*`  
- Run a single test: `--gtest_filter=ZonePropertiesLogicTest.SetRootNote`  

Always run the **full** suite before considering a step “done.”

---

## 3. What to Add When Extracting Logic

### 3.1 When adding to ZonePropertiesLogic

- **New key (e.g. zoneColor):** Add one or more tests in `ZonePropertiesLogicTests.cpp` that call `setZonePropertyFromKey` (or the new API) and assert the zone member.  
- **Existing keys:** Already covered; no new tests required, but keep them passing when you change the panel to call Logic.

### 3.2 When introducing TouchpadEditorLogic

- **New file:** `Source/Tests/TouchpadEditorLogicTests.cpp` (or similar).  
- **Layout config:** For each `applyLayoutConfigProperty` branch you implement, add a test: create a TouchpadLayoutConfig, call Logic with propertyId and value, assert config field.  
- **Mapping:** Same for mapping ValueTree where TouchpadEditorLogic applies properties; reuse existing MappingDefinition/ValueTree test patterns.  
- Add the test file to CMakeLists (MIDIQy_Tests target).

### 3.3 When adding aliasNameToHash (or DeviceManager helper)

- **New tests:** e.g. in a small `DeviceAliasTests.cpp` or inside an existing test file: empty string → 0, "Global" → 0, "Any / Master" → 0, "Unassigned" → 0, "Some Device" → non-zero, and that two same names give same hash.

---

## 4. Order of Operations (Safe Sequence)

Follow this so you don’t break behavior mid-way:

1. **Baseline** – Build, run full tests, note pass count and any failures.  
2. **aliasNameToHash to Core** – Add Core API + tests, then switch ZonePropertiesPanel to use it. Build + full tests.  
3. **ZonePropertiesLogic extended** – Add zoneColor (or any missing key) in Core + tests. Then refactor ZonePropertiesPanel to call `setZonePropertyFromKey` (and new API) for **all** controls; remove direct `zone->...` assignments. Build + full tests.  
4. **TouchpadEditorLogic** – Add namespace and tests for layout config first; switch TouchpadEditorPanel layout apply to Logic. Build + full tests. Then add mapping apply and tests; switch panel mapping apply to Logic. Build + full tests.  
5. **Optional: SettingsDefinition to Core, UI folder structure** – Build + full tests after each move.

At each step, if tests fail, fix before continuing. Do not leave the codebase with the panel still using duplicated logic and Logic out of sync.

---

## 5. Manual Smoke Checklist (optional but recommended)

Keep a short list and run it after a full standardization pass or before a release. Example:

- [ ] Open app; load a preset (or factory default).  
- [ ] **Zones:** Select a zone; change root note, play mode, one toggle; switch zone and back; save preset; reload preset; values are correct.  
- [ ] **Keyboard mappings:** Select a mapping; change type (Note/Expression/Command), one combo; save and reload; values correct.  
- [ ] **Touchpad:** Select a layout; change one layout property; select a touchpad mapping; change one mapping property; save and reload; values correct.  
- [ ] **Settings:** Change pitch bend range or one checkbox; restart app or reload settings; value persists.  
- [ ] No crashes, no asserts, log shows no unexpected errors.

You can put this in `Development/manual-smoke-ui-standardization.md` and tick it after each major refactor or before a visual overhaul.

---

## 6. Summary

- **Before starting:** Record baseline (build + full tests).  
- **After every step:** Build + full test suite; fix any failure before continuing.  
- **When adding Logic:** Add unit tests for new branches/keys.  
- **Order:** Small steps (one extraction at a time); aliasNameToHash → Zone panel → TouchpadEditorLogic (layout then mapping).  
- **Optional:** Manual smoke list for UI wiring and save/load.

This keeps features and logic intact while you standardize and later change the look and layout of the UI.
