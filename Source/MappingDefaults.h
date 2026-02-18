#pragma once
#include <JuceHeader.h>

// Single source of truth for mapping property defaults. Used by the compiler,
// schema (InspectorControl.defaultValue), Touchpad editor, and MappingInspector.
namespace MappingDefaults {

// ADSR (Expression custom envelope)
constexpr int ADSRAttackMs = 200;
constexpr int ADSRDecayMs = 0;
constexpr double ADSRSustain = 1.0;
constexpr int ADSRReleaseMs = 100;

// Expression CC
constexpr int TouchpadValueWhenOn = 127;
constexpr int TouchpadValueWhenOff = 0;

// Common (Note/Expression/Command)
constexpr int Channel = 1;
constexpr int Data1 = 60;
constexpr int Data2 = 127;
constexpr int VelRandom = 0;
constexpr bool Enabled = true;

// Note release / hold (stored as string in ValueTree)
inline const juce::String ReleaseBehaviorSendNoteOff{ "Send Note Off" };
inline const juce::String TouchpadHoldBehaviorHold{ "Hold to not send note off immediately" };

// Expression target (stored as string)
inline const juce::String AdsrTargetCC{ "CC" };

// Touchpad event (Finger1Down = 0)
constexpr int InputTouchpadEvent = 0;

// Touchpad/Note params
constexpr float TouchpadThreshold = 0.5f;
constexpr int TouchpadTriggerAbove = 2;
constexpr float TouchpadInputMin = 0.0f;
constexpr float TouchpadInputMax = 1.0f;
constexpr float PitchPadRestingPercent = 10.0f;
constexpr float PitchPadCustomStart = 0.5f;

// Touchpad output range
constexpr int TouchpadOutputMin = 0;
constexpr int TouchpadOutputMax = 127;
constexpr int TouchpadOutputMaxPitchBend = 3; // 0-3 semitones display

// Command / transpose / scale
constexpr int ReleaseValue = 0;
constexpr int TouchpadLayoutGroupId = 0;
constexpr int TouchpadSoloScope = 0;
constexpr int TransposeModify = 0;
constexpr int TransposeSemitones = 0;
constexpr int RootModify = 0;
constexpr int RootNote = 60;
constexpr int ScaleModify = 0;
constexpr int ScaleIndex = 0;
constexpr int SmartStepShift = 1;

// Expression CC controller number (data1 when target is CC)
constexpr int ExpressionData1 = 1;

// Input key (keyboard)
constexpr int InputKey = 0;

} // namespace MappingDefaults
