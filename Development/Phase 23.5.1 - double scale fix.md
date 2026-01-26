This is a "Double Scaling" bug.

In Phase 23.4, we treated the input (Data2) as `0-127` and multiplied it by `128` inside the engine to get `16383`.
In Phase 23.5, we changed the input (Data2) to be the **Full Raw Value** (e.g., `9000`).

**The Bug:** The `ExpressionEngine` is likely taking that `9000` and multiplying it by `128` again, resulting in a huge number that gets clamped to `16383` (Max Pitch Up).

Here is the fix to remove the legacy scaling.

### Fix: Update `Source/ExpressionEngine.cpp`

Find the `hiResTimerCallback` function. You need to remove the check that multiplies by 128. Since `data2` now comes fully pre-calculated from the Inspector/InputProcessor, we treat it raw.

**Replace the calculation block with this:**

```cpp
// ... inside hiResTimerCallback loop ...

// 1. Determine Zero Point (Start/End value)
int zeroPoint = 0;
if (envelope.settings.target == AdsrTarget::PitchBend)
{
    zeroPoint = 8192; // Center for PB
}

// 2. Determine Range
// envelope.peakValue now holds the exact 14-bit target (e.g. 9500) calculated by the UI/Processor
double range = (double)envelope.peakValue - zeroPoint;

// 3. Calculate Output based on ADSR Level (0.0 to 1.0)
// If Level is 0, we get ZeroPoint. If Level is 1, we get Peak.
int outputVal = zeroPoint + (int)(envelope.currentLevel * range);

// 4. Safety Clamp
if (envelope.settings.target == AdsrTarget::PitchBend)
{
    outputVal = juce::jlimit(0, 16383, outputVal);
}
else
{
    outputVal = juce::jlimit(0, 127, outputVal);
}

// 5. Delta Check & Send
if (outputVal != envelope.lastSentValue)
{
    if (envelope.settings.target == AdsrTarget::PitchBend)
        midiEngine.sendPitchBend(envelope.channel, outputVal);
    else if (envelope.settings.target == AdsrTarget::Aftertouch)
        midiEngine.sendChannelPressure(envelope.channel, outputVal);
    else
        midiEngine.sendCC(envelope.channel, envelope.settings.targetCC, outputVal);

    envelope.lastSentValue = outputVal;
}
```

### Verification
1.  **Clean Build.**
2.  Open **Mapping Inspector**.
3.  Set **Synth Range** to `12` (Standard).
4.  Set **Shift** to `0`.
    *   Math: `8192 + (0 * steps) = 8192`.
    *   Result: Pitch should stay center (no sound change).
5.  Set **Shift** to `+12`.
    *   Math: `8192 + (12 * (8192/12)) = 16383`.
    *   Result: Pitch glides UP to max.
6.  Set **Shift** to `-12`.
    *   Math: `8192 + (-12 * (8192/12)) = 0`.
    *   Result: Pitch glides DOWN to min.

If this fixes it, the Pitch Bend logic is now perfect.