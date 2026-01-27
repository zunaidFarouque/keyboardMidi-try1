# 🤖 Cursor Prompt: Phase 31.1 - Shutdown Hardening (Destructors)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Problem:** The app crashes with `_CrtIsValidHeapPointer` (Heap Corruption) upon closing.
*   **Diagnosis:** Active timers (HighResolutionTimer) or Listeners are firing callbacks *during* the destruction of their owning objects. Reordering `MainComponent` was step 1, but the classes themselves need explicit shutdown logic.

**Phase Goal:** Add/Update destructors for all active classes to ensure they stop all threads and unregister callbacks immediately.

**Strict Constraints:**
1.  **Timers:** Any class inheriting `Timer` or `HighResolutionTimer` MUST call `stopTimer()` in its destructor.
2.  **Listeners:** Any class observing another MUST call `removeListener` in its destructor.
3.  **Visualizer:** Explicitly set `vBlankAttachment = nullptr` in the destructor to stop the render loop before members are destroyed.

---

### Step 1: Update `Source/VoiceManager.cpp`
Ensure both timers are stopped.

```cpp
VoiceManager::~VoiceManager()
{
    // 1. Stop Watchdog (juce::Timer)
    stopTimer(); 
    
    // 2. Stop Release Envelopes (juce::HighResolutionTimer)
    juce::HighResolutionTimer::stopTimer();
    
    // 3. Clear data
    {
        const juce::ScopedLock sl(monoCriticalSection);
        voices.clear();
        activeNoteNumbers.clear();
    }
}
```

### Step 2: Update `Source/ExpressionEngine.cpp`
Stop the ADSR engine.

```cpp
ExpressionEngine::~ExpressionEngine()
{
    stopTimer(); // HighResolutionTimer
    
    const juce::ScopedLock sl(lock);
    activeEnvelopes.clear();
}
```

### Step 3: Update `Source/StrumEngine.cpp` & `PortamentoEngine.cpp`
Stop the timing engines.

*   **StrumEngine:** `~StrumEngine() { stopTimer(); }`
*   **PortamentoEngine:** `~PortamentoEngine() { stopTimer(); }`

### Step 4: Update `Source/VisualizerComponent.cpp`
Stop the render loop.

```cpp
VisualizerComponent::~VisualizerComponent()
{
    // Stop callbacks immediately
    vBlankAttachment = nullptr; 
    
    // Unregister listeners
    if (rawInputManager) rawInputManager->removeListener(this);
    zoneManager.removeChangeListener(this);
    // VoiceManager doesn't have a listener interface, it's polled.
}
```

### Step 5: Update `Source/InputProcessor.cpp`
Unregister from everything.

```cpp
InputProcessor::~InputProcessor()
{
    // Stop listening to prevent callbacks while destroying
    presetManager.getRootNode().removeListener(this);
    deviceManager.removeChangeListener(this);
    zoneManager.removeChangeListener(this);
    settingsManager.removeChangeListener(this);
}
```

---

**Generate code for: Updated destructors in `VoiceManager.cpp`, `ExpressionEngine.cpp`, `StrumEngine.cpp`, `PortamentoEngine.cpp`, `VisualizerComponent.cpp`, and `InputProcessor.cpp`.**