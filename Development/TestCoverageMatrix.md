# Test Coverage Matrix

Living checklist for test coverage. When adding a new mapping/zone/setting option, add a row and ensure at least one test covers each relevant cell (Schema / Compile / Runtime). Run `ctest -C Debug` to confirm all tests pass.

**Convention:** Rows = UI option / feature. Columns = Schema | Compile | Runtime. Mark with X when at least one test covers that cell; add test name in parentheses where useful.

## Mappings

| Option | Schema | Compile | Runtime |
|--------|--------|---------|---------|
| Note: channel, data1, data2 | X (SchemaGeneration) | X (VerticalInheritance etc.) | X (NoteTypeTest.ChannelAndNoteNumberSentCorrectly) |
| Note: releaseBehaviour SendNoteOff | X | X (NoteReleaseBehaviorCompiles) | X (SendNoteOff_PressRelease) |
| Note: releaseBehaviour SustainUntilRetrigger | X | X | X |
| Note: releaseBehaviour Always Latch | X | X | X |
| Note: followTranspose on/off | X (KeyboardNoteHasFollowTranspose) | X (applyNoteOptionsFromMapping) | X (FollowTransposeAddsToNoteWhenEnabled, FollowTransposeIgnoredWhenDisabled) |
| Note: velRandom | X | X | X (VelocitySentCorrectly) |
| Note: enabled false | X (IsMappingEnabled*) | X (DisabledMappingNotCompiled) | X (DisabledMappingNotExecuted) |
| Touchpad Note: boolean events | X | X (TouchpadNoteReleaseBehaviorApplied) | X (TouchpadFinger1DownSendsNoteOn*) |
| Touchpad Note: continuous threshold/trigger | X (TouchpadContinuousNoteHasThresholdAndTrigger) | X (TouchpadContinuousEventCompiledAsContinuousToGate) | X (TouchpadContinuousToGate_ThresholdAndTriggerAbove_AffectsNoteOnOff) |
| Expression: target CC | X | X (ExpressionSimpleCcProducesFastPathAdsr) | (implied) |
| Expression: target PitchBend | X | X (ExpressionPitchBendCompilesCorrectly) | X (TouchpadPitchPadTest*) |
| Expression: target SmartScaleBend | X | X (SmartScaleBendLookupIsBuilt) | (implied) |
| Expression: custom ADSR (attack/decay/sustain/release) | X (ExpressionCCCustomEnvelopeShowsAdsrSliders) | X (ExpressionCustomEnvelopeReadsAdsr) | - |
| Expression: sendReleaseValue / releaseValue | X (CCCompactDependentLayout) | X (ExpressionValueWhenOnOffCompiled) | (implied) |
| Expression: touchpad pitch pad (Absolute/Relative) | X | X (TouchpadPitchPadConfigCompiledForPitchBend) | X (TouchpadPitchPadTest) |
| Command: Sustain (0,1,2) | X (SustainCommandHasCategoryAndStyle) | X | X (SustainToggle*, SustainInverse*) |
| Command: Layer Momentary/Toggle | X (LayerUnifiedUI) | X (LayerCommandsAreNotInherited) | X (LayerMomentarySwitching, LayerToggleSwitching) |
| Command: Latch Toggle + releaseLatchedOnToggleOff | X (LatchToggleHasReleaseLatchedControl) | X (LatchToggleReleaseLatchedCompiled) | X (LatchToggleReleaseLatchedOnToggleOff*) |
| Command: Panic (all/latched/chords) | X (PanicCommandHasPanicMode) | X (PanicCommandCompilesData2) | X (PanicAll*, PanicLatchedOnly*, PanicChords*) |
| Command: Transpose (modify, semitones) | X (TransposeCommandHasModeModifySemitones) | X (TransposeCommandCompilesModifyAndSemitones) | X (TransposeUp1*, TransposeSet*, etc.) |
| Command: Global Mode Up/Down | X (GlobalModeUpHasNoExtraControls) | X | X (GlobalModeUpIncreasesDegreeTranspose) |
| Disabled touchpad mapping | X | X (DisabledTouchpadMappingNotInContext) | (same as disabled mapping) |

## Zones

| Option | Schema | Compile | Runtime |
|--------|--------|---------|---------|
| Identity (name, layerID, midiChannel) | - | X (zone in compile) | - |
| rootNote, scaleName, chromaticOffset, degreeOffset | - | X (ZoneCompilesToChordPool) | - |
| useGlobalRoot / globalRootOctaveOffset | X (GlobalRootShowsOctaveOffset) | X (ZoneUseGlobalRoot_UsesGlobalRootWhenCompiling, ZoneEffectiveRoot_GetNotesForKeyUsesPassedRoot) | - |
| ignoreGlobalTranspose | X (serialization) | X (getNotesForKey) | - |
| baseVelocity, velocityRandom | X (serialization) | X | - |
| chordType, playMode, releaseBehavior | X (serialization) | X (chord pool) | - |
| layoutStrategy (Linear, Grid, Piano) | - | X (GridIntervalAffectsCache) | - |
| strum*, instrumentMode, voicing, addBass | X (serialization) | X (ChordUtilities*) | - |
| polyphonyMode, glideTimeMs, isAdaptiveGlide | X (serialization) | X | - |
| delayReleaseOn, overrideTimer | X (serialization) | - | X (OverrideTimer* in InputProcessorTests) |
| useGlobalScale | X (serialization) | X (SmartScaleBend uses global scale) | - |

## Settings

| Option | Compile | Runtime |
|--------|---------|---------|
| Pitch bend range | X (SettingsPitchBendRangeAffectsExpressionBend) | X (TouchpadPitchPadTest, PitchBendRangeAffectsSentPitchBend) |
| Studio mode | - | X (StudioModeOffIgnoresDeviceMappings, StudioModeOn_UsesDeviceSpecificMapping) |
| MIDI mode active | - | X (MidiModeOff_KeyEventsProduceNoMidi) |

## Layer inheritance

| Option | Compile | Runtime |
|--------|---------|---------|
| Solo, Passthru, Private | X (GridCompilerTest LayerInheritance*) | X (InputProcessorTest LayerInheritanceSolo*) |

## UI → Data (inspector apply logic)

| Control / virtual property | Test (ValueTree state after apply) |
|----------------------------|-------------------------------------|
| type (Note / Expression / Command) | MappingInspectorLogicTest.ApplyType_* |
| releaseBehavior | ApplyReleaseBehavior_* |
| adsrTarget (CC / PitchBend / SmartScaleBend) | ApplyAdsrTarget_* |
| pitchPadMode (Absolute / Relative) | ApplyPitchPadMode_* |
| commandCategory (Sustain / Panic / Transpose / Layer / data1 id) | ApplyCommandCategory_*, ApplyData1Command_* |
| sustainStyle (Hold / Toggle / Default on) | ApplySustainStyle_* |
| layerStyle (Hold to switch / Toggle layer) | ApplyLayerStyle_* |
| panicMode (all / latched only / chords) | ApplyPanicMode_* |
| transposeMode (Global / Local) | ApplyTransposeMode_* |
| transposeModify (0..4) | ApplyTransposeModify_* |
| Invalid mapping no-op | InvalidMapping_NoOp |

## ZoneDefinition schema visibility

| Zone state | Test (getSchema shows expected controls) |
|------------|-------------------------------------------|
| Poly + chord + guitar | ZoneDefinitionSchema_PolyChordGuitarShowsStrumControls |
| Guitar Rhythm | ZoneDefinitionSchema_GuitarRhythmShowsFretAnchor |
| Poly + chord + piano (voicing, magnet) | ZoneDefinitionSchema_PolyChordPianoShowsVoicingAndMagnet |
| Legato (glide) | ZoneDefinitionSchema_LegatoShowsGlideControls, LegatoAdaptiveShowsMaxGlideTime |
| Release Normal + chord (delay release) | ZoneDefinitionSchema_ReleaseNormalChordShowsDelayRelease |
| useGlobalRoot (octave offset) | ZoneDefinitionSchema_GlobalRootShowsOctaveOffset |
| Schema signature changes when visibility changes | ZoneDefinitionSchema_SchemaSignatureChangesWhenVisibilityChanges |

## Zone panel apply (UI → Zone member)

| propertyKey / behaviour | Test |
|------------------------|------|
| Slider: rootNote, baseVelocity, glideTimeMs, ghostVelocityScale, etc. | ZonePropertiesLogicTest.SetRootNote, SetBaseVelocity, SetGlideTimeMs, SetGhostVelocityScale |
| Combo: playMode, chordType, polyphonyMode, instrumentMode, layoutStrategy | ZonePropertiesLogicTest.SetPlayMode_*, SetChordType_Triad, SetPolyphonyMode_Legato, SetInstrumentMode_Guitar, SetLayoutStrategy_Piano |
| Toggle: useGlobalRoot, delayReleaseOn | ZonePropertiesLogicTest.SetUseGlobalRoot, SetDelayReleaseOn |
| Unknown key / null zone | ZonePropertiesLogicTest.UnknownKey_ReturnsFalse, NullZone_ReturnsFalse |
