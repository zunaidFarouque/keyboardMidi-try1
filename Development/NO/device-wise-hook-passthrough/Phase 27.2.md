# 🤖 Cursor Prompt: Phase 27.2 - Per-Alias Passthrough & Visualizer Polish

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 27.1 Complete. "MIDI Mode" blocks *all* assigned devices globally.
*   **Problem 1:** Users want to selectively allow specific Aliases (e.g., "Laptop Keyboard") to type text even while MIDI Mode is active, without removing them from the rig.
*   **Problem 2:** When MIDI Mode is OFF, the Visualizer is completely blocked by an opaque overlay, making it impossible to see settings/keys.

**Phase Goal:**
1.  Implement a "Passthrough" flag per Alias in `DeviceManager`.
2.  Update `RawInputManager` to respect this flag (Inject key if Passthrough is ON).
3.  Update `VisualizerComponent` to show an "Inputs" menu with checkboxes and make the disabled overlay semi-transparent.

**Strict Constraints:**
1.  **Logic:** If an Alias is set to "Passthrough":
    *   The Low-Level Hook still blocks the physical key (global behavior).
    *   `RawInputManager` detects this specific device, **Re-injects** the keystroke (allowing typing), and **Does NOT** broadcast MIDI (silencing notes).
2.  **Visuals:** The "MIDI DISABLED" overlay must be `Colours::black.withAlpha(0.6f)` so the underlying keys are still visible.

---

### Step 1: Update `DeviceManager` (`Source/DeviceManager.h/cpp`)
Store the state.

**Requirements:**
1.  **Member:** `std::unordered_map<juce::String, bool> passthroughStates;` (Alias Name -> IsPassthrough).
2.  **Methods:**
    *   `void setPassthrough(String alias, bool enable);` (Broadcast change).
    *   `bool isPassthrough(String alias);`
    *   `bool isHardwarePassthrough(uintptr_t hardwareId);` (Helper: Lookup Alias for ID, then check state).
3.  **Serialization:** Save/Load these states in `MIDIQyConfig.xml`.

### Step 2: Update `RawInputManager.cpp`
Refactor the routing logic.

**Update `rawInputWndProc`:**
*   Inside `if (settings.isMidiModeActive())`:
    *   Check `DeviceManager::isHardwareAssigned(device)`.
    *   **If Assigned:**
        *   **NEW CHECK:** `if (deviceManager.isHardwarePassthrough(device))`
            *   **True (Passthrough Mode):** Call `InputInjector::injectKey(...)`. Return (Stop MIDI).
            *   **False (Standard MIDI):** Proceed to Broadcast MIDI (Do not inject).
    *   **If Unassigned:**
        *   Call `InputInjector::injectKey(...)`.

### Step 3: Update `VisualizerComponent` (`Source/VisualizerComponent.h/cpp`)
UI Updates.

**1. Header Update:**
*   Add `juce::TextButton inputMenuButton;` ("Inputs").
*   **Logic (`onClick`):**
    *   Create `juce::PopupMenu`.
    *   Iterate `deviceManager.getAllAliasNames()`.
    *   Add items as **Checkboxes** (`addItem(id, name, true, isPassthrough)`).
    *   On callback: Toggle the passthrough state in `DeviceManager`.

**2. Paint Update:**
*   In `paint()`, change the "Disabled" block logic.
*   Instead of `return` after drawing the overlay, ensure the overlay is drawn **last** (on top of keys) using `g.fillAll(Colours::black.withAlpha(0.5f));`.
*   Draw the text "MIDI MODE SUSPENDED" centered on top.

---

**Generate code for: Updated `DeviceManager`, Updated `RawInputManager`, and Updated `VisualizerComponent`.**