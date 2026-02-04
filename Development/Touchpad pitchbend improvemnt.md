---
name: touchpad-pitchbend-resting-space
overview: Implement precise touchpad pitch-bend mapping with resting spaces and a visualizer, for PitchBend and SmartScaleBend targets on touchpad axis Expression mappings.
todos:
  - id: inspect-current-pitchpad
    content: Inspect current Expression + touchpad PB/SmartScale mapping and existing range/steps options
    status: in_progress
  - id: design-pitchpad-model
    content: Design shared PitchPadConfig + PitchPadLayout and X→step mapping helpers
    status: pending
  - id: extend-schema
    content: Extend MappingDefinition/Inspector for touchpad Expression PB/SmartScale (mode, start, step range, restingSpace)
    status: pending
  - id: compile-config
    content: Compile per-mapping PitchPadConfig into GridCompiler touchpad entries
    status: pending
  - id: runtime-mapping
    content: Integrate new pitch-pad mapping into InputProcessor touchpad ContinuousToRange for PB/SmartScale
    status: pending
  - id: visualizer-ui
    content: Add touchpad pitch layout visualizer band/transition rendering and current X marker
    status: pending
  - id: tests
    content: Add GridCompiler and InputProcessor tests for pitch-pad configs and mapping correctness
    status: pending
isProject: false
---

### Goals

- **Touchpad-only**: Apply new pitch-bend behavior only to touchpad axis Expression mappings (e.g. `Finger1X`, `Finger1Y`) with `Target = PitchBend` or `SmartScaleBend`.
- **Accurate pitch mapping**: Use a clear, shared mapping from touchpad X coordinate to discrete pitch bend steps, honoring:
  - Mode: **Absolute** vs **Relative**.
  - Starting position: **Left / Center / Right / Custom slider**.
  - Pitch bend range in steps (semitones for PitchBend, scale steps for SmartScaleBend).
  - **Resting space %** per pitch (flat areas) plus **transition regions** in between.
- **Visualizer**: Show a rectangular touchpad preview with colored resting bands and transition bands, plus a marker at the current touch X.
- **No CC spam**: Keep existing change-only sending for continuous CC/PB, reusing any new mapping helpers.

---

### 1. Understand and confirm current behavior

- Inspect the existing Expression + touchpad pipeline:
  - Mapping schema for Expression in `[Source/MappingDefinition.cpp](Source/MappingDefinition.cpp)`
    - How `adsrTarget = PitchBend` vs `SmartScaleBend` is configured.
    - Existing touchpad range/mapping controls: `touchpadInputMin`, `touchpadInputMax`, `touchpadOutputMin`, `touchpadOutputMax`.
  - Compilation to runtime structs in `[Source/GridCompiler.cpp](Source/GridCompiler.cpp)`
    - How Expression actions with PB / SmartScaleBend are compiled.
    - How `TouchpadConversionParams` is filled for `TouchpadConversionKind::ContinuousToRange`.
  - Runtime mapping in `[Source/InputProcessor.cpp](Source/InputProcessor.cpp)`
    - `processTouchpadContacts` and the `ContinuousToRange` branch (now CC/PB send with change-only guard).
  - Pitch bend math and SmartScale behavior:
    - `[Source/ExpressionEngine.cpp](Source/ExpressionEngine.cpp)` for envelopes and PB/SmartScale output.
    - `[Source/VoiceManager.cpp](Source/VoiceManager.cpp)` `rebuildPbLookup()` and any SmartScale-related logic.
  - Visualizer touchpad display (if any) in `[Source/VisualizerComponent.cpp](Source/VisualizerComponent.cpp)`.
- Verify there are currently no per-mapping settings for:
  - Absolute vs Relative pitch-bend mode.
  - Starting position (left/center/right/custom).
  - Resting space % per pitch.

---

### 2. Design shared pitch-mapping model

Define a **single, reusable mapping model** that both MIDI generation and the visualizer can call.

- Introduce a small struct (in `MappingTypes.h` or a dedicated header) for touchpad pitch mapping, e.g.:
  - `enum class PitchPadMode { Absolute, Relative };`
  - `enum class PitchPadStart { Left, Center, Right, Custom };`
  - `struct PitchPadConfig { PitchPadMode mode; PitchPadStart start; float customStartX; int minStep; int maxStep; float restingSpacePercent; };`
  - `struct PitchPadLayout { struct Band { float xStart, xEnd; int step; bool isRest; }; std::vector<Band> bands; };`
- Define a **pure function** that generates the layout from the config and global PB range:
  - Input: `PitchPadConfig`, global PB range in steps (semitones or scale steps), and direction (left → right).
  - Output: `PitchPadLayout` containing alternating **resting bands** and **transition bands** as per your example:
    - For range = ±2: pitches `[-2,-1,0,+1,+2]`.
    - Each pitch gets a resting band covering `restingSpacePercent` of the total width.
    - Remaining width is split evenly into `stepsCount-1` transition bands.
  - Guarantee bands are contiguous and cover `[0,1]` on X.
- Add a helper to map an X coordinate to a **target step** and any fractional position:
  - `struct PitchSample { int step; bool inRestingBand; };`
  - `PitchSample mapXToStep(const PitchPadLayout&, float x)`, where:
    - Inside a resting band: `step = band.step`, `inRestingBand=true`.
    - Inside a transition band: choose a suitable behavior:
      - Either continuous interpolation between adjacent steps (for smooth glides),
      - Or snap to the nearest step but mark `inRestingBand=false` so the visualizer shows transition.
- Clarify semantics for **SmartScaleBend**:
  - Treat `step` as **scale step offset**, later converted to PB via existing SmartScale lookup.
  - For PitchBend, treat `step` as **semitones offset**.

---

### 3. Extend mapping schema (config panel)

For Expression mappings where `inputAlias == "Touchpad"` **and** the event is continuous (`Finger1X`, `Finger1Y`, etc.) **and** the target is **PitchBend** or **SmartScaleBend**, extend the schema in `[Source/MappingDefinition.cpp](Source/MappingDefinition.cpp)`:

- **Mode**: `pitchPadMode` ComboBox
  - Options: `Absolute`, `Relative`.
- **Starting position**: `pitchPadStart` ComboBox
  - Options: `Left`, `Center`, `Right`, `Custom`.
- **Custom start**: `pitchPadCustomStart` Slider
  - Range `[0.0, 1.0]`, visible/enabled only when `pitchPadStart == Custom`.
- **Pitch range in steps**:
  - For PitchBend: sliders `pitchPadMinSemitone`, `pitchPadMaxSemitone` (integers, e.g. -2..+2).
  - For SmartScaleBend: sliders `pitchPadMinStep`, `pitchPadMaxStep` (integers, e.g. -2..+2) or reuse same property names but documented as scale steps.
  - These define the discrete steps shown in the layout (min..max inclusive).
- **Resting space %**: `pitchPadRestingPercent`
  - Slider 0–50 (or 0–40) representing **percentage of total width** reserved per pitch’s resting band.
  - Default e.g. 10%.
- Keep existing **global PB range** from `SettingsManager` as the mapping from steps → MIDI PB, not per-mapping.
- Update `[Source/MappingInspector.cpp](Source/MappingInspector.cpp)` and `[Source/MappingEditorComponent.cpp](Source/MappingEditorComponent.cpp)` to:
  - Show/hide these controls based on mapping type (touchpad, Expression, PB/SmartScale).
  - Provide reasonable defaults when switching target or mode.

---

### 4. Compile pitch-pad settings into runtime context

In `[Source/GridCompiler.cpp](Source/GridCompiler.cpp)`:

- When compiling a **touchpad Expression** mapping:
  - Detect PB / SmartScaleBend continuous events (`ContinuousToRange`).
  - Read per-mapping properties:
    - `pitchPadMode`, `pitchPadStart`, `pitchPadCustomStart`.
    - `pitchPadMinSemitone` / `pitchPadMaxSemitone` (PitchBend) or step equivalents.
    - `pitchPadRestingPercent`.
  - Construct a `PitchPadConfig` and store it somewhere reachable at runtime, e.g.:
    - Extend `TouchpadConversionParams` with an optional `PitchPadConfig`.
    - Or add a separate `PitchPadConfig` field on `TouchpadMappingEntry`.
  - For SmartScaleBend, keep using `buildSmartBendLookup` and global PB range, but defer from this config only the **step range and resting layout**.
- Ensure keyboard Expression mappings remain unaffected (touchpad-only per your answer).

---

### 5. Implement runtime mapping for MIDI

In `[Source/InputProcessor.cpp](Source/InputProcessor.cpp)`, inside `processTouchpadContacts` for `ContinuousToRange`:

- For PB / SmartScaleBend continuous mappings:
  - Instead of mapping `continuousVal` directly to a linear step or CC, call the shared mapping helper:
    - Convert touchpad X (or Y) to normalized `[0,1]` based on `touchpadInputMin/Max`.
    - For **Absolute** mode:
      - Compute a global X along the full `[0,1]` width, offset by start position:
        - Left start: 0.
        - Center: 0.5.
        - Right: 1.0.
        - Custom: `pitchPadCustomStart`.
      - Use this X to get `step` from `PitchPadLayout`.
    - For **Relative** mode:
      - On finger **down**, record `startX` for this `(device, layer, eventId)`.
      - On each move, compute relative displacement `dx = (currentX - startX)` mapped into the same `[0,1]` width used to place steps.
      - Apply the same `mapXToStep` on a virtual coordinate that advances with dx.
  - Convert `step` to actual **PB value**:
    - For PitchBend: semitone offset → `pbVal` via the same semitone→PB function used elsewhere.
    - For SmartScaleBend: use the existing SmartScale lookup with `step` offset (or a helper that maps (note, step) → PB).
  - Continue to use the **change-only** guard already in place so we don’t spam identical PB values.

---

### 6. Integrate with the visualizer

In `[Source/VisualizerComponent.cpp](Source/VisualizerComponent.cpp)` (or wherever touchpad info is drawn):

- Add a **touchpad layout preview** below the touch coordinates:
  - A rectangle representing the touchpad’s X range.
  - Use the same `PitchPadLayout` helper as runtime to compute bands.
  - Render:
    - **Resting bands** (one for each pitch step) as colored blocks (e.g. alternating colors).
    - **Transition bands** as different fill (e.g. white or thinner lines).
  - Draw tick labels (e.g. `PB-2`, `PB-1`, `PB0`, `PB+1`, `PB+2` or step numbers) aligned to each resting band’s center.
  - Draw a small **“x” marker** at the current finger X position (using current touchpad contact info already tracked elsewhere).
- Ensure the visualizer is using **exactly the same `PitchPadConfig` and global range** as MIDI mapping so what you see matches what you hear.
- If multiple touchpad mappings exist, either:
  - Show the layout for the active layer/mapping associated with the current touchpad alias, or
  - Show an aggregated or selected mapping (start with the simplest: the first active PB mapping for that device and layer).

---

### 7. Tests

Add targeted tests in `[Source/Tests/InputProcessorTests.cpp](Source/Tests/InputProcessorTests.cpp)` and `[Source/Tests/GridCompilerTests.cpp](Source/Tests/GridCompilerTests.cpp)`:

- **GridCompiler** tests:
  - Confirm that touchpad Expression PB/SmartScale mappings compile `PitchPadConfig` correctly:
    - Correct mode, start position, step range, resting percent.
  - Ensure CC mappings are unchanged.
- **InputProcessor** tests:
  - Simulate touchpad continuous PB mappings with deterministic contacts:
    - Absolute mode, center start, range -2..+2, given `restingSpacePercent`, verify that specific X positions produce exact steps (e.g. PB-2, PB-1, PB0, PB+1, PB+2). Both small and large swipes.
    - Relative mode: start at some X, move by fixed deltas, verify same displacement always gives same PB semitone regardless of absolute position.
  - Verify change-only behavior: repeated frames with identical X do **not** produce additional PB events.
- (Optional) **Visualizer** tests (if feasible):
  - Use a small helper test to validate `PitchPadLayout` band boundaries and total coverage of `[0,1]`.

---

### 8. Non-goals for this phase

- **Smoothing slider** for PB noise/jitter: per your choice, this will be tackled in a follow-up phase.
- Keyboard Expression mappings: behavior remains as-is; all new logic is scoped to touchpad axis Expression with PB/SmartScale.

This plan keeps the pitch-bend math and layout logic in a single, shared place and wires it into both the audio path and the visualizer, so fixes/improvements automatically stay in sync across MIDI generation and UI.