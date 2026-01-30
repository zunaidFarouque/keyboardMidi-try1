### 📋 Cursor Prompt: Phase 52.3 - Wiring the Logger

**Target File:** `Source/MainComponent.cpp`

**Task:**
Update the logging logic in `MainComponent` to use the new Grid system. Replace the old `simulateInput` call with a direct lookup into the `CompiledMapContext`.

**Specific Instructions:**

1.  **Refactor `logEvent`:**
    *   **Remove:** Calls to `inputProcessor.simulateInput`.
    *   **New Logic:**
        1.  **Get Context:** `auto ctx = inputProcessor.getContext();` (Check for null).
        2.  **Determine Scope:**
            *   Get Device Hash from `device` handle.
            *   Get Active Layer: `int layer = inputProcessor.getHighestActiveLayerIndex();`
        3.  **Lookup Visual Slot:**
            *   We use the **Visual Grid** because it already contains the pre-calculated strings (`label` and `sourceName`).
            *   Try `ctx->visualLookup[deviceHash][layer]`.
            *   Fallback: `ctx->visualLookup[0][layer]` (Global).
        4.  **Read Slot:**
            *   `const auto& slot = (*grid)[keyCode];`
        5.  **Format String:**
            *   Start with input info: `"Dev: ... | Key: ..."`
            *   If `slot.state != VisualState::Empty`:
                *   Append `" -> [MIDI] " + slot.label`.
                *   Append `" | Source: " + slot.sourceName`.
                *   If `slot.state == VisualState::Override`, append `" [OVERRIDE]"`.
                *   If `slot.state == VisualState::Conflict`, append `" [CONFLICT]"`.

**Why this works:**
Because our Compiler (Phase 51.4) implemented "Vertical Stacking" (inheritance), the Grid at `[layer]` already contains the correct data, even if it was defined in Layer 0. We don't need to loop through layers manually in the Logger.

**Verification:**
After this, pressing a key should print a log line that matches exactly what the Visualizer shows (same text, same source).