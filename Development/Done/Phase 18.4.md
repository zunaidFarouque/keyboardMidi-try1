# 🤖 Cursor Prompt: Phase 18.4 - Velocity Randomization (Humanization)

**Role:** Expert C++ Audio Developer (JUCE Framework).

**Context:**
We are building "MIDIQy".
*   **Current State:** Phase 18.3 Complete. Chords are voiced beautifully, but velocity is static (robotic).
*   **Phase Goal:** Implement **Velocity Randomization**.
*   **Logic:** `FinalVelocity = BaseVelocity + Random(-Range, +Range)`.

**Strict Constraints:**
1.  **Calculation Time:** Calculate randomness inside `InputProcessor::processEvent` (Runtime). Do NOT compile it into the cache (otherwise every key press would have the *same* "random" value until settings changed).
2.  **Clamping:** Ensure result is always between 1 and 127.
3.  **Serialization:** Save/Load these new settings in XML.

---

### Step 1: Update Data Structures

**1. `Source/MappingTypes.h` (`MidiAction`):**
*   Add member: `int velocityRandom = 0;`
*   *Note:* `data2` holds the Base Velocity.

**2. `Source/Zone.h` (`Zone`):**
*   Add members: `int baseVelocity = 100;`, `int velocityRandom = 0;`
*   Update `toValueTree`: Save properties `"baseVel"` and `"randVel"`.
*   Update `fromValueTree`: Load properties (Default 100 and 0).

### Step 2: Update `InputProcessor` (`Source/InputProcessor.h/cpp`)
Implement the runtime math.

**1. Header:**
*   Add `juce::Random random;` as a private member.

**2. `addMappingFromTree` (XML Loader):**
*   Read `"velRandom"` property from XML (Default 0).
*   Store in `MidiAction.velocityRandom`.

**3. New Helper:** `int calculateVelocity(int base, int range);`
*   `int delta = (range > 0) ? random.nextInt(range * 2 + 1) - range : 0;`
*   `return juce::jlimit(1, 127, base + delta);`

**4. `processEvent` (Runtime):**
*   **For Zones:**
    *   Calculate `vel = calculateVelocity(zone->baseVelocity, zone->velocityRandom)`.
    *   Pass `vel` to `voiceManager.noteOn`.
*   **For Mappings:**
    *   Calculate `vel = calculateVelocity(action.data2, action.velocityRandom)`.
    *   Pass `vel` to `voiceManager.noteOn`.

### Step 3: Update UI Panels

**1. `Source/ZonePropertiesPanel.cpp`:**
*   Add `juce::Slider baseVelSlider`, `juce::Slider randVelSlider`.
*   **Ranges:** Base (1-127), Random (0-64).
*   **Logic:** Update `currentZone` members. Trigger no rebuild (runtime only change).

**2. `Source/MappingInspector.cpp`:**
*   Add `juce::Slider randVelSlider`.
*   **Logic:** Visible only if `Type == Note`.
*   **Callback:** Write to `"velRandom"` property in ValueTree.

---

**Generate code for: `MappingTypes.h`, `Zone.h/cpp` (serialization updates), `InputProcessor.h/cpp`, `ZonePropertiesPanel.h/cpp`, and `MappingInspector.h/cpp`.**