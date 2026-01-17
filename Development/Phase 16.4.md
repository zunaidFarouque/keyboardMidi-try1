
# 🤖 Cursor Prompt: Phase 16.4 - Visualizer Polish & Logic Restoration

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
*   **Current State:** The Visualizer correctly renders 4 layers (Underlay/Body/Border/Text), but the layout is tiny/broken, and it is currently hardcoded to "Debug Colors" (Red/Blue).
*   **Phase Goal:** Fix the scaling math to fill the screen and reconnect the `ZoneManager` to show real data.

**Strict Constraints:**
1.  **Scaling:** The keyboard standard width is approx **15.5 units** (Row 1) and height is **5 units**. Calculate `keySize` dynamically to fit `getWidth()` and `getHeight()`.
2.  **Layering:** Keep the 4-layer draw order established in Phase 16.3.
3.  **Data:** Use `zoneManager` to determine colors and text.

---

### Step 1: Update `VisualizerComponent.cpp`

**Refactor `paint()`:**

```cpp
void VisualizerComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff111111)); // Background

    // --- 1. Calculate Dynamic Scale ---
    // Standard ANSI layout is roughly 15.5 keys wide and 5 keys tall.
    float unitsWide = 15.5f;
    float unitsTall = 5.5f;
    
    // Determine the maximum size a key can be while fitting in the component
    float scaleX = (float)getWidth() / unitsWide;
    float scaleY = (float)getHeight() / unitsTall;
    float keySize = std::min(scaleX, scaleY) * 0.9f; // 0.9f for margin
    
    // Center the keyboard
    float totalWidth = unitsWide * keySize;
    float totalHeight = unitsTall * keySize;
    float startX = (getWidth() - totalWidth) / 2.0f;
    float startY = (getHeight() - totalHeight) / 2.0f;

    // --- 2. Iterate Keys ---
    for (auto const& [keyCode, geometry] : KeyboardLayoutUtils::getLayout())
    {
        // Calculate Position relative to start
        float x = startX + (geometry.col * keySize);
        float y = startY + (geometry.row * keySize);
        float w = geometry.width * keySize;
        float h = keySize;

        juce::Rectangle<float> fullBounds(x, y, w, h);
        
        // Define Inner Body (Padding)
        float padding = keySize * 0.1f; // 10% padding looks good at any size
        auto keyBounds = fullBounds.reduced(padding);

        // --- 3. Get Data from Engine ---
        // A. Zone Underlay Color
        juce::Colour underlayColor = juce::Colours::transparentBlack;
        // Call zoneManager->getZoneColorForKey(keyCode, deviceHash)... 
        // For now, use a helper or assume master device 0
        if (auto zoneColor = zoneManager.getZoneColorForKey(keyCode, 0))
            underlayColor = *zoneColor;

        // B. Key State
        bool isPressed = activeKeys.contains(keyCode);
        
        // C. Text/Label
        juce::String labelText = geometry.label; // Default: "Q"
        if (auto result = zoneManager.simulateInput(keyCode, 0))
        {
            if (result->type == ActionType::Note)
                labelText = MidiNoteUtilities::getMidiNoteName(result->data1);
        }

        // --- 4. Render Layers ---
        
        // Layer 1: Underlay (Zone Color)
        if (!underlayColor.isTransparent())
        {
            g.setColour(underlayColor.withAlpha(0.6f));
            g.fillRoundedRectangle(fullBounds, 4.0f);
        }

        // Layer 2: Key Body
        g.setColour(isPressed ? juce::Colours::yellow : juce::Colour(0xff333333));
        g.fillRoundedRectangle(keyBounds, 6.0f);

        // Layer 3: Border
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(keyBounds, 6.0f, 2.0f);

        // Layer 4: Text
        g.setColour(isPressed ? juce::Colours::black : juce::Colours::white);
        g.setFont(keySize * 0.4f); // Dynamic font size
        g.drawText(labelText, keyBounds, juce::Justification::centred, false);
    }
}
```

**Generate code for: `Source/VisualizerComponent.cpp`.**