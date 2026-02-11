---
name: touchpad-layout-expansion
overview: Define additional touchpad layout types for the MIDI controller, each reusing the existing global/local parameter architecture and focusing on concrete benefits for musicians.
todos:
  - id: phase1-layouts
    content: Design and specify Harmonic Grid, Chord Pad, and Drum+FX Split layouts in detail, including their layout-specific parameters and presets.
    status: pending
  - id: phase2-layouts
    content: Design and specify Step Sequencer, Scale Solo, Bassline, and Strum layouts with their configuration options.
    status: pending
  - id: phase3-layouts
    content: Design and specify Clip/Scene Launcher, Mixer+Sends Hybrid, Utility/Transport, and XY FX layouts and how they integrate with DAWs and instruments.
    status: pending
isProject: false
---

## Touchpad Layout Expansion Plan

### 1) Reuse of Existing Config Architecture

- **Global/general parameters (shared across all layouts)**
  - MIDI I/O basics (MIDI channel, port, transpose, velocity curve, aftertouch mode)
  - Global musical context (root note, scale, tempo sync, quantization/swing, octave range limits)
  - Visual/UI options (color theme, brightness, per-note color mode on/off)
  - Global performance options (latency/processing mode, global sustain/hold behavior, clock source)
- **Layout-specific parameters (per layout type)**
  - Grid shape and mapping rules (rows/columns, note/CC mapping pattern)
  - Behavior modes (latching vs momentary, step vs live trigger, etc.)
  - Layout-local scales, chord sets, pattern sets, step lengths, banks, etc.

All new layouts below assume this same two-layer config structure.

---

### 2) Harmonic Grid / Isomorphic Keyboard Layout

A layout where each pad is a note, arranged so that shapes are consistent across keys (e.g. 4th-per-row like Push/Linnstrument).

- **Layout-specific parameters**
  - Interval between rows (e.g. perfect 4th, major 3rd)
  - Grid size (rows/columns actually used)
  - Scale filter (show only scale notes vs full chromatic)
  - Per-row octave offsets (e.g. bass row vs melody rows)
- **Musician benefits**
  - **Consistent fingering in any key**: learn one shape for a chord/scale pattern and reuse it across all keys.
  - **Fast harmony exploration**: easy to slide chord shapes around without hitting wrong notes when scale filter is on.
  - **Compact performance surface**: both bass and melody lines can be played on one grid with octave-row offsets.

---

### 3) Chord Pad / Progression Layout

Each pad triggers a full chord or voicing, optionally with inversions and velocity shaping.

- **Layout-specific parameters**
  - Chord set bank (I–VII, custom user chords, borrowed chords)
  - Voicing style (close, open, spread, add9/11/13 toggles)
  - Inversion rules (root/1st/2nd inversion, voice-leading aware)
  - Trigger mode (momentary vs latch; legato re-trigger behavior)
- **Musician benefits**
  - **Instant rich harmony**: non-pianists can perform full progressions with one finger per chord.
  - **Consistent live sets**: quickly recall the same progression set across songs.
  - **Smarter voice-leading**: inversion rules help avoid jumps when changing chords live.

---

### 4) Scale Solo / Lead Layout

Pads laid out in a single scale or mode, optimised for fast melodic lines and solos.

- **Layout-specific parameters**
  - Scale/mode selection per layout (major, minor, dorian, etc.)
  - Register focus (low, mid, high) and octave spread
  - Note density (diatonic only vs chromatic passing tones on edge pads)
  - Legato/glide options (MIDI pitch bend/glide per pad row)
- **Musician benefits**
  - **Safe improvisation**: hard to hit a “wrong” note when constrained to scale.
  - **Speed-friendly ergonomics**: layouts tuned for fast runs and repeated motifs.
  - **Expressive leads**: optional glide/legato mappings enable synth-style solos.

---

### 5) Bassline Layout

A layout optimized for basslines: fewer notes, strong emphasis on root, fifth, octave, and groove.

- **Layout-specific parameters**
  - Root-note centered mapping (dedicated root/fifth/octave pads)
  - Range clamp to bass registers
  - Accent/ghost-note pads (velocity or CC accent)
  - Slide mode (legato slide vs retrigger)
- **Musician benefits**
  - **Solid low-end control**: easy to stay in the bass register with useful intervals at hand.
  - **Groove accents**: dedicated accent/ghost pads make rhythmic nuance simple.
  - **Live-friendly**: avoids accidentally jumping into high registers mid-performance.

---

### 6) Step Sequencer Grid Layout

Pads represent steps in one or more lanes (pitch, velocity, gate, CC). Tapping toggles or edits steps.

- **Layout-specific parameters**
  - Number of steps and tracks (e.g. 16x2, 32x1)
  - Per-row lane meaning (pitch row, velocity row, gate row, CC row)
  - Step resolution and length (1/16, 1/8T, etc.)
  - Edit mode (toggle on/off, cycle values like velocity/gate, tie/slide)
- **Musician benefits**
  - **Hands-on pattern building**: program rhythms and melodies visually on the grid.
  - **Immediate feedback**: see which steps are active and adjust during playback.
  - **Performance sequencing**: mutate patterns live by toggling and re-accenting steps.

---

### 7) XY FX / Macro Pad Layout

Pads or pad groups act as XY or multi-parameter controls for FX and synth macros using CC outputs.

- **Layout-specific parameters**
  - XY group definition (which pads form an XY surface)
  - CC mappings for X/Y axes and optional pressure/aftertouch
  - Range and curve per CC (linear, log, inverted)
  - Hold vs return-to-center behavior
- **Musician benefits**
  - **Deep FX control**: sweep filters, reverb size, and other parameters intuitively.
  - **Expressive sound design**: combine X/Y plus pressure for three dimensions of control.
  - **Compact macro control**: replaces multiple physical knobs/sliders in a performance.

---

### 8) Clip/Scene Launcher Layout

Pads trigger clips, scenes, or preconfigured patterns via MIDI notes/CCs.

- **Layout-specific parameters**
  - Bank size and paging (how many clips/scenes visible per page)
  - Binding mode (MIDI notes vs CCs, toggle vs momentary)
  - Scene vs clip rows (top for scenes, rest for clips)
  - Feedback policy (LED color/state per clip status if supported)
- **Musician benefits**
  - **Session-style performance**: launch stems, loops, and sections like Ableton-style grids.
  - **Clear structure**: scene rows help organize intros, verses, drops, etc.
  - **Visual state tracking**: LED/visual feedback reduces confusion in live shows.

---

### 9) Mixer + Sends Hybrid Layout

An evolution of the existing mixer: dedicated faders plus send/FX controls on the same surface.

- **Layout-specific parameters**
  - Channel count and grouping (e.g. 4 or 8 channels visible)
  - Per-channel mapping (volume, pan, send A/B, mute/solo)
  - Bank switching behavior (how channels 1–8, 9–16, etc. are paged)
  - Fader curves and pickup mode (jump vs soft takeover)
- **Musician benefits**
  - **Deeper mix control**: adjust both level and FX sends without changing layouts.
  - **Faster live balancing**: tweak volume and reverb/delay per track mid-song.
  - **Consistent channel mapping**: predictable banks across songs and DAWs.

---

### 10) Drum Performance + FX Split Layout

A hybrid of drum pads and FX/macros on one surface.

- **Layout-specific parameters**
  - Split definition (which rows/columns are drums vs FX)
  - Drum bank assigned notes and velocity behavior
  - FX macro mappings (CCs, ranges, hold/toggle behavior)
  - Optional sidechain/duck control pad (sends CC or note to sidechain bus)
- **Musician benefits**
  - **One-surface performance**: play drums and shape FX without switching modes.
  - **Dynamic drops and builds**: dedicated pads for risers, filters, and mutes.
  - **Efficient space use**: ideal for small rigs where every pad matters.

---

### 11) Strum/Chord-Strum Layout

Pads represent string positions or strumming directions, triggering chord notes in rhythmic ways.

- **Layout-specific parameters**
  - Number of virtual strings and tuning per string
  - Assigned chord per strum group (e.g. per column)
  - Strum direction and timing (up/down, rolled delays between string notes)
  - Humanization amount (timing/velocity randomness)
- **Musician benefits**
  - **Guitar-like expression without a guitar**: natural-sounding strums from pads.
  - **Rhythmic variation**: easy to create different feels by changing strum direction/pattern.
  - **Songwriting helper**: quickly audition chord+strum ideas without detailed programming.

---

### 12) Utility / Transport & Markers Layout

Pads control transport, locators, and common DAW functions.

- **Layout-specific parameters**
  - Transport bindings (play, stop, rec, loop, punch, tap tempo)
  - Marker controls (set marker, next/prev marker, section jump)
  - Global toggle bindings (click on/off, automation arm, etc.)
  - Safety options (confirmation for critical actions like stop-all)
- **Musician benefits**
  - **Hands-on session control**: run a show without touching mouse/keyboard.
  - **Reduced mistakes**: large, clearly-mapped pads for crucial commands.
  - **Quicker takes**: punch in/out and marker navigation directly from the controller.

---

### 13) Prioritisation / Roadmap

- **Phase 1 (performance-centric)**
  - Implement Harmonic Grid, Chord Pad, Drum+FX Split using existing touchpad foundations.
- **Phase 2 (composition & sequencing)**
  - Add Step Sequencer Grid, Scale Solo, Bassline, Strum Layouts.
- **Phase 3 (rig control & deep integration)**
  - Add Clip/Scene Launcher, Mixer+Sends Hybrid, Utility/Transport Layout, XY FX.

Each new layout type plugs into the same global vs layout-specific parameter system, so the UI work focuses on: (a) surfacing the layout-specific controls, and (b) providing per-layout presets that musicians can quickly load and tweak.