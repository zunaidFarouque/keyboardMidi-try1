# 🤖 Cursor Prompt: Phase 4.1 - Enhanced Logging & Optimization

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 4 Complete. The app works, but the "Log Console" is just a basic `TextEditor` in `MainComponent`.
*   **Problem:** The log grows indefinitely (memory leak risk) and slows down the UI. The output text is also unformatted and hard to read.
*   **Phase Goal:** Extract logging into a dedicated, high-performance component and implement rich string formatting.

**Strict Constraints:**
1.  **Performance:** The logger must use a **Ring Buffer** (e.g., `std::deque`) to keep only the last N lines.
2.  **Thread Safety:** UI updates must happen via `juce::MessageManager::callAsync`.
3.  **Architecture:** `MainComponent` should not calculate log strings; it should delegate to the new component.

---

### Step 1: `LogComponent` (`Source/LogComponent.h/cpp`)
Create a dedicated UI component for the console.

**Requirements:**
1.  **Inheritance:** `juce::Component`.
2.  **Members:**
    *   `juce::TextEditor console;` (Read-only, Monospaced Font, No Caret).
    *   `std::deque<juce::String> buffer;` (Limit to 50 lines).
3.  **Methods:**
    *   `void addEntry(const juce::String& text);`
        *   Push to back of deque.
        *   Pop front if size > 50.
        *   Reconstruct string and update `console` asynchronously.
    *   `void clear();`

### Step 2: Update `InputProcessor`
We need to "peek" at the mapping data to log what *would* happen, without actually triggering a note.

**Requirements:**
1.  **Method:** `const MidiAction* getMappingForInput(InputID input);`
2.  **Implementation:**
    *   Use `juce::ScopedReadLock`.
    *   Return the pointer to the `MidiAction` if found, or `nullptr`.

### Step 3: Update `MainComponent`
Refactor the logging logic.

**Requirements:**
1.  **Replace:** Remove the `juce::TextEditor` member; replace with `LogComponent`.
2.  **Logic (`logEvent` method):**
    *   Format the string to align columns: `Dev: [HANDLE] | DOWN | Key: [CODE]`.
    *   Call `inputProcessor.getMappingForInput(...)`.
    *   If a mapping exists, append `-> [MIDI] Note On | 60 (C4)`.
    *   Pass the final string to `logComponent.addEntry()`.

### Step 4: Update `CMakeLists.txt`
Add `Source/LogComponent.cpp` to `target_sources`.