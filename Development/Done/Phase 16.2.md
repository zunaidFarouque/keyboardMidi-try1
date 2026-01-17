# 🤖 Cursor Prompt: Phase 16.2 - Visualizer Debug "Rainbow Mode"

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
*   **Current State:** Phase 16.1 implemented. We have a 4-layer rendering system.
*   **Problem:** The user reports that the "Underlay" (Zone Background) layer is not visible. It seems hidden or transparent.
*   **Goal:** Implement a temporary "Debug Mode" in `VisualizerComponent::paint`.

**Objective:**
Modify `VisualizerComponent.cpp` to ignore the Zone Engine logic temporarily and instead render **Random Colors** for all 4 layers of every key.

**Rendering Requirements:**
1.  **Layer 1 (Underlay):** `fullBounds`. Random Opaque Color (e.g., Red).
2.  **Layer 2 (Fill):** `keyBounds` (which is `fullBounds.reduced(3.0f)`). Random Opaque Color (e.g., Blue).
3.  **Layer 3 (Border):** `keyBounds` outline. Random Bright Color (e.g., Green, 2px thick).
4.  **Layer 4 (Text):** Random Contrast Color (e.g., White/Black).

**Logic Update (`paint`):**
*   Create a `juce::Random rng;` at the start of paint.
*   Iterate through `KeyboardLayoutUtils`.
*   **Do NOT** call `zoneManager`.
*   Generate 4 random colors using `juce::Colour(rng.nextInt())`.
*   Draw the rects strictly using the geometry logic defined in Phase 16.1.

**Outcome:**
We expect to see a chaotic "Clown Keyboard".
*   If we see a **border of Color A** surrounding a **box of Color B**, the layering geometry is **CORRECT**.
*   If we only see one solid block, the **padding/reduction logic is BROKEN**.

---

**Generate the code for: The temporary `VisualizerComponent::paint` method.**