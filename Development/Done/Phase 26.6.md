# 🤖 Cursor Prompt: Phase 26.6 - The Voice Watchdog (Stuck Note Failsafe)

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26.5 Complete. The system is robust, but extreme usage ("spamming") can still result in rare stuck notes due to race conditions between the Input Thread and Audio/Timer threads.
*   **Phase Goal:** Implement a **"Watchdog Timer"** inside `VoiceManager`. This background process periodically checks for logic inconsistencies (e.g., Audio playing but no Keys held) and forcibly corrects them.

**Strict Constraints:**
1.  **Frequency:** Run the check every **100ms**.
2.  **Thread Safety:** The Watchdog runs on the Message Thread. It MUST acquire `monoCriticalSection` before touching the voices or stacks.
3.  **Logic:** Only kill a note if the Stack is Empty AND Global Sustain/Latch are False.

---

### Step 1: Update `VoiceManager.h`
Inherit from `Timer`.

**Updates:**
1.  Inherit `private juce::Timer`.
2.  Override `void timerCallback()`.

### Step 2: Update `VoiceManager.cpp`
Implement the Garbage Collector.

**1. Constructor:**
*   `startTimer(100);` // Start the watchdog.

**2. `timerCallback` Implementation:**
```cpp
void VoiceManager::timerCallback()
{
    // Try to lock. If the Audio thread is busy processing a note, 
    // skip this check to avoid blocking audio. Efficiency first.
    if (monoCriticalSection.tryEnter())
    {
        // 1. Iterate over all active voices
        // Use a standard for-loop with iterator so we can erase efficiently
        for (auto it = voices.begin(); it != voices.end();)
        {
            // Only care about Mono/Legato voices (Poly handles itself)
            // Or apply to all for maximum safety? Let's check logic.
            // If Stack is tracked for this channel, we use Stack logic.
            
            int ch = it->midiChannel;
            bool channelHasStack = (monoStacks.count(ch) > 0);
            
            if (channelHasStack)
            {
                bool stackEmpty = monoStacks[ch].empty();
                bool sustainActive = globalSustainActive && it->allowSustain;
                bool latched = (it->state == NoteState::Latched);

                // ZOMBIE CONDITION:
                // Stack is Empty AND Not Sustained AND Not Latched.
                if (stackEmpty && !sustainActive && !latched)
                {
                    // Kill it.
                    midiEngine.sendNoteOff(ch, it->noteNumber, 0.0f);
                    midiEngine.sendPitchBend(ch, 8192); // Reset PB
                    
                    // Force stop glide if on this channel
                    if (portamentoEngine.getCurrentValue() != 8192) // Optimization?
                        portamentoEngine.stop(); 

                    // Remove from vector
                    it = voices.erase(it);
                    continue; // Loop continues
                }
            }
            
            ++it;
        }
        
        monoCriticalSection.exit();
    }
}
```

### Step 3: Destructor
*   `stopTimer()` in `~VoiceManager()`.

---

**Generate code for: Updated `VoiceManager.h` and `VoiceManager.cpp`.**