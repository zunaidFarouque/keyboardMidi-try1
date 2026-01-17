# 🤖 Cursor Prompt: Phase 16.1 - Layered Visualizer & Zone Colors

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
We are building "OmniKey".
*   **Current State:** Phase 16 Complete. The Visualizer exists but is basic.
*   **Problem:** It is hard to distinguish Zones visually. The rendering is flat.
*   **Phase Goal:** Implement a **4-Layer Rendering System** (Underlay -> Fill -> Border -> Text) and assign Colors to Zones.

**Strict Constraints:**
1.  **Layering Logic:**
    *   **Layer 1 (Underlay):** Fills the *entire* grid slot. Shows Zone Color.
    *   **Layer 2 (Fill):** The Key body, reduced by padding (e.g., 4px margin).
    *   **Layer 3 (Border):** The outline of the Key body.
    *   **Layer 4 (Text):** Text on top of the Key body.
2.  **Efficiency:** Query the Zone Engine *once* per key per paint, not 4 times.

---

### Step 1: Update `Zone` & `ZoneManager` (Color Support)
Zones need to look distinct.

**1. `Source/Zone.h`:**
*   Add member: `juce::Colour zoneColor;`.
*   Update `toValueTree` / `fromValueTree` to save/load this color (store as string hash or hex).

**2. `Source/ZoneManager.cpp`:**
*   **Auto-Coloring:** In `createDefaultZone` or `addZone`, if the color is default (black/transparent), assign one from a palette.
*   **Palette:** Create a static array of nice colors (Teal, Orange, Purple, Green, Red). Cycle through them based on `zones.size()`.
*   **New Method:** `std::optional<juce::Colour> getZoneColorForKey(int keyCode, uintptr_t aliasHash);`
    *   Iterate zones. If `processKey` (or logic) matches, return `zone->zoneColor`.

### Step 2: `VisualizerComponent` (The Rendering Engine)
Refactor `paint` completely to use the 4-layer approach.

**Updates in `Source/VisualizerComponent.cpp`:**

1.  **Geometry Helper:**
    *   Define `float keyMargin = 3.0f;` (Padding between grid slot and actual key).
    *   Define `float cornerSize = 4.0f;` (Roundedness).

2.  **Refactor `paint(juce::Graphics& g)`:**
    *   **Pre-calculation:** Create a temporary list/vector of state for all keys to avoid querying the lock repeatedly inside the drawing loop.
        *   Struct `KeyDrawState { Rect fullBounds; Rect keyBounds; Colour underlay; Colour fill; Colour border; String text; bool active; };`
    *   **Loop 1 (Calculate States):** Iterate `KeyboardLayoutUtils`.
        *   Get `fullBounds` from Utils.
        *   Get `keyBounds` = `fullBounds.reduced(keyMargin)`.
        *   **Underlay:** Call `zoneManager->getZoneColorForKey(...)`. If found, set `underlay`, else Transparent.
        *   **Fill:** Check `activeKeys`. If Pressed -> Bright Yellow. Else -> Dark Grey (`0xff333333`).
        *   **Text:** Logic from Phase 12 (Note Name).
    *   **Loop 2 (Draw Underlays):** Iterate States. Fill `fullBounds` with `underlay`.
    *   **Loop 3 (Draw Keys):** Iterate States.
        *   Fill `keyBounds` (Rounded) with `fill`.
        *   Draw Border `keyBounds` (Rounded) with `border` (Light Grey).
    *   **Loop 4 (Draw Text):** Iterate States. Draw `text` centered in `keyBounds`.

### Step 3: Update `MainComponent`
Ensure the Visualizer is receiving the correct `DeviceManager` alias so it knows which "Virtual Keyboard" to display (usually "Master Input" or the currently selected device in the UI if we add that feature later. For now, assume Master/0).

---

**Generate code for: Updated `Zone.h`, Updated `ZoneManager.h/cpp`, and Updated `VisualizerComponent.cpp`.**