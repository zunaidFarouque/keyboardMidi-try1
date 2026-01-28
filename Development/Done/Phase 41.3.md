# 🤖 Cursor Prompt: Phase 41.3 - Fix Pointer Safety (Value Semantics)

**Role:** Expert C++ Audio Developer (Debugging).

**Context:**
*   **Current State:** The app crashes randomly on startup/shutdown.
*   **Diagnosis:** `InputProcessor::getMappingForInput` returns a raw pointer (`const MidiAction*`) protected by a lock *only during the lookup*. Once returned, the pointer is unprotected. If `rebuildMap` runs (modifying the underlying map), the pointer becomes a "Dangling Pointer", causing Heap Corruption.
*   **Phase Goal:** Change the API to return `std::optional<MidiAction>` (Copy by Value).

**Strict Constraints:**
1.  **API Change:** `getMappingForInput` must return `std::optional<MidiAction>`.
2.  **Implementation:** Inside the function, dereference the pointer and return a copy of the object inside the lock scope.
3.  **Call Sites:** Update `MainComponent::logEvent` (and `Visualizer` if it uses this method) to handle the `optional` instead of checking for `nullptr`.

---

### Step 1: Update `Source/InputProcessor.h`
Change the signature.

```cpp
// Old: const MidiAction* getMappingForInput(InputID input);
// New:
std::optional<MidiAction> getMappingForInput(InputID input);
```

### Step 2: Update `Source/InputProcessor.cpp`
Refactor the implementation.

```cpp
std::optional<MidiAction> InputProcessor::getMappingForInput(InputID input)
{
    juce::ScopedReadLock lock(mapLock);
    
    // We still use findMapping internally (which might return raw pointer or iterator)
    const MidiAction* ptr = findMapping(input);
    
    if (ptr != nullptr)
    {
        // RETURN A COPY while lock is held
        return *ptr;
    }
    
    return std::nullopt;
}
```

### Step 3: Update `Source/MainComponent.cpp`
Update the logger to use the optional.

**Refactor `logEvent`:**
```cpp
void MainComponent::logEvent(uintptr_t device, int keyCode, bool isDown)
{
    // ... string formatting ...

    // NEW LOGIC
    InputID id = { device, keyCode };
    auto actionOpt = inputProcessor.getMappingForInput(id);

    if (actionOpt.has_value()) // Check optional
    {
        const auto& action = *actionOpt; // Access copy
        
        logLine += " -> [MIDI] ";
        // ... access action.type, action.data1 etc ...
    }

    // ...
}
```

---

**Generate code for: Updated `InputProcessor.h`, Updated `InputProcessor.cpp`, and Updated `MainComponent.cpp`.**