## Test Review (“Testing the Tests”) Plan

This plan lets you review the new/changed tests in **focused batches**, so you can verify that each test is asserting the **right behaviour**, not just mirroring implementation quirks.

Mark items as you go (e.g. `[ ]` → `[x]` when you’re satisfied).

---

## How to use this document

- **For each batch**
  - **Step 1 – Read tests**: Open the listed test file(s) and skim the named tests.
  - **Step 2 – Read implementation**: Open the referenced production file(s) and find the logic those tests exercise.
  - **Step 3 – Compare intent vs. code**:
    - Ask “Is this *really* the behaviour I want to lock in?”
    - If the feature is incomplete or likely to change, consider loosening or rewriting the test.
  - **Step 4 – Run tests**: Optionally run only that test suite (e.g. via `ctest -R <pattern>` or your IDE) and confirm it passes.
  - **Step 5 – Annotate**: Add short notes in this file if you decide to keep, relax, or refactor any test.

You don’t need to follow the order strictly, but it’s arranged from **most data/behaviour‑critical** to more **UI‑wiring** details.

---

## Batch 1 – Mapping inspector apply logic (UI → ValueTree)

**Goal:** Confirm that the `MappingInspectorLogic` tests encode the *intended* mapping from inspector combos to `ValueTree` properties (including virtual props like `commandCategory` → `data1`, `panicMode` → `data1`/`data2`, etc.).

- **Files (tests)**
  - `Source/Tests/MappingInspectorLogicTests.cpp`
- **Files (implementation)**
  - `Source/MappingInspectorLogic.cpp`
  - `Source/MappingInspector.cpp` (ComboBox setup / `onChange`)
  - `Source/MappingDefinition.cpp` (schema, option IDs)

- **Checklist**
  - [x] **Type / adsrTarget / pitchPadMode**
    - Tests: `ApplyType_*`, `ApplyAdsrTarget_*`, `ApplyPitchPadMode_*`
    - Verify that using option text (`\"Note\"`, `\"Expression\"`, `\"PitchBend\"`, `\"SmartScaleBend\"`, `\"Absolute\"`, `\"Relative\"`) matches how the rest of the system expects these properties.
  - [ ] **Command virtual props**
    - Tests: `ApplyCommandCategory_*`, `ApplyData1Command_*`, `ApplySustainStyle_*`, `ApplyLayerStyle_*`, `ApplyPanicMode_*`
    - Confirm that:
      - `commandCategory` → correct `data1` encodings (Sustain/Layers/Panic/Transpose).
      - `panicMode` → `data1 == 4` and `data2` (0/1/2) is what runtime expects.
      - `sustainStyle` / `layerStyle` combos map to the right command IDs.
  - [ ] **Transpose virtual props**
    - Tests: `ApplyTransposeMode_*`, `ApplyTransposeModify_*`
    - Confirm that `\"Global\"`/`\"Local\"` strings and modify `0..4` are what GridCompiler/InputProcessor expect.
  - [ ] **Release behaviour + pitch‑pad CC**
    - Tests: `ApplyReleaseBehavior_*`
    - Make sure the string values align with MappingDefinition’s schema and the runtime release behaviour tests.
  - [ ] **Safety / edge cases**
    - Tests: `InvalidMapping_NoOp`
    - Decide whether “silent no‑op on invalid mapping” is desired, or if you’d prefer assertions/logging in the future.

Use this batch to decide if any inspector option semantics should change before they are “locked in” by tests.

---

## Batch 2 – Touchpad & settings runtime behaviour

**Goal:** Confirm runtime behaviour for touchpad mappings and global settings (MIDI mode, studio mode, pitch bend range) is what you want.

- **Files (tests)**
  - `Source/Tests/InputProcessorTests.cpp`
    - Existing touchpad tests:
      - `TouchpadFinger1DownSendsNoteOnThenNoteOff`
      - `TouchpadFinger1DownSustainUntilRetrigger_NoNoteOffOnRelease`
      - `TouchpadSustainUntilRetrigger_ReTrigger_NoNoteOffBeforeSecondNoteOn`
      - `TouchpadFinger1UpTriggersNoteOnOnly`
    - New/updated:
      - `MidiModeOff_KeyEventsProduceNoMidi`
      - `StudioModeOffIgnoresDeviceMappings`
      - `StudioModeOn_UsesDeviceSpecificMapping`
      - `TouchpadContinuousToGate_ThresholdAndTriggerAbove_AffectsNoteOnOff`
      - `PitchBendRangeAffectsSentPitchBend`
      - All `TouchpadPitchPadTest.*` (pitch‑pad range/behaviour)
- **Files (implementation)**
  - `Source/InputProcessor.cpp` (studio mode, MIDI mode, touchpad paths)
  - `Source/SettingsManager.cpp` / `.h`
  - `Source/GridCompiler.cpp` (touchpad compile params)
  - `Source/VoiceManager.cpp` (pitch bend send)

- **Checklist**
  - [ ] **MIDI mode OFF semantics**
    - Test: `MidiModeOff_KeyEventsProduceNoMidi`
    - Confirm that *all* key events are ignored when MIDI mode is inactive, and that this matches UX expectations.
  - [ ] **Studio mode ON/OFF device routing**
    - Tests: `StudioModeOffIgnoresDeviceMappings`, `StudioModeOn_UsesDeviceSpecificMapping`
    - Decide whether:
      - Studio mode OFF should really force global device `0` (ignoring device grids).
      - Studio mode ON should always prefer device‑specific grids when present.
  - [ ] **Touchpad continuous‑to‑gate threshold**
    - Test: `TouchpadContinuousToGate_ThresholdAndTriggerAbove_AffectsNoteOnOff`
    - Confirm that `>= threshold` vs `< threshold` semantics are correct and stable (check `touchpadThreshold` and `touchpadTriggerAbove` use).
  - [ ] **Pitch bend range influence**
    - Test: `PitchBendRangeAffectsSentPitchBend` plus the `TouchpadPitchPadTest.*` cases.
    - Ensure that:
      - Changing pitch bend range in settings produces the expected PB values.
      - The “±range semitones” math matches your mental model for both Absolute and Relative modes.

If any of these behaviours are provisional (e.g. studio mode semantics), note it in the test as a comment or TODO.

---

## Batch 3 – ZoneDefinition schema visibility

**Goal:** Confirm that `ZoneDefinition::getSchema` exposes exactly the controls you want for each “zone mode” and that tests don’t over‑specify internal layout.

- **Files (tests)**
  - `Source/Tests/ZoneTests.cpp` – new schema tests:
    - `ZoneDefinitionSchema_PolyChordGuitarShowsStrumControls`
    - `ZoneDefinitionSchema_GuitarRhythmShowsFretAnchor`
    - `ZoneDefinitionSchema_PolyChordPianoShowsVoicingAndMagnet`
    - `ZoneDefinitionSchema_LegatoShowsGlideControls`
    - `ZoneDefinitionSchema_LegatoAdaptiveShowsMaxGlideTime`
    - `ZoneDefinitionSchema_ReleaseNormalChordShowsDelayRelease`
    - `ZoneDefinitionSchema_GlobalRootShowsOctaveOffset`
    - `ZoneDefinitionSchema_SchemaSignatureChangesWhenVisibilityChanges`
- **Files (implementation)**
  - `Source/ZoneDefinition.cpp` / `.h`
  - `Source/Zone.h`

- **Checklist**
  - [ ] For each schema test, check:
    - The **zone state** set up in the test (chord type, instrumentMode, playMode, polyphonyMode, etc.).
    - The **expected controls** (e.g. `strumSpeedMs`, `guitarFretAnchor`, `voicingMagnetSemitones`, `glideTimeMs`, `releaseDurationMs`, `globalRootOctaveOffset`).
    - Whether you’re comfortable treating the *presence/absence* of those specific controls as part of the public contract.
  - [ ] `getSchemaSignature` test:
    - Decide if the signature is allowed to change for cosmetic reasons, or should only change when visibility‑relevant properties change (the test currently enforces the latter).

This batch is where you can relax tests if you expect to tweak the Zones UI more aggressively.

---

## Batch 4 – Zone behaviour & global root / cache

**Goal:** Confirm that zone compile/runtime behaviour regarding global root and cache usage is correct.

- **Files (tests)**
  - `Source/Tests/GridCompilerTests.cpp`
    - `ZoneUseGlobalRoot_UsesGlobalRootWhenCompiling`
  - `Source/Tests/ZoneTests.cpp`
    - `ZoneEffectiveRoot_GetNotesForKeyUsesPassedRoot`
- **Files (implementation)**
  - `Source/Zone.cpp` / `.h`
  - `Source/GridCompiler.cpp`
  - `Source/ZoneManager.cpp`

- **Checklist**
  - [ ] Verify that `useGlobalRoot` and `globalRootOctaveOffset` behaviour in tests matches how you conceptually want global + local roots to combine.
  - [ ] Ensure `rebuildCache` + `getNotesForKey` semantics in tests match your expectation:
    - Effective root is always passed explicitly.
    - Changing global root vs. zone root behaves as you’d expect.

If you plan to evolve the global root/scale story, mark any tests that might become brittle.

---

## Batch 5 – Zone properties apply (UI → Zone)

**Goal:** Confirm that `ZonePropertiesLogic` encodes the intended mapping between schema `propertyKey` and `Zone` members, independent of the UI.

- **Files (tests)**
  - `Source/Tests/ZonePropertiesLogicTests.cpp`
- **Files (implementation)**
  - `Source/ZonePropertiesLogic.cpp`
  - `Source/ZonePropertiesPanel.cpp` (current UI wiring)
  - `Source/Zone.h`

- **Checklist**
  - [ ] **Slider mappings**
    - Tests: `SetRootNote`, `SetBaseVelocity`, `SetGlideTimeMs`, `SetGhostVelocityScale`, etc.
    - Confirm that types and ranges (int vs float) match your semantics.
  - [ ] **Combo mappings**
    - Tests: `SetPlayMode_*`, `SetChordType_Triad`, `SetPolyphonyMode_Legato`, `SetInstrumentMode_Guitar`, `SetLayoutStrategy_Piano`.
    - Ensure `id → enum` mapping matches ZoneDefinition + UI expectations.
  - [ ] **Toggle mappings**
    - Tests: `SetUseGlobalRoot`, `SetDelayReleaseOn`, and others inferred in implementation.
  - [ ] **Failure cases**
    - Tests: `UnknownKey_ReturnsFalse`, `NullZone_ReturnsFalse`.
    - Confirm you want silent `false` rather than assertions for bad keys.

Optional follow‑up (separate task): refactor `ZonePropertiesPanel` to call `ZonePropertiesLogic::setZonePropertyFromKey` from its lambdas so UI and tests share a single source of truth.

---

## Batch 6 – Settings & compile‑time behaviour

**Goal:** Ensure settings that affect compile behaviour are explicitly and correctly tested.

- **Files (tests)**
  - `Source/Tests/GridCompilerTests.cpp`
    - `SettingsPitchBendRangeAffectsExpressionBend`
  - `Source/Tests/MappingDefinitionTests.cpp`
    - Touchpad schema / release options tests
- **Files (implementation)**
  - `Source/GridCompiler.cpp`
  - `Source/SettingsManager.cpp`
  - `Source/MappingDefinition.cpp`

- **Checklist**
  - [ ] Pitch bend range compile‑time test:
    - Confirm the compiled data (e.g. data2 / PB scaling) truly matches what your runtime expects for that range.
  - [ ] Touchpad schema tests:
    - Ensure that schema options (e.g. “no Send Note Off for Finger1Up”) are intentional UX decisions.

---

## Batch 7 – Sanity & regression hygiene

**Goal:** Quick pass to ensure naming and grouping remain helpful as the test suite grows.

- **Files**
  - All `Source/Tests/*.cpp` (especially new files)

- **Checklist**
  - [ ] Test names are **feature‑oriented** (not overly tied to internal details).
  - [ ] Related tests are grouped logically within each file.
  - [ ] Any “workaround” or “temporary” behaviour is clearly commented in the test.
  - [ ] `Development/TestCoverageMatrix.md` still accurately summarizes coverage after any further tweaks.

Use this batch periodically as you evolve the codebase, not just once.

