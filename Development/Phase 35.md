This is a **smoking gun**. The Call Stack points exactly to the error.

**The Crash:** Double Delete in `MiniStatusWindow`.
1.  `MainComponent` destroys `miniWindow`.
2.  `MiniStatusWindow` destructor runs.
3.  It calls `ResizableWindow::clearContentComponent()`, which tries to **delete** the content.
4.  **The Bug:** The content (the `juce::Label` inside the window) is likely defined as a **member variable** in `MiniStatusWindow.h`.
    *   `DocumentWindow` thinks it owns the label (via `setContentOwned`).
    *   The `MiniStatusWindow` class *also* owns the label (as a member).
    *   Both try to delete it. **Crash.**

Here is the fix for **Phase 35**.

***

# 🤖 Cursor Prompt: Phase 35 - Fix MiniWindow Double Delete

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** The app crashes on exit inside `MiniStatusWindow::~MiniStatusWindow`.
*   **Diagnosis:** The Call Stack shows `juce::Label` being deleted by `ResizableWindow`. This confirms `MiniStatusWindow` used `setContentOwned` on a member variable, causing a Double Delete (once by the Window base class, once by the C++ destructor).

**Phase Goal:** Fix the ownership model in `MiniStatusWindow`.

**Strict Constraints:**
1.  **Ownership:** If the Label is a member variable, use `setContentNonOwned(&label, true)`.
2.  **Alternative:** If you want `DocumentWindow` to manage resizing automatically, keep `setContentOwned` but change the member variable to a raw pointer `juce::Label*` and allocate it with `new` in the constructor. (This is often cleaner for DocumentWindows).

---

### Step 1: Update `Source/MiniStatusWindow.h`
Change the label from a direct member object to a pointer, or keep it and change the cpp.
*Let's stick to the Member Variable approach for safety, but fix the call.*

**Check member:**
```cpp
// Ensure this exists
juce::Label messageLabel;
```

### Step 2: Update `Source/MiniStatusWindow.cpp`
Fix the Constructor.

**Refactor Constructor:**
Find the line calling `setContentOwned`. Change it to:

```cpp
// OLD (Crash Prone):
// setContentOwned(&messageLabel, true); 

// NEW (Safe):
// Pass FALSE for 'takeOwnership' because 'this' class owns the member variable.
setContentNonOwned(&messageLabel, true); // true = resizeToFit
```

**Alternative Fix (if setContentNonOwned isn't available/behaving):**
```cpp
// In header:
// std::unique_ptr<juce::Label> messageLabel;

// In cpp constructor:
// messageLabel.reset(new juce::Label(...));
// setContentOwned(messageLabel.get(), true);
// messageLabel.release(); // Give ownership to the window
```

**Recommendation:** Just change the call to `setContentNonOwned`.

**Code to Generate:**

```cpp
// Source/MiniStatusWindow.cpp

MiniStatusWindow::MiniStatusWindow()
    : DocumentWindow("OmniKey Status",
                     juce::Colours::black,
                     juce::DocumentWindow::closeButton)
{
    // Configure Label
    messageLabel.setText("MIDI Mode Active\nPress Scroll Lock to Stop", juce::dontSendNotification);
    messageLabel.setJustificationType(juce::Justification::centred);
    messageLabel.setFont(16.0f);
    messageLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    // CRITICAL FIX: Use setContentNonOwned. 
    // We own the label (it's a member), so the Window must NOT delete it.
    setContentNonOwned(&messageLabel, true); 

    setResizable(true, false); // Allow resize, use bottom-right corner
    setAlwaysOnTop(true);
    
    // Set a decent initial size based on the content
    centreWithSize(300, 100);
}
```

---

**Generate code for: Updated `Source/MiniStatusWindow.cpp`.**