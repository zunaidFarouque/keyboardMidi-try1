### Analysis of the Bug

1.  **The "Compiler" Bug (`InputProcessor`):**
    *   **Logic:** `if (aliasHash == 0) { ... targetLayer.compiledMap[globalInputId] = action; }`
    *   **Problem:** This block only runs if `aliasName` is NOT empty. But when `DeviceManager` returns `aliasName="Global (All Devices)"`, we might be failing to detect `hash==0` correctly if the name string doesn't match the hardcoded expectation of the hash function.
    *   **Worse:** If `foundAliasHash == 0` (legacy fallback), you add to `compiledMap`. But if `aliasName` is "Global", do you also add to specific hardware IDs? No, `getHardwareForAlias("Global")` returns nothing (because Global has no specific hardware list, it's a catch-all).
    *   **The Fix:** We must ensure that if a mapping is for "Global", it is added to `compiledMap` with key `{0, inputKey}`. This seems covered by the legacy fallback, but we need to ensure the primary `aliasName` path handles `hash==0` correctly even if `getHardwareForAlias` returns empty.

2.  **The Visualizer Bug (`VisualizerComponent`):**
    *   **Logic:** `if (!zColor.has_value() && simResult.state == VisualState::Inherited)`
    *   **Problem:** When you select "External" view, `simResult` for a Global Zone returns `VisualState::Inherited`. This enters the `if`. It checks `getZoneColorForKey(key, 0)`. **This works.**
    *   **BUT:** When you select **"Any / Master"** view (`currentViewHash == 0`), `simResult` returns `VisualState::Active`.
        *   `state == Active` != `Inherited`.
        *   The `if` condition fails.
        *   It calls `getZoneColorForKey(key, 0)`.
        *   If `ZoneManager` finds the zone, it returns color.
        *   **Wait:** Why did you say "Any/Master" view fails to show color?
        *   If `simulateInput` returns `Active`, then `ZoneManager` *found* the zone on Hash 0.
        *   So `getZoneColorForKey(key, 0)` *should* find it too.
        *   **Hypothesis:** `simulateInput` finds the zone using `zoneManager.handleInput`. `getZoneColorForKey` uses `zoneLookupTable.find`. They are the same.
        *   **Wait, re-read your report:** *"Inheritance from global is not being colored."* Ah, you meant in "External" view?
            *   "Visualizer inheritance keys does not show from global, when I selected view 'external'".
            *   Okay, so `simResult.state` IS `Inherited`.
            *   So it enters the block: `if (!zColor.has_value() && state == Inherited)`.
            *   It calls `getZoneColorForKey(key, 0)`.
            *   If this returns `nullopt`, then the Global Zone is missing from the lookup table for that key.

### The Real Fix (Phase 47)

We need to fix the logic in `InputProcessor` to ensure Global Mappings are compiled correctly, and update `VisualizerComponent` to be more robust about fetching colors.

**Step 1: Fix `InputProcessor::addMappingFromTree`**
*   When `aliasHash == 0`, we MUST add `{0, key}` to `compiledMap`. Currently, the code relies on `aliasName` being non-empty. If "Global" alias has no name (or empty string in some paths), it skips.
*   We need to force: `if (aliasHash == 0 || aliasName == "Global (All Devices)")` -> Add to `{0, key}`.

**Step 2: Fix `VisualizerComponent::refreshCache`**
*   **Simplify Logic:**
    *   Try `getZoneColorForKey(key, currentViewHash)`.
    *   If failed AND `currentViewHash != 0`: Try `getZoneColorForKey(key, 0)`.
    *   (Remove the dependency on `VisualState::Inherited` for the color lookup. If we found a zone action, we should find a zone color, regardless of state).

***

# 🤖 Cursor Prompt: Phase 47 - Fix Compiler & Visualizer Lookup

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** Phase 46.5 Complete.
*   **Bug 1 (Audio):** Keys mapped to "Global" alias don't work (silent).
*   **Bug 2 (Visuals):** In "External" view, keys that inherit from "Global" Zone show text labels but **no background color**.

**Phase Goal:**
1.  **InputProcessor:** Ensure "Global" mappings are always compiled into `{0, Key}` in the `compiledMap`.
2.  **Visualizer:** Simplify the color lookup logic to always fallback to Global Color if Local Color is missing, regardless of the `VisualState`.

**Strict Constraints:**
1.  **Compiler:** If `aliasHash == 0`, strictly add to `compiledMap[{0, key}]`.
2.  **Visualizer:** If `simResult.isZone` is true:
    *   First try `getZoneColorForKey(key, currentViewHash)`.
    *   If that fails, try `getZoneColorForKey(key, 0)`.
    *   Use the result if valid. Ignore `simResult.state` for this specific check.

---

### Step 1: Update `Source/InputProcessor.cpp`
Fix the compiler loop.

**Refactor `addMappingFromTree`:**
*   Inside the `if (!aliasName.isEmpty())` block:
    *   Keep the hardware loop.
    *   **Explicit Check:**
        ```cpp
        // Ensure Global mappings are compiled even if hardware list is empty
        if (aliasHash == 0) 
        {
            InputID globalInputId = {0, inputKey};
            targetLayer.compiledMap[globalInputId] = action;
        }
        ```

### Step 2: Update `Source/VisualizerComponent.cpp`
Fix the color fallback.

**Refactor `refreshCache` loop (Color Section):**
```cpp
          // 1. Determine Base Color
          if (simResult.isZone) 
          {
            // Try specific view first
            auto zColor = zoneManager->getZoneColorForKey(keyCode, currentViewHash);

            // Fallback to Global if not found
            if (!zColor.has_value()) 
            {
              zColor = zoneManager->getZoneColorForKey(keyCode, 0);
            }

            if (zColor.has_value()) 
            {
              underlayColor = zColor.value();
            }
          } 
```

---

**Generate code for: Updated `InputProcessor.cpp` and `VisualizerComponent.cpp`.**