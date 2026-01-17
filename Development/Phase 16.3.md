# 🤖 Cursor Prompt: Phase 16.3 - Visualizer Layer Debug (Fixed Colors)

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
*   **Current State:** The visualizer test flight failed. Keys appeared solid white.
*   **Diagnosis:** Likely an issue with `g.setColour` not being called correctly, or random colors having 0 Alpha (invisible).
*   **Goal:** Force-render the 4 layers with **Fixed High-Contrast Colors** to verify the geometry math.

**Strict Constraints:**
1.  **Colors:** Use `juce::Colours::red` (Underlay), `juce::Colours::blue` (Fill), `juce::Colours::green` (Border), `juce::Colours::white` (Text).
2.  **Geometry:**
    *   `fullBounds` = From Layout Utils.
    *   `keyBounds` = `fullBounds.reduced(5.0f)` (Use **5px** to make the red underlay very obvious).
3.  **Drawing Order:** Ensure `g.setColour(...)` is called immediately before *every* draw operation.

---

### Action: Rewrite `VisualizerComponent::paint`

Replace the existing `paint` method with this logic:

```cpp
void VisualizerComponent::paint(juce::Graphics& g)
{
    // 1. Background
    g.fillAll(juce::Colours::black);

    // 2. Iterate Layout
    for (auto const& [keyCode, geometry] : KeyboardLayoutUtils::getLayout())
    {
        // Calculate Bounds
        auto fullBounds = KeyboardLayoutUtils::getKeyBounds(
            keyCode, 
            std::min(getWidth(), getHeight()) / 15.0f, // Approx key size
            0.0f
        );
        
        // Center the keyboard roughly
        fullBounds.translate(50, 50); 

        // Define Inner Key Body (shrunk by 5 pixels to reveal Underlay)
        auto keyBounds = fullBounds.reduced(5.0f);

        // --- LAYER 1: UNDERLAY (RED) ---
        // If this is working, you should see a Red box BEHIND the Blue key
        g.setColour(juce::Colours::red);
        g.fillRect(fullBounds);

        // --- LAYER 2: BODY FILL (BLUE) ---
        g.setColour(juce::Colours::blue);
        g.fillRoundedRectangle(keyBounds, 4.0f);

        // --- LAYER 3: BORDER (GREEN) ---
        g.setColour(juce::Colours::green);
        g.drawRoundedRectangle(keyBounds, 4.0f, 3.0f); // 3px thick

        // --- LAYER 4: TEXT (WHITE) ---
        g.setColour(juce::Colours::white);
        g.setFont(14.0f);
        g.drawText(geometry.label, keyBounds, juce::Justification::centred, false);
    }
}
```

**Generate the code for: `Source/VisualizerComponent.cpp`.**