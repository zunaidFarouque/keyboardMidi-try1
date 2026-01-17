
# 🤖 Cursor Prompt: Phase 15.1 - Zone Wildcards & Enhanced Logging

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** Phase 15 Complete.
*   **Issue 1 (Zone Logic):** Zones set to "Any / Master" (Hash 0) do not trigger because `InputProcessor` only checks the specific aliases assigned to the hardware. It fails to check the "Wildcard" ID.
*   **Issue 2 (Logging):** The log only shows input keys, not the resulting MIDI or the source (Zone vs Mapping).

**Phase Goal:** Fix the "Any" device logic and implement rich logging.

**Strict Constraints:**
1.  **Priority Order:** Specific Alias Zones > "Any" Device Zones > Manual Mappings.
2.  **Architecture:** Do not break existing thread safety (`ReadWriteLock`).

---

### Step 1: Update `InputProcessor.h/cpp` (Logic Fix)

**1. Implement `simulateInput`:**
Create a method that mirrors `processEvent` but returns data instead of playing sound.
*   `std::pair<std::optional<MidiAction>, juce::String> simulateInput(uintptr_t deviceHandle, int keyCode);`
*   **Return:** Pair of `{ Action, SourceDescription }`.

**2. Update Logic (Shared by `processEvent` and `simulateInput`):**
Refactor the core lookup logic into a helper or ensure both methods follow this flow:

1.  **Get Aliases:** `deviceManager.getAliasesForHardware(deviceHandle)`.
2.  **Check Specific Zones:** Iterate through these aliases. Call `zoneManager.handleInput({aliasHash, keyCode})`.
    *   If valid, Return Result (Source: "Zone: [Name]").
3.  **Check Wildcard Zone (THE FIX):**
    *   Call `zoneManager.handleInput({0, keyCode})`.
    *   If valid, Return Result (Source: "Zone: [Name]").
4.  **Check Manual Mapping (Fallback):**
    *   Check `keyMapping` for `{aliasHash, keyCode}`.
    *   Check `keyMapping` for `{0, keyCode}`.
    *   If valid, Return Result (Source: "Mapping").

### Step 2: Update `MainComponent.cpp` (Logging)

Refactor `logEvent` to use the new simulation method.

**Logic:**
1.  Call `auto [action, source] = inputProcessor.simulateInput(device, key);`
2.  **Format String:**
    *   Standard Input part: `Dev: [...] | Key: Q`.
    *   **If Action exists:**
        *   Append ` -> [MIDI] Note 60 (C4)`.
        *   Append ` | Source: ` + `source`.
    *   *Result:* `Dev: 100d7 | Key: Q -> [MIDI] Note 60 (C4) | Source: Zone: Main Keys`

---

**Generate code for: Updated `InputProcessor.h`, Updated `InputProcessor.cpp`, and Updated `MainComponent.cpp`.**