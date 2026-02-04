# 🤖 Cursor Prompt: Phase 26.4 - Mono/Legato Advanced Retrigger Logic

**Role:** Expert C++ Audio Developer (Real-time Systems).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 26.3 Complete. The Legato engine keeps the anchor alive, but it misses edge cases.
*   **Problem:** If a note (B) overrides the anchor (A) because it was out of PB range, releasing B **silences the synth** instead of retriggering A.
*   **Phase Goal:** Implement a robust "Retrigger vs Glide" decision tree in `handleKeyUp`.

**Strict Constraints:**
1.  **State Logic:** In `handleKeyUp`, if the Mono Stack falls back to a previous note, we must check: **Is the Anchor Note (Audio) currently playing?**
    *   If **No** (it was killed earlier): We MUST send `NoteOn(NewTop)`.
    *   If **Yes**: We check the Pitch Bend range.
        *   If In Range: Glide PB.
        *   If Out of Range: `NoteOff(Anchor)` -> `NoteOn(NewTop)`.

---

### Step 1: Update `VoiceManager.cpp`

**Refactor `handleKeyUp` (The Decision Tree):**

```cpp
void VoiceManager::handleKeyUp(InputID source)
{
    // 1. Find Channel & Remove source from Stack
    // ... (Existing lookup logic) ...
    // auto& stack = monoStacks[channel];
    // ... erase source ...

    // 2. Check Stack Status
    if (stack.empty())
    {
        // CASE: Final Release
        // Kill whatever voice is active on this channel
        for (auto it = voices.begin(); it != voices.end();)
        {
            if (it->midiChannel == channel)
            {
                midiEngine.sendNoteOff(channel, it->noteNumber, 0.0f);
                it = voices.erase(it);
            }
            else ++it;
        }
        // Reset PB
        midiEngine.sendPitchBend(channel, 8192);
        portamentoEngine.stop(); // If needed
    }
    else
    {
        // CASE: Handoff / Retrigger
        // The note that *should* be heard now
        int targetNote = stack.back().first; 
        
        // Find the "Anchor" (The voice actually playing audio on this channel)
        ActiveVoice* anchor = nullptr;
        for (auto& v : voices) {
            if (v.midiChannel == channel) {
                anchor = &v;
                break;
            }
        }

        // Sub-Case A: No Anchor exists (was killed by a non-legato note)
        if (anchor == nullptr)
        {
            // We must RETRIGGER the target note
            // Call internal noteOn logic or helper
            // Note: Ensure we don't duplicate logic, maybe call noteOn directly
            // with a flag to bypass stack pushing? 
            // Actually, safest is to send MIDI NoteOn manually and add to voices vector manually here.
            midiEngine.sendNoteOn(channel, targetNote, 100.0f); // Use stored velocity if possible
            voices.push_back({targetNote, channel, stack.back().second, ...});
            
            // Reset PB to center for the new note
            midiEngine.sendPitchBend(channel, 8192);
        }
        // Sub-Case B: Anchor exists
        else
        {
            int currentRoot = anchor->noteNumber;
            int delta = targetNote - currentRoot;
            
            // Check PB Range (using lookup)
            int pbTarget = pbLookup[delta + 127]; // Offset for array index
            
            if (pbTarget != -1) 
            {
                // GLIDE (Ghost Anchor)
                // Keep Anchor alive. Just move PB.
                int startVal = portamentoEngine.getCurrentValue();
                // Get speed from Zone logic (or use default 50ms if not accessible)
                portamentoEngine.startGlide(startVal, pbTarget, 50, channel);
            }
            else
            {
                // HARD SWITCH (Retrigger)
                // Range too far. Kill Anchor. Start Target.
                midiEngine.sendNoteOff(channel, currentRoot, 0.0f);
                // Remove old anchor from voices
                // Add new voice...
                // (Similar to Sub-Case A)
            }
        }
    }
}
```

*Note to Cursor:* Ensure `voices` vector is managed carefully (iterators invalidated upon erase).

---

**Generate code for: Updated `VoiceManager.cpp`.**