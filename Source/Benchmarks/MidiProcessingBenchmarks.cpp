// MIDI Processing Performance Benchmarks
// Uses Google Benchmark to measure latency and throughput of various MIDI paths

#include "../GridCompiler.h"
#include "../TouchpadTypes.h"
#include "BenchmarkFixtures.h"

// =============================================================================
// Category 1: Manual Mapping Tests
// =============================================================================

// Single note press + release cycle
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, ManualNote_SingleKey)
(benchmark::State &state) {
  addNoteMapping(0, 81, 60, 100, 1); // Key Q -> C4
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);  // Key down
    proc.processEvent(input, false); // Key up
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, ManualNote_SingleKey)
    ->Unit(benchmark::kMicrosecond);

// Rapid-fire 100 key presses (throughput test)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, ManualNote_RapidFire)
(benchmark::State &state) {
  // Map 10 different keys
  for (int i = 0; i < 10; ++i) {
    addNoteMapping(0, 81 + i, 60 + i, 100, 1);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    for (int i = 0; i < 100; ++i) {
      InputID input{0, 81 + (i % 10)};
      proc.processEvent(input, true);
      proc.processEvent(input, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, ManualNote_RapidFire)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 2: Expression/ADSR Tests
// =============================================================================

// CC with fast path (no ADSR envelope)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Expression_CC_NoADSR)
(benchmark::State &state) {
  addExpressionCCMapping(0, 81, 1, 1, false); // CC1 modwheel, no envelope
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Expression_CC_NoADSR)
    ->Unit(benchmark::kMicrosecond);

// CC with full ADSR envelope
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Expression_CC_WithADSR)
(benchmark::State &state) {
  addExpressionCCMapping(0, 81, 1, 1, true, 50, 100, 80,
                         200); // ADSR: 50/100/80/200ms
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Expression_CC_WithADSR)
    ->Unit(benchmark::kMicrosecond);

// PitchBend with fast path (no envelope)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Expression_PitchBend_NoADSR)
(benchmark::State &state) {
  addExpressionPBMapping(0, 81, 1, false);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Expression_PitchBend_NoADSR)
    ->Unit(benchmark::kMicrosecond);

// PitchBend with full ADSR envelope
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Expression_PitchBend_WithADSR)
(benchmark::State &state) {
  addExpressionPBMapping(0, 81, 1, true, 50, 100, 80, 200);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Expression_PitchBend_WithADSR)
    ->Unit(benchmark::kMicrosecond);

// Single ADSR timer callback with N active envelopes (cost of one tick)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Expression_ADSRTimerTick)
(benchmark::State &state) {
  const int N = 20;
  for (int i = 0; i < N; ++i) {
    addExpressionCCMapping(0, 20 + i, 1 + i, 1, true, 50, 100, 80, 200);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();
  // Trigger all N envelopes (key down)
  for (int i = 0; i < N; ++i) {
    proc.processEvent(InputID{0, 20 + i}, true);
  }

  for (auto _ : state) {
    proc.runExpressionEngineOneTick();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Expression_ADSRTimerTick)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 3: Layer/Command Tests
// =============================================================================

// Event processing with only Layer 0 active
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Layer_SingleActive)
(benchmark::State &state) {
  addNoteMapping(0, 81, 60, 100, 1);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Layer_SingleActive)
    ->Unit(benchmark::kMicrosecond);

// Event processing with all 9 layers active (worst case layer search)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Layer_AllActive)
(benchmark::State &state) {
  // Activate all layers by adding toggle commands and toggling them on
  for (int layer = 1; layer < 9; ++layer) {
    auto layerNode = presetMgr.getLayerNode(layer);
    if (layerNode.isValid()) {
      layerNode.setProperty("isActive", true, nullptr);
    }
  }
  // Put the actual note mapping on layer 8 (worst case: must search all layers)
  addNoteMapping(8, 81, 60, 100, 1);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Layer_AllActive)
    ->Unit(benchmark::kMicrosecond);

// Layer momentary press + release cycle
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Layer_MomentarySwitch)
(benchmark::State &state) {
  addCommandMapping(0, 81, static_cast<int>(MIDIQy::CommandID::LayerMomentary),
                    1);
  proc.forceRebuildMappings();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);  // Activate layer
    proc.processEvent(input, false); // Deactivate layer
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Layer_MomentarySwitch)
    ->Unit(benchmark::kMicrosecond);

// Layer toggle command
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Layer_Toggle)
(benchmark::State &state) {
  addCommandMapping(0, 81, static_cast<int>(MIDIQy::CommandID::LayerToggle), 1);
  proc.forceRebuildMappings();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true); // Toggle on
    proc.processEvent(input, false);
    proc.processEvent(input, true); // Toggle off
    proc.processEvent(input, false);
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Layer_Toggle)
    ->Unit(benchmark::kMicrosecond);

// Transpose command processing
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Command_Transpose)
(benchmark::State &state) {
  addCommandMapping(0, 81, static_cast<int>(MIDIQy::CommandID::Transpose), 0);
  proc.forceRebuildMappings();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Command_Transpose)
    ->Unit(benchmark::kMicrosecond);

// Panic command (all notes off)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Command_Panic)
(benchmark::State &state) {
  // First add some notes to make panic do real work
  for (int i = 0; i < 5; ++i) {
    addNoteMapping(0, 70 + i, 60 + i, 100, 1);
  }
  addCommandMapping(0, 81, static_cast<int>(MIDIQy::CommandID::Panic), 0);
  proc.forceRebuildMappings();
  mockMidi.clear();

  // Pre-play some notes so panic has work to do
  for (int i = 0; i < 5; ++i) {
    proc.processEvent(InputID{0, 70 + i}, true);
  }
  mockMidi.clear();

  InputID panicKey{0, 81};
  for (auto _ : state) {
    proc.processEvent(panicKey, true);
    proc.processEvent(panicKey, false);
    // Re-play notes for next iteration
    for (int i = 0; i < 5; ++i) {
      proc.processEvent(InputID{0, 70 + i}, true);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Command_Panic)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 4: Zone Tests (No Chords - Single Notes)
// =============================================================================

// Zone single note, Poly mode
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_SingleNote_Poly)
(benchmark::State &state) {
  auto zone = createZone("PolyZone", 0, {81, 87, 69},
                         ChordUtilities::ChordType::None, PolyphonyMode::Poly);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_SingleNote_Poly)
    ->Unit(benchmark::kMicrosecond);

// Zone single note, Mono mode (stack operations)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_SingleNote_Mono)
(benchmark::State &state) {
  auto zone = createZone("MonoZone", 0, {81, 87, 69},
                         ChordUtilities::ChordType::None, PolyphonyMode::Mono);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_SingleNote_Mono)
    ->Unit(benchmark::kMicrosecond);

// Zone single note, Legato mode (portamento calculations)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_SingleNote_Legato)
(benchmark::State &state) {
  auto zone =
      createZone("LegatoZone", 0, {81, 87, 69}, ChordUtilities::ChordType::None,
                 PolyphonyMode::Legato);
  zone->glideTimeMs = 50;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_SingleNote_Legato)
    ->Unit(benchmark::kMicrosecond);

// 100 rapid zone notes in Poly mode
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_RapidNotes_Poly)
(benchmark::State &state) {
  std::vector<int> keyCodes;
  for (int i = 0; i < 10; ++i) {
    keyCodes.push_back(81 + i);
  }
  auto zone = createZone("RapidPolyZone", 0, keyCodes,
                         ChordUtilities::ChordType::None, PolyphonyMode::Poly);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    for (int i = 0; i < 100; ++i) {
      InputID input{0, 81 + (i % 10)};
      proc.processEvent(input, true);
      proc.processEvent(input, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_RapidNotes_Poly)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 5: Zone Chord Tests
// =============================================================================

// Triad chord, Block voicing
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Triad_Block)
(benchmark::State &state) {
  auto zone = createPianoZone("BlockTriad", 0, {81, 87, 69},
                              ChordUtilities::ChordType::Triad,
                              Zone::PianoVoicingStyle::Block, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Triad_Block)
    ->Unit(benchmark::kMicrosecond);

// Triad chord, Close voicing (gravity well algorithm)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Triad_Close)
(benchmark::State &state) {
  auto zone = createPianoZone("CloseTriad", 0, {81, 87, 69},
                              ChordUtilities::ChordType::Triad,
                              Zone::PianoVoicingStyle::Close, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Triad_Close)
    ->Unit(benchmark::kMicrosecond);

// Triad chord, Open voicing
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Triad_Open)
(benchmark::State &state) {
  auto zone = createPianoZone("OpenTriad", 0, {81, 87, 69},
                              ChordUtilities::ChordType::Triad,
                              Zone::PianoVoicingStyle::Open, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Triad_Open)
    ->Unit(benchmark::kMicrosecond);

// 7th chord, Block voicing
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Seventh_Block)
(benchmark::State &state) {
  auto zone = createPianoZone("BlockSeventh", 0, {81, 87, 69},
                              ChordUtilities::ChordType::Seventh,
                              Zone::PianoVoicingStyle::Block, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Seventh_Block)
    ->Unit(benchmark::kMicrosecond);

// 7th chord, Close voicing (alternating grip algorithm)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Seventh_Close)
(benchmark::State &state) {
  auto zone = createPianoZone("CloseSeventh", 0, {81, 87, 69},
                              ChordUtilities::ChordType::Seventh,
                              Zone::PianoVoicingStyle::Close, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Seventh_Close)
    ->Unit(benchmark::kMicrosecond);

// Chord with voicing magnet applied
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Chord_WithMagnet)
(benchmark::State &state) {
  auto zone = createPianoZone(
      "MagnetChord", 0, {81, 87, 69}, ChordUtilities::ChordType::Triad,
      Zone::PianoVoicingStyle::Close, false, 3); // +3 magnet
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Chord_WithMagnet)
    ->Unit(benchmark::kMicrosecond);

// Chord with bass note added
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Chord_WithBass)
(benchmark::State &state) {
  auto zone = createPianoZone(
      "BassChord", 0, {81, 87, 69}, ChordUtilities::ChordType::Triad,
      Zone::PianoVoicingStyle::Close, true, 0); // Add bass
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Chord_WithBass)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 6: Zone Release Mode Tests
// =============================================================================

// Normal release, instant note-off
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Release_Normal_Instant)
(benchmark::State &state) {
  auto zone = createZone("NormalInstant", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly,
                         Zone::ReleaseBehavior::Normal);
  zone->delayReleaseOn = false;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Release_Normal_Instant)
    ->Unit(benchmark::kMicrosecond);

// Normal release with delay timer setup
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Release_Normal_Delayed)
(benchmark::State &state) {
  auto zone = createZone("NormalDelayed", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly,
                         Zone::ReleaseBehavior::Normal);
  zone->delayReleaseOn = true;
  zone->releaseDurationMs = 500;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Release_Normal_Delayed)
    ->Unit(benchmark::kMicrosecond);

// Delayed release with override timer cancel
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Release_Delayed_Override)
(benchmark::State &state) {
  auto zone = createZone("DelayedOverride", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly,
                         Zone::ReleaseBehavior::Normal);
  zone->delayReleaseOn = true;
  zone->releaseDurationMs = 500;
  zone->overrideTimer = true;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input1{0, 81};
  InputID input2{0, 87};
  for (auto _ : state) {
    proc.processEvent(input1, true);
    proc.processEvent(input1, false); // Start timer
    proc.processEvent(input2, true);  // Override timer
    proc.processEvent(input2, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Release_Delayed_Override)
    ->Unit(benchmark::kMicrosecond);

// Sustain mode (one-shot latch)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Release_Sustain)
(benchmark::State &state) {
  auto zone = createZone("SustainMode", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly,
                         Zone::ReleaseBehavior::Sustain);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false); // Notes sustain
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Release_Sustain)
    ->Unit(benchmark::kMicrosecond);

// Sustain mode, next chord cancels previous
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Zone_Release_Sustain_NextChord)
(benchmark::State &state) {
  auto zone = createZone("SustainNextChord", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly,
                         Zone::ReleaseBehavior::Sustain);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input1{0, 81};
  InputID input2{0, 87};
  for (auto _ : state) {
    proc.processEvent(input1, true);
    proc.processEvent(input1, false); // Notes sustain
    proc.processEvent(input2, true);  // Cancels previous, plays new
    proc.processEvent(input2, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Zone_Release_Sustain_NextChord)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 7: Stress Tests
// =============================================================================

// 10 simultaneous chord triggers
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_PolyChords_10Keys)
(benchmark::State &state) {
  std::vector<int> keyCodes;
  for (int i = 0; i < 10; ++i) {
    keyCodes.push_back(81 + i);
  }
  auto zone = createPianoZone("StressChords", 0, keyCodes,
                              ChordUtilities::ChordType::Triad,
                              Zone::PianoVoicingStyle::Close, false, 0);
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    // Press all 10 keys
    for (int i = 0; i < 10; ++i) {
      proc.processEvent(InputID{0, 81 + i}, true);
    }
    // Release all 10 keys
    for (int i = 0; i < 10; ++i) {
      proc.processEvent(InputID{0, 81 + i}, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_PolyChords_10Keys)
    ->Unit(benchmark::kMicrosecond);

// Rapid layer toggling while playing notes
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_RapidLayerSwitch)
(benchmark::State &state) {
  addCommandMapping(0, 70, static_cast<int>(MIDIQy::CommandID::LayerToggle), 1);
  addNoteMapping(0, 81, 60, 100, 1);
  addNoteMapping(1, 81, 72, 100, 1); // Different note on layer 1
  proc.forceRebuildMappings();

  InputID layerKey{0, 70};
  InputID noteKey{0, 81};

  for (auto _ : state) {
    for (int i = 0; i < 50; ++i) {
      proc.processEvent(layerKey, true); // Toggle layer
      proc.processEvent(layerKey, false);
      proc.processEvent(noteKey, true); // Play note
      proc.processEvent(noteKey, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_RapidLayerSwitch)
    ->Unit(benchmark::kMicrosecond);

// 50+ simultaneous active voices
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_ManyActiveVoices)
(benchmark::State &state) {
  // Create 50 note mappings
  for (int i = 0; i < 50; ++i) {
    addNoteMapping(0, 20 + i, 36 + i, 100, 1);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    // Press all 50 keys (creates 50 active voices)
    for (int i = 0; i < 50; ++i) {
      proc.processEvent(InputID{0, 20 + i}, true);
    }
    // Release all
    for (int i = 0; i < 50; ++i) {
      proc.processEvent(InputID{0, 20 + i}, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_ManyActiveVoices)
    ->Unit(benchmark::kMicrosecond);

// 20 active ADSR envelopes (timer callback stress)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_ADSREnvelopes_20)
(benchmark::State &state) {
  // Create 20 expression mappings with ADSR
  for (int i = 0; i < 20; ++i) {
    addExpressionCCMapping(0, 20 + i, 1 + i, 1, true, 50, 100, 80, 200);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    // Trigger all 20 envelopes
    for (int i = 0; i < 20; ++i) {
      proc.processEvent(InputID{0, 20 + i}, true);
    }
    // Release all
    for (int i = 0; i < 20; ++i) {
      proc.processEvent(InputID{0, 20 + i}, false);
    }
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_ADSREnvelopes_20)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 8: Hot Path - GridCompiler and ZoneManager
// =============================================================================

// Full grid compile: 9 layers, 20 mappings, 5 zones (realistic preset)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, HotPath_GridCompiler_FullRebuild)
(benchmark::State &state) {
  for (int layer = 0; layer < 9; ++layer) {
    for (int k = 0; k < 3; ++k) {
      addNoteMapping(layer, 70 + layer * 2 + k, 60 + k, 100, 1);
    }
  }
  addNoteMapping(0, 81, 60, 100, 1);
  addNoteMapping(0, 82, 62, 100, 1);
  std::vector<std::shared_ptr<Zone>> zonesToRemove;
  for (int i = 0; i < 5; ++i) {
    std::vector<int> keys = {81 + i * 2, 82 + i * 2};
    auto z = createZone("Z" + juce::String(i), 0, keys,
                        ChordUtilities::ChordType::Triad, PolyphonyMode::Poly);
    proc.getZoneManager().addZone(z);
    zonesToRemove.push_back(z);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  for (auto _ : state) {
    (void)GridCompiler::compile(presetMgr, deviceMgr, proc.getZoneManager(),
                                touchpadMixerMgr, settingsMgr);
  }
  for (auto &z : zonesToRemove) {
    proc.getZoneManager().removeZone(z);
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, HotPath_GridCompiler_FullRebuild)
    ->Unit(benchmark::kMicrosecond);

// ZoneManager: add 5 zones (each triggers rebuildLookupTable)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, HotPath_ZoneManager_AddFiveZones)
(benchmark::State &state) {
  for (auto _ : state) {
    std::vector<std::shared_ptr<Zone>> zones;
    for (int i = 0; i < 5; ++i) {
      std::vector<int> keys = {81 + i, 82 + i, 83 + i};
      auto z =
          createZone("BmZ" + juce::String(i), 0, keys,
                     ChordUtilities::ChordType::Triad, PolyphonyMode::Poly);
      proc.getZoneManager().addZone(z);
      zones.push_back(z);
    }
    for (auto &z : zones) {
      proc.getZoneManager().removeZone(z);
    }
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, HotPath_ZoneManager_AddFiveZones)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 9: Strum, Portamento/Legato, Rhythm path, Touchpad
// =============================================================================

// Zone with Strum mode: buffer + trigger path
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Feature_Zone_Strum_Trigger)
(benchmark::State &state) {
  auto zone = createZone("StrumZone", 0, {81, 87, 69},
                         ChordUtilities::ChordType::Triad, PolyphonyMode::Poly);
  zone->playMode = Zone::PlayMode::Strum;
  zone->strumSpeedMs = 50;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
  proc.getZoneManager().removeZone(zone);
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Feature_Zone_Strum_Trigger)
    ->Unit(benchmark::kMicrosecond);

// Legato zone with adaptive glide (RhythmAnalyzer path)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Feature_Zone_Legato_AdaptiveGlide)
(benchmark::State &state) {
  auto zone =
      createZone("LegatoAdaptive", 0, {81, 87, 69},
                 ChordUtilities::ChordType::None, PolyphonyMode::Legato);
  zone->glideTimeMs = 50;
  zone->isAdaptiveGlide = true;
  zone->maxGlideTimeMs = 200;
  proc.getZoneManager().addZone(zone);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
  proc.getZoneManager().removeZone(zone);
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Feature_Zone_Legato_AdaptiveGlide)
    ->Unit(benchmark::kMicrosecond);

// Touchpad: processTouchpadContacts (Finger1Down -> Note)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Feature_Touchpad_FingerDownUp)
(benchmark::State &state) {
  addTouchpadNoteMapping(0, 60, 1);
  proc.forceRebuildMappings();
  mockMidi.clear();

  uintptr_t deviceHandle = 0x9000;
  std::vector<TouchpadContact> downContacts = {{0, 100, 100, 0.5f, 0.5f, true}};
  std::vector<TouchpadContact> upContacts = {{0, 100, 100, 0.5f, 0.5f, false}};

  for (auto _ : state) {
    proc.processTouchpadContacts(deviceHandle, downContacts);
    proc.processTouchpadContacts(deviceHandle, upContacts);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Feature_Touchpad_FingerDownUp)
    ->Unit(benchmark::kMicrosecond);

// Axis/pitch-pad path: handleAxisEvent (scroll or pointer)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Feature_HandleAxisEvent)
(benchmark::State &state) {
  // Expression CC on ScrollUp (InputTypes::ScrollUp) - if no mapping, still
  // exercises path
  addExpressionCCMapping(0, InputTypes::ScrollUp, 1, 1, false);
  proc.forceRebuildMappings();
  mockMidi.clear();

  uintptr_t deviceHandle = 0;
  for (auto _ : state) {
    proc.handleAxisEvent(deviceHandle, InputTypes::ScrollUp, 1.0f);
    proc.handleAxisEvent(deviceHandle, InputTypes::ScrollUp, 0.0f);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Feature_HandleAxisEvent)
    ->Unit(benchmark::kMicrosecond);

// =============================================================================
// Category 10: Stress - Many zones, layer search
// =============================================================================

// 15 zones, each with 3 keys; one processEvent (worst-case zone lookup)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_ManyZones_15)
(benchmark::State &state) {
  std::vector<std::shared_ptr<Zone>> zones;
  for (int i = 0; i < 15; ++i) {
    std::vector<int> keys = {70 + i * 2, 71 + i * 2, 72 + i * 2};
    auto z = createZone("StressZ" + juce::String(i), 0, keys,
                        ChordUtilities::ChordType::None, PolyphonyMode::Poly);
    proc.getZoneManager().addZone(z);
    zones.push_back(z);
  }
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 70};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
  for (auto &z : zones) {
    proc.getZoneManager().removeZone(z);
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_ManyZones_15)
    ->Unit(benchmark::kMicrosecond);

// Layer search: 9 layers with note on layer 8, toggle all on (already in
// Layer_AllActive; this one emphasizes compile + one hit)
BENCHMARK_DEFINE_F(MidiBenchmarkFixture, Stress_LayerSearch_AllNineActive)
(benchmark::State &state) {
  for (int layer = 1; layer < 9; ++layer) {
    auto layerNode = presetMgr.getLayerNode(layer);
    if (layerNode.isValid()) {
      layerNode.setProperty("isActive", true, nullptr);
    }
  }
  addNoteMapping(8, 81, 72, 100, 1);
  proc.forceRebuildMappings();
  mockMidi.clear();

  InputID input{0, 81};
  for (auto _ : state) {
    proc.processEvent(input, true);
    proc.processEvent(input, false);
    mockMidi.clear();
  }
}
BENCHMARK_REGISTER_F(MidiBenchmarkFixture, Stress_LayerSearch_AllNineActive)
    ->Unit(benchmark::kMicrosecond);
