# 🤖 Cursor Prompt: Phase 39.5 - Fix Master View Colors

**Role:** Expert C++ Audio Developer (JUCE Graphics).

**Context:**
*   **Current State:** Phase 39.4 Complete.
*   **The Bug:** In the Visualizer, selecting "Any / Master" shows text labels (Note names) but **no colors** (Underlays are transparent).
*   **Contrast:** Selecting "Laptop" correctly shows Master mappings as "Inherited" (Dimmed colors).
*   **Diagnosis:** The `refreshCache` loop likely handles `VisualState::Inherited` correctly but fails to assign a color for `VisualState::Active` when `currentViewHash == 0`.

**Phase Goal:** Fix the color selection logic in `VisualizerComponent::refreshCache` to ensure Master mappings are colored.

**Strict Constraints:**
1.  **Logic:**
    *   If `result.state == VisualState::Active`: Use Full Opacity.
    *   If `result.state == VisualState::Inherited`: Use Low Opacity.
    *   If `result.state == VisualState::Override`: Use Full Opacity + Orange Border.
2.  **Color Source:**
    *   If `result.isZone`: Call `zoneManager.getZoneColorForKey(keyCode, currentViewHash)`.
        *   *Crucial Check:* If `currentViewHash` is 0, this returns the Master Zone color.
        *   *Backup:* If that returns nullopt (which shouldn't happen if text exists), try checking Global (0) explicitly if we are in a Specific View... but for Master view, 0 is correct.
    *   If `!result.isZone` (Manual): Call `settingsManager.getTypeColor(result.action->type)`.

---

### Step 1: Update `VisualizerComponent.cpp`

**Refactor `refreshCache`:**

Focus on the Color Determination block inside the loop.

```cpp
// ... inside loop ...

auto result = inputProcessor.simulateInput(currentViewHash, keyCode);
juce::Colour underlayColor = juce::Colours::transparentBlack;
juce::Colour borderColor = juce::Colours::grey;
float alpha = 1.0f;

if (result.state != VisualState::Empty)
{
    // 1. Determine Base Color
    if (result.isZone)
    {
        // Try to get color for the current view
        auto zColor = zoneManager.getZoneColorForKey(keyCode, currentViewHash);
        
        // If not found (e.g., we are viewing a specific device, but the zone is Global/Inherited),
        // we must check the Global context (0) to get the color of the inherited zone.
        if (!zColor.has_value() && result.state == VisualState::Inherited)
        {
            zColor = zoneManager.getZoneColorForKey(keyCode, 0);
        }

        if (zColor.has_value())
            underlayColor = *zColor;
    }
    else if (result.action.has_value()) // Manual Mapping
    {
        underlayColor = settingsManager.getTypeColor(result.action->type);
    }

    // 2. Determine Alpha / Style
    if (result.state == VisualState::Inherited)
    {
        alpha = 0.3f; // Dim for inherited
        borderColor = juce::Colours::darkgrey;
    }
    else if (result.state == VisualState::Override)
    {
        alpha = 1.0f;
        borderColor = juce::Colours::orange; // Highlight override
    }
    else if (result.state == VisualState::Active)
    {
        alpha = 1.0f;
        borderColor = juce::Colours::lightgrey;
    }
    
    // Apply Alpha
    underlayColor = underlayColor.withAlpha(alpha);
}

// ... Drawing calls ...
// g.setColour(underlayColor);
// g.fillRect(fullBounds); 
// ...
```

---

**Generate code for: Updated `Source/VisualizerComponent.cpp`.**