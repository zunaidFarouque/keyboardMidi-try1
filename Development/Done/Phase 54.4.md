### 📋 Cursor Prompt: Phase 54.4 - Visualizer Text Contrast Tuning

**Target File:** `Source/VisualizerComponent.cpp`

**Task:**
Tune the Smart Contrast logic in `refreshCache` to ensure text is legible.
1.  If a key is **Inherited** (dimmed), force **White Text**.
2.  If a key is **Active**, raise the brightness threshold to **0.7** (prefer White text unless background is very bright).

**Specific Instructions:**

1.  **Locate `refreshCache`:** Find the text color calculation block.
2.  **Replace Logic:**

    ```cpp
    // 1. Conflict: Always White on Red
    if (slot.state == VisualState::Conflict) {
        textColor = juce::Colours::white;
    }
    // 2. Inherited (Dim): Always White
    // Alpha 0.3 over dark grey is always dark.
    else if (slot.state == VisualState::Inherited) {
        textColor = juce::Colours::white;
    }
    // 3. Active/Override (Bright): Calculate
    else {
        // Calculate Effective Color (What the user sees)
        // We assume we are drawing over the dark window background or key body
        const juce::Colour bgBase(0xff333333); 
        juce::Colour effective = bgBase.interpolatedWith(backColor, alpha);
        
        // Higher threshold (0.7) ensures we only switch to Black text 
        // for very bright colors (Yellow, Cyan, White).
        // Mid-tones (Teal, Blue, Red) will stick to White text.
        float brightness = effective.getPerceivedBrightness();
        textColor = (brightness > 0.7f) ? juce::Colours::black : juce::Colours::white;
    }
    ```

**Verification:**
After this, your Teal keys (which are mid-brightness) should flip to White text, making them readable. Yellow keys (very bright) will stay Black.