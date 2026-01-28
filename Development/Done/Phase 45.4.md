# 🤖 Cursor Prompt: Fix MappingInspector Slider Ranges

**Role:** Expert C++ Audio Developer (JUCE UI Debugging).

**Context:**
*   **Current State:** The `MappingInspector`'s `data1Slider` (Note/CC Number) is incorrectly capped at range **0 to 8**.
*   **Cause:** The logic for "Layer Commands" (which uses Data1 for Layer ID 0-8) sets the range, but the logic for "Notes/CC" fails to reset it back to **0-127**.
*   **Goal:** Ensure `data1Slider` has the correct range and Text/Value functions based on the current selection.

**Strict Constraints:**
1.  **Refactor `updateVisibility` / `setSelection`:**
    *   If **Note/CC**: Set Range `0-127`. Restore `MidiNoteUtilities` text function (if Note).
    *   If **Layer Command**: Set Range `0-8`. Disable text function (show raw number).
    *   If **Other Command**: Set Range `0-127` (Generic).

---

### Step 1: Update `Source/MappingInspector.cpp`

**Refactor the visibility/update logic (likely inside `setSelection` or a helper):**

```cpp
void MappingInspector::updateVisibility()
{
    // ... existing type retrieval ...
    // ActionType type = ...;
    // int cmdId = ...;

    bool isLayerCmd = (type == ActionType::Command && 
                      (cmdId == CommandID::LayerMomentary || 
                       cmdId == CommandID::LayerToggle || 
                       cmdId == CommandID::LayerSolo));

    // --- FIX: Explicitly Switch Ranges ---

    if (isLayerCmd)
    {
        // Layer Mode: 0-8
        data1Label.setText("Layer ID", juce::dontSendNotification);
        data1Slider.setRange(0, 8, 1);
        data1Slider.textFromValueFunction = nullptr; // Raw numbers
        data1Slider.valueFromTextFunction = nullptr;
    }
    else if (type == ActionType::Note)
    {
        // Note Mode: 0-127 + Smart Text
        data1Label.setText("Note", juce::dontSendNotification);
        data1Slider.setRange(0, 127, 1);
        
        // Re-attach Smart Functions (Phase 8 Logic)
        data1Slider.textFromValueFunction = [](double val) { 
            return MidiNoteUtilities::getMidiNoteName((int)val); 
        };
        data1Slider.valueFromTextFunction = [](const juce::String& text) { 
            return MidiNoteUtilities::getMidiNoteFromText(text); 
        };
    }
    else if (type == ActionType::CC)
    {
        // CC Mode: 0-127 + Raw Text
        data1Label.setText("CC Num", juce::dontSendNotification);
        data1Slider.setRange(0, 127, 1);
        data1Slider.textFromValueFunction = nullptr;
        data1Slider.valueFromTextFunction = nullptr;
    }
    else
    {
        // Fallback / Other Commands
        data1Slider.setRange(0, 127, 1);
        data1Slider.textFromValueFunction = nullptr;
        data1Slider.valueFromTextFunction = nullptr;
    }

    // ... rest of visibility logic ...
}
```

**Generate code for: Updated `Source/MappingInspector.cpp`.**