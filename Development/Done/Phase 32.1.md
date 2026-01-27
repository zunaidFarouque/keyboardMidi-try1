# 🤖 Cursor Prompt: Phase 32.1 - Fix Shutdown Crash (Safe Async Logging)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
*   **Current State:** The app crashes on shutdown with `_CrtIsValidHeapPointer`.
*   **Diagnosis:** `MainComponent::log` and `LogComponent::addEntry` use `juce::MessageManager::callAsync` capturing `this`. When the app closes, destructors trigger logs, but the object is destroyed before the async message is processed, leading to a Use-After-Free.
*   **Phase Goal:** Make all async UI updates "Death-Aware" using `SafePointer`.

**Strict Constraints:**
1.  **Safety:** In any `callAsync` lambda that accesses class members, capture `juce::Component::SafePointer<MyClass> safeThis(this);`.
2.  **Check:** Inside the lambda, check `if (safeThis)` before accessing anything.

---

### Step 1: Update `Source/LogComponent.cpp`
Refactor `addEntry` to be safe.

```cpp
void LogComponent::addEntry(const juce::String& text)
{
    // Create a SafePointer to 'this'
    // If LogComponent is deleted, safeThis becomes null automatically.
    juce::Component::SafePointer<LogComponent> safeThis(this);

    juce::MessageManager::callAsync([safeThis, text]()
    {
        // Check if we are still alive
        if (safeThis == nullptr) return;

        // Use safeThis-> instead of implicit this->
        safeThis->logBuffer.push_back(text);

        while (safeThis->logBuffer.size() > safeThis->maxLines)
        {
            safeThis->logBuffer.pop_front();
        }

        juce::String combinedText;
        for (const auto& line : safeThis->logBuffer)
        {
            combinedText += line + "\n";
        }

        safeThis->console.setText(combinedText, false);
        safeThis->console.moveCaretToEnd();
    });
}
```

### Step 2: Update `Source/MainComponent.cpp`
Refactor `log` (and any other async UI callbacks like `timerCallback` if they use async internally).

```cpp
void MainComponent::log(const juce::String& text)
{
    juce::Component::SafePointer<MainComponent> safeThis(this);

    juce::MessageManager::callAsync([safeThis, text]()
    {
        if (safeThis == nullptr) return;
        
        safeThis->logComponent.addEntry(text);
    });
}
```

### Step 3: Check `VisualizerComponent.cpp`
Does it use `callAsync`?
*   It uses `VBlankAttachment`. This is automatically safe because the attachment is destroyed in the destructor.
*   It uses `needsRepaint` atomic. Safe.
*   If `handleRawKeyEvent` calls `repaint` directly (it shouldn't anymore, but check), update it.

---

**Generate code for: Updated `Source/LogComponent.cpp` and Updated `Source/MainComponent.cpp`.**