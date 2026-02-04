# 🤖 Cursor Prompt: Phase 23.7.1 - Mono Retrigger Bug Fixes

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 23.7 Complete. Mono mode has retrigger logic, but two bugs exist:
    1. **Bug 1:** When releasing the retriggered last note, NoteOff is not sent (because dummy source doesn't match).
    2. **Bug 2:** When releasing a background key (A) while another key (B) is overriding it, A remains in the mono stack and gets incorrectly retriggered when B is released.

**Phase Goal:** Fix both bugs in Mono mode retrigger behavior.

---

## Bug 1: Retriggered Note Doesn't Send NoteOff

**Problem:**
When a note is retriggered from the mono stack (e.g., release B, retrigger A), the voice is added with a dummy source `{0, 0}`. When the user releases the actual key (A), `handleKeyUp` receives `source = A`, but the voice has `dummySource`, so no match is found and NoteOff is never sent.

**Fix:**
1. Add `getMonoStackTopWithSource()` method to `VoiceManager` that returns both the note number AND the original `InputID` from the stack.
2. When retriggering a note from the stack, use the **original source** from the stack entry instead of a dummy source.
3. This ensures `handleKeyUp` can find and properly release the retriggered note.

---

## Bug 2: Background Key Release Doesn't Clean Stack

**Problem:**
When releasing a background key (A) while another key (B) is overriding it:
- A has no active voice (it was NoteOff'd when B took over).
- `handleKeyUp(A)` doesn't find any matching voice, so `releasedChannel = -1`.
- Because `releasedChannel < 0`, the mono stack cleanup logic never runs.
- A remains in the mono stack.
- When B is released later, A is incorrectly retriggered even though it was already released.

**Fix:**
Add cleanup logic in `handleKeyUp` **before** the mono/legato handling:
- If `releasedChannel < 0` (no active voice found):
  - Search **all** `monoStacks` for any entry matching this `source`.
  - If found, remove it from the stack silently (no NoteOn/NoteOff, no PB changes).
  - This handles background key releases that have no active voice.

---

### Step 1: Update `VoiceManager.h`

Add helper method to get stack top with source:

```cpp
// Mono/Legato stack management
void pushToMonoStack(int channel, int note, InputID source);
void removeFromMonoStack(int channel, InputID source);
int getMonoStackTop(int channel) const; // Returns -1 if empty
std::pair<int, InputID> getMonoStackTopWithSource(int channel) const; // Returns {-1, {0,0}} if empty
```

### Step 2: Update `VoiceManager.cpp`

**1. Implement `getMonoStackTopWithSource()`:**
```cpp
std::pair<int, InputID> VoiceManager::getMonoStackTopWithSource(int channel) const {
  juce::ScopedLock lock(monoStackLock);
  auto it = monoStacks.find(channel);
  if (it != monoStacks.end() && !it->second.empty()) {
    return it->second.back(); // Returns {note, source}
  }
  return {-1, {0, 0}}; // Stack empty
}
```

**2. Fix Bug 1 - Use original source when retriggering:**
In `handleKeyUp`, when retriggering a previous note:
- Change from: `getMonoStackTop(releasedChannel)` to `getMonoStackTopWithSource(releasedChannel)`.
- Change from: `InputID dummySource = {0, 0};` to using `previousSource` from the stack.
- When adding voice: `voices.push_back({previousNote, releasedChannel, previousSource, ...})`.

**3. Fix Bug 2 - Clean background releases:**
In `handleKeyUp`, **before** the mono/legato handling block:
```cpp
// If this key had no active voice (e.g. it was overridden/retriggered),
// we may still have it in the mono stack. In that case it's a background
// key release and should simply be removed from the stack without
// affecting the currently driving note.
if (releasedChannel < 0) {
  juce::ScopedLock monoLock(monoStackLock);
  for (auto it = monoStacks.begin(); it != monoStacks.end();) {
    auto &stack = it->second;
    auto beforeSize = stack.size();
    stack.erase(std::remove_if(stack.begin(), stack.end(),
                               [source](const std::pair<int, InputID> &entry) {
                                 return entry.second == source;
                               }),
                stack.end());
    if (stack.empty()) {
      it = monoStacks.erase(it);
    } else {
      ++it;
    }
    // If we removed at least one occurrence, we're done
    if (beforeSize != stack.size())
      break;
  }
}
```

---

**Expected Behavior After Fix:**

**Scenario 1: Retriggered Note Release**
- Press A → NoteOn A
- Press B → NoteOff A, NoteOn B
- Release B → NoteOff B, NoteOn A (retriggered with A's original source)
- Release A → **NoteOff A** ✓ (now works because voice has A's source)

**Scenario 2: Background Key Release**
- Press A → NoteOn A
- Press B → NoteOff A, NoteOn B
- Release A (while B held) → **No sound change, A removed from stack** ✓
- Release B → NoteOff B, **no retrigger** ✓ (stack is empty)

---

**Generate code for: Updated `VoiceManager.h` and `VoiceManager.cpp`.**
