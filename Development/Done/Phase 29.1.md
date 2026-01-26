# 🤖 Cursor Prompt: Phase 29.1 - Mini Window UI Tweak

**Role:** Expert C++ Audio Developer (JUCE UI).

**Context:**
*   **Current State:** Phase 29 Complete. `MiniStatusWindow` exists.
*   **Goal:** Remove the "Minimize" and "Maximize" buttons from the Mini Window title bar. It should only have a "Close" button.

**Strict Constraints:**
1.  **Code Change:** Modify the `MiniStatusWindow` constructor initialization list.
2.  **Flags:** Change `juce::DocumentWindow::allButtons` to `juce::DocumentWindow::closeButton`.

---

### Step 1: Update `Source/MiniStatusWindow.cpp`

**Refactor Constructor:**
Change the `requiredButtons` argument in the initializer list.

```cpp
MiniStatusWindow::MiniStatusWindow()
    : DocumentWindow("OmniKey Status",
                     juce::Colours::black, // Background color
                     juce::DocumentWindow::closeButton) // <--- CHANGE THIS (Was allButtons)
{
    // ... existing content setup ...
    // Keep setResizable(true, false) if you want it resizeable, 
    // or setResizable(false, false) if it should be fixed size.
    // Let's keep it resizeable but without the Maximize button.
}
```

---

**Generate code for: Updated `Source/MiniStatusWindow.cpp`.**