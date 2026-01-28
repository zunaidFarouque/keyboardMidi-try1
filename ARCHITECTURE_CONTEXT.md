# 🧠 OmniKey Architecture Codex
**Current Phase:** Phase 48 (Layer System & UI Polish completed).
**Tech Stack:** C++20, JUCE 8, CMake, Win32 API.

## 🏛 Core Philosophy
1.  **The "Compiler" Strategy:** We do **NOT** do complex logic on key presses. We pre-calculate everything (Chords, Scales, Pitch Bends, Mappings) into Hash Maps/Vectors during configuration (`rebuildCache` methods). The Audio Thread must be **O(1)**.
2.  **Thread Safety:**
    *   **Audio/Input Thread:** Reads data. Uses `ScopedReadLock`.
    *   **UI/Config Thread:** Writes data. Uses `ScopedWriteLock`.
    *   **Async UI:** The Input Thread **NEVER** touches UI classes directly. It pushes POD structs to a Queue. A Timer or VBlankAttachment handles the actual painting/logging.
3.  **Win32 Isolation:** **NEVER** include `<windows.h>` in a `.h` file. Use `void*` or `uintptr_t` for handles. Include windows headers only in `.cpp` files.

## 🏗 Subsystem Breakdown

### 1. Input System (`RawInputManager`)
*   **Mechanism:** Uses `WM_INPUT` with `RIDEV_INPUTSINK` (Background Input).
*   **Blocking:** Uses a "Focus Guard" strategy (`AttachThreadInput` + `SetForegroundWindow`) to steal focus when MIDI Mode is Active. We do **not** use `WH_KEYBOARD_LL` anymore.
*   **Device Distinction:** We distinguish devices by their Handle.

### 2. Device Management (`DeviceManager`)
*   **Aliases:** Maps specific Hardware IDs to Logical Names ("Laptop", "External").
*   **Hygiene:** `validateConnectedDevices` removes unplugged hardware from memory to prevent "Zombie IDs".
*   **Storage:** Saves to `%APPDATA%/OmniKey/OmniKeyConfig.xml`.

### 3. The Engine (`InputProcessor`)
*   **Layer Stack:** 9 Static Layers (0=Base, 1-8=Overlay).
*   **Dual Maps:**
    *   `configMap`: Maps `{AliasHash, KeyCode} -> Action` (For UI/Visualizer).
    *   `compiledMap`: Maps `{HardwareID, KeyCode} -> Action` (For Audio).
*   **Logic:** Checks layers Top-Down. If no Manual Mapping found, falls back to `ZoneManager`.

### 4. Zone Engine (`ZoneManager`, `Zone`)
*   **Smart Zones:** Key ranges mapped to Scales/Chords.
*   **Logic:** `Zone` pre-compiles `keyToChordCache` (Vector of Notes) and `keyToLabelCache` (Strings).
*   **Modes:** Supports **Adaptive Guitar Voicing** (Octave folding), **Strict/Loose Harmony** (Ghost notes), and **Strumming**.

### 5. Audio Output (`VoiceManager`, `MidiEngine`)
*   **State Machine:** Tracks `PolyphonyMode` (Poly, Mono, Legato).
*   **Legato:** Implements "Ghost Anchor" logic. If a glide occurs, the original note stays active (Anchor) while Pitch Bend moves.
*   **Safety:** Includes a **Watchdog Timer** (100ms) to kill "Zombie Voices" (Active voice but Empty Stack).

### 6. UI Architecture
*   **Visualizer:** Uses `VBlankAttachment` and **Image Caching**. Only redraws when `needsRepaint` atomic is true.
*   **Editors:** Use `juce::ValueTree` listeners.
*   **Destruction Safety:** `MainComponent` explicitly clears Tabs and shuts down Managers in its Destructor to prevent `_CrtIsValidHeapPointer` crashes. **Declaration Order in `.h` is critical** (Managers Top, UI Bottom).

## ⚠️ Critical Constraints (Do Not Break)
1.  **Shutdown:** `Visualizer`, `MappingEditor`, etc. use `SafePointer` or explicit `shutdown()` calls. Do not remove these checks.
2.  **Layers:** Layer 0 is the Base. Layers 1-8 are toggled via Commands (`LayerMomentary`, `LayerToggle`).
3.  **Presets:** `PresetManager` uses a Transaction (`isLoading` flag) to prevent Event Storms during load.