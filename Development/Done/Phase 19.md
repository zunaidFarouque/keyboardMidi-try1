# 🤖 Cursor Prompt: Phase 19 - Visualizing Performance States (HUD Upgrade)

**Role:** Expert C++ Audio Developer (JUCE Framework & Graphics).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18 Complete. `VoiceManager` maintains complex note states (`Playing`, `Sustained`, `Latched`), but the Visualizer only shows physical key presses.
*   **Phase Goal:** Update the Visualizer to show a global "Sustain" indicator and colorize specific keys that are currently "Latched" (droning).

**Strict Constraints:**
1.  **Dependency:** `VisualizerComponent` must hold a reference to `const VoiceManager&`.
2.  **Layering:** The "Latched" color (Cyan) must be drawn **after** the Zone Background but **before** the Physical Press (Yellow).
3.  **Lookup:** Since the Visualizer displays a generic keyboard, `VoiceManager` needs a helper to check if *any* device has latched a specific Key Code.

---

### Step 1: Update `VoiceManager` (`Source/VoiceManager.h/cpp`)
Add accessors for the UI.

**Requirements:**
1.  **Method:** `bool isSustainActive() const { return globalSustainActive; }`
2.  **Method:** `bool isKeyLatched(int keyCode) const;`
    *   **Logic:** Iterate through the `voices` vector.
    *   If `voice.source.keyCode == keyCode` AND `voice.state == NoteState::Latched`: Return `true`.
    *   Else: Return `false`.

### Step 2: Update `VisualizerComponent` (`Source/VisualizerComponent.h/cpp`)
Connect and Draw.

**1. Header:**
*   Add member: `const VoiceManager& voiceManager;`
*   Update Constructor: Accept `const VoiceManager& vm` and initialize the member.

**2. `paint` Method (Refactor):**
*   **Header Bar:**
    *   Define a top strip (e.g., `headerRect = bounds.removeFromTop(30)`).
    *   Draw background (`0xff222222`).
    *   **Sustain Indicator:**
        *   Draw a small circle/rect on the right side of the header.
        *   Color: `voiceManager.isSustainActive() ? Colours::lime : Colours::grey`.
        *   Label: "SUSTAIN".
*   **Key Loop (Layer Update):**
    *   ... (Zone Underlay drawing) ...
    *   **Layer 1.5 (Latched State):**
        *   `if (voiceManager.isKeyLatched(keyCode))`:
        *   `g.setColour(juce::Colours::cyan.withAlpha(0.8f));`
        *   `g.fillRoundedRectangle(keyBounds, ...);`
    *   ... (Physical Press drawing - Overwrites Latch) ...
    *   ... (Border/Text drawing) ...

### Step 3: Update `MainComponent` (`Source/MainComponent.cpp`)
Pass the reference.

**Constructor:**
*   Update `visualizer` initialization to pass `voiceManager`.
    *   *Note:* Ensure `voiceManager` is declared *before* `visualizer` in `MainComponent.h` so it is initialized first.

---

**Generate code for: Updated `VoiceManager.h`, Updated `VoiceManager.cpp`, Updated `VisualizerComponent.h`, Updated `VisualizerComponent.cpp`, and Updated `MainComponent.cpp`.**