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
inline const juce::String CcReleaseBehaviorInstant{ "Send release (instant)" };

// Expression target (stored as string)
inline const juce::String AdsrTargetCC{ "CC" };

// Expression CC input mode (Position = current behaviour, Slide = single fader)
inline const juce::String ExpressionCCModePosition{ "Position" };

// Slide mode: 0 = Quick, 1 = Precision; 0 = Absolute, 1 = Relative; 0 = Lock, 1 = Free
constexpr int SlideQuickPrecision = 0;
constexpr int SlideAbsRel = 0;
constexpr int SlideLockFree = 1;
// Slide axis: 0 = Vertical (Y), 1 = Horizontal (X)
constexpr int SlideAxis = 0;
// Slide CC rest-on-release defaults
constexpr bool SlideReturnOnRelease = false;
constexpr int SlideRestValue = 0;
constexpr int SlideReturnGlideMs = 0;

// Encoder (Expression CC mode = Encoder)
inline const juce::String ExpressionCCModeEncoder{ "Encoder" };
constexpr int EncoderAxis = 0;           // 0=Vertical, 1=Horizontal, 2=Both
constexpr double EncoderSensitivity = 1.0;
constexpr int EncoderStepSize = 1;
constexpr int EncoderStepSizeX = 1;
constexpr int EncoderStepSizeY = 1;
constexpr int EncoderOutputModeAbsolute = 0;
constexpr int EncoderOutputModeRelative = 1;
constexpr int EncoderOutputModeNRPN = 2;
inline const juce::String EncoderOutputModeAbsoluteStr{ "Absolute" };
inline const juce::String EncoderOutputModeRelativeStr{ "Relative" };
inline const juce::String EncoderOutputModeNRPNStr{ "NRPN" };
constexpr int EncoderRelativeEncoding = 0; // 0=Offset Binary, 1=Sign Magnitude, 2=Two's Complement, 3=Simple Incremental
constexpr bool EncoderWrap = false;
constexpr int EncoderInitialValue = 64;
constexpr int EncoderNRPNNumber = 0;
constexpr int EncoderPushDetection = 0;   // 0=Two-finger, 1=Three-finger
constexpr int EncoderPushOutputType = 0;   // 0=CC, 1=Note, 2=ProgramChange
constexpr int EncoderPushMode = 0;        // 0=Off, 1=Momentary, 2=Toggle, 3=Trigger
constexpr int EncoderPushCCNumber = 1;    // default same as rotation CC (set at compile)
constexpr int EncoderPushValue = 127;
constexpr int EncoderPushNote = 60;
constexpr int EncoderPushProgram = 0;
constexpr int EncoderPushChannel = 1;     // default same as rotation channel (set at compile)
constexpr double EncoderDeadZone = 0.0;

// Touchpad event (Finger1Down = 0)
constexpr int InputTouchpadEvent = 0;

// Touchpad/Note params
constexpr float TouchpadThreshold = 0.5f;
constexpr int TouchpadTriggerAbove = 2;
constexpr float TouchpadInputMin = 0.0f;
constexpr float TouchpadInputMax = 1.0f;
constexpr float PitchPadRestingPercent = 10.0f;
constexpr float PitchPadRestZonePercent = 10.0f;
constexpr float PitchPadTransitionZonePercent = 10.0f;
constexpr float PitchPadCustomStart = 0.5f;

// Touch glide: smooth transition on touch/release (0 = off, ms when > 0)
constexpr int PitchPadTouchGlideMs = 0;
constexpr int TouchpadOutputMin = 0;
constexpr int TouchpadOutputMax = 127;
constexpr int TouchpadOutputMaxPitchBend = 3; // 0-3 semitones display

// Command / transpose / scale
constexpr int ReleaseValue = 0;
constexpr int TouchpadLayoutGroupId = 0;
constexpr int TouchpadSoloScope = 0;
constexpr int KeyboardGroupId = 0;
constexpr int KeyboardLayoutGroupId = 0;
constexpr int KeyboardSoloScope = 0;
constexpr int TransposeModify = 0;
constexpr int TransposeSemitones = 0;
constexpr int RootModify = 0;
constexpr int RootNote = 60;
constexpr int ScaleModify = 0;
constexpr int ScaleIndex = 0;
constexpr int SmartStepShift = 1;
constexpr bool SmartScaleFollowGlobal = true;
inline const juce::String SmartScaleName{ "Major" };

// Expression CC controller number (data1 when target is CC)
constexpr int ExpressionData1 = 1;

// Input key (keyboard)
constexpr int InputKey = 0;

} // namespace MappingDefaults
