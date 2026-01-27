# 🤖 Cursor Prompt: Phase 39.7 - Fix Visualizer Conflict Detection

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 39.6 Complete. Visualizer shows Active, Inherited, and Override states correctly.
*   **The Bug:** Keys with **Self-Conflicts** (e.g., covered by Two Zones on the *same* device alias) are not shown in **Red**. They just show as "Active".
*   **Diagnosis:** `InputProcessor::simulateInput` returns the first valid action it finds. It does not check for overlaps.

**Phase Goal:**
1.  Implement `ZoneManager::getZoneCountForKey` to detect how many zones cover a specific key/alias tuple.
2.  Update `InputProcessor::simulateInput` to set `VisualState::Conflict` if multiple zones overlap.
3.  Update `VisualizerComponent` to render Conflicts in **Red**.

**Strict Constraints:**
1.  **Definition of Conflict:**
    *   **Zone Overlap:** If `count > 1` for the current View Hash.
    *   **Mapping + Zone:** If a Manual Mapping exists AND a Zone exists for the *same* View Hash (unless we consider this an Override feature, but let's flag it as Conflict if `isOverride` isn't sufficient or if the user wants to see overlaps. *Decision:* Let's stick to **Zone vs Zone** as the primary "Error" Conflict. Manual vs Zone is usually intentional Override).
2.  **Color:** `VisualState::Conflict` = `juce::Colours::red`.

---

### Step 1: Update `ZoneManager` (`Source/ZoneManager.h/cpp`)
Add overlap detection.

**Method:** `int getZoneCountForKey(int keyCode, uintptr_t aliasHash);`
*   **Logic:**
    *   Acquire Read Lock.
    *   Counter `int count = 0`.
    *   **Iterate `zones` vector** (NOT the lookup table, because the lookup table only stores the *winner*).
    *   For each zone:
        *   Check `targetAliasHash == aliasHash`.
        *   Check if `inputKeyCodes` contains `keyCode` (or use `processKey` dry run logic if complex).
        *   If match, `count++`.
    *   Return `count`.

### Step 2: Update `InputProcessor.cpp`
Integrate into the simulation.

**Refactor `simulateInput`:**
*   ... (Existing Logic to find the Action) ...
*   **Conflict Check:**
    *   If `res.isZone`:
        *   `int count = zoneManager.getZoneCountForKey(key, viewDevice);`
        *   If `count > 1`: `res.state = VisualState::Conflict;`
*   *Note:* Perform this check *after* determining the source layer. If we are viewing "Laptop", check overlap on "Laptop".

### Step 3: Update `VisualizerComponent.cpp`
Render the red flag.

**Update `refreshCache`:**
*   Inside the color logic block:
*   `if (result.state == VisualState::Conflict)`
    *   `underlayColor = juce::Colours::red.withAlpha(0.8f);`
    *   `borderColor = juce::Colours::red;`
    *   `alpha = 1.0f;`

---

**Generate code for: Updated `ZoneManager.h/cpp`, Updated `InputProcessor.cpp`, and Updated `VisualizerComponent.cpp`.**