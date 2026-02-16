# Touchpad Mappings - Complete Implementation Guide

This document provides a **comprehensive, step-by-step implementation guide** for the touchpad mappings feature. Every detail, logic flow, integration point, and test is documented to enable accurate re-implementation in another program.

---

## Table of Contents

1. [Overview](#overview)
2. [Data Structures & Types](#data-structures--types)
3. [Compilation Process](#compilation-process)
4. [Runtime Processing](#runtime-processing)
5. [Visualizer Integration](#visualizer-integration)
6. [Pitch Pad Feature](#pitch-pad-feature)
7. [UI Schema Generation](#ui-schema-generation)
8. [Tests](#tests)
9. [Edge Cases & Nuances](#edge-cases--nuances)

---

## Overview

Touchpad mappings allow users to map touchpad events (finger down/up, X/Y positions, distance, averages) to MIDI actions (Notes, Expression CC/PitchBend/SmartScaleBend, Commands). The system supports:

- **11 touchpad event types**: 4 boolean (Down/Up for fingers 1 & 2), 7 continuous (X/Y positions, distance, averages)
- **3 action types**: Note, Expression, Command
- **4 conversion kinds**: Boolean→Gate, Boolean→CC, Continuous→Gate, Continuous→Range
- **Pitch Pad feature**: Discrete step-based pitch control for PitchBend/SmartScaleBend
- **Layer support**: Mappings are per-layer (0-8)
- **Layout integration**: Touchpad layouts (Mixer, Drum Pad, Chord Pad) can consume finger events

---

## Data Structures & Types

### Core Enumerations

**TouchpadEvent** (in `MappingTypes.h`):
```cpp
namespace TouchpadEvent {
  constexpr int Finger1Down = 0;      // Boolean
  constexpr int Finger1Up = 1;        // Boolean
  constexpr int Finger1X = 2;         // Continuous
  constexpr int Finger1Y = 3;         // Continuous
  constexpr int Finger2Down = 4;      // Boolean
  constexpr int Finger2Up = 5;        // Boolean
  constexpr int Finger2X = 6;         // Continuous
  constexpr int Finger2Y = 7;         // Continuous
  constexpr int Finger1And2Dist = 8;  // Continuous
  constexpr int Finger1And2AvgX = 9;  // Continuous
  constexpr int Finger1And2AvgY = 10; // Continuous
  constexpr int Count = 11;
}
```

**TouchpadConversionKind** (in `MappingTypes.h`):
```cpp
enum class TouchpadConversionKind {
  BoolToGate,         // Boolean event → Note/Command (direct trigger)
  BoolToCC,           // Boolean event → Expression (value when on/off)
  ContinuousToGate,   // Continuous event → Note (threshold-based)
  ContinuousToRange   // Continuous event → Expression (range mapping)
};
```

**PitchPadMode** (in `MappingTypes.h`):
```cpp
enum class PitchPadMode {
  Absolute,  // Fixed mapping: center of pad = zero pitch
  Relative   // Anchor-based: first touch = zero pitch
};
```

**PitchPadStart** (in `MappingTypes.h`):
```cpp
enum class PitchPadStart {
  Left,    // Zero step = minStep
  Center,  // Zero step = 0 (middle of range)
  Right,   // Zero step = maxStep
  Custom   // Zero step = interpolated from customStartX
};
```

### Core Data Structures

**TouchpadContact** (in `TouchpadTypes.h`):
```cpp
struct TouchpadContact {
  int contactId = 0;      // Unique ID for this contact (persists across frames)
  int x = 0;              // Raw X coordinate (device-specific)
  int y = 0;              // Raw Y coordinate (device-specific)
  float normX = 0.0f;     // Normalized X [0,1] (left=0, right=1)
  float normY = 0.0f;     // Normalized Y [0,1] (bottom=0, top=1)
  bool tipDown = true;    // true when finger touches, false when lifted
};
```

**TouchpadConversionParams** (in `MappingTypes.h`):
```cpp
struct TouchpadConversionParams {
  // ContinuousToGate fields
  float threshold = 0.5f;        // Threshold value (0-1)
  bool triggerAbove = true;      // true = trigger when above threshold
  
  // ContinuousToRange fields
  float inputMin = 0.0f;         // Input range minimum (0-1)
  float inputMax = 1.0f;         // Input range maximum (0-1)
  float invInputRange = 1.0f;    // 1/(inputMax-inputMin) for fast remap
  int outputMin = 0;             // Output range minimum (0-127 for CC, -12..+12 for steps)
  int outputMax = 127;           // Output range maximum
  
  // BoolToCC fields
  int valueWhenOn = 127;         // CC value when boolean is true
  int valueWhenOff = 0;          // CC value when boolean is false
  
  // Pitch Pad fields (optional, only for PitchBend/SmartScaleBend)
  std::optional<PitchPadConfig> pitchPadConfig;
  std::optional<PitchPadLayout> cachedPitchPadLayout;  // Pre-built layout
};
```

**PitchPadConfig** (in `MappingTypes.h`):
```cpp
struct PitchPadConfig {
  PitchPadMode mode = PitchPadMode::Absolute;
  PitchPadStart start = PitchPadStart::Center;
  float customStartX = 0.5f;           // Custom neutral position [0,1]
  int minStep = -2;                    // Minimum step (semitones or scale steps)
  int maxStep = 2;                     // Maximum step
  float restingSpacePercent = 10.0f;   // % of pad width per step for resting bands
  float zeroStep = 0.0f;               // Step that maps to zero pitch (computed)
};
```

**PitchPadBand** (in `MappingTypes.h`):
```cpp
struct PitchPadBand {
  float xStart = 0.0f;      // Start X [0,1]
  float xEnd = 0.0f;        // End X [0,1]
  float invSpan = 0.0f;     // 1/(xEnd-xStart) for fast lookup
  int step = 0;             // Step value for this band
  bool isRest = false;      // true = resting band, false = transition band
};
```

**PitchPadLayout** (in `MappingTypes.h`):
```cpp
struct PitchPadLayout {
  std::vector<PitchPadBand> bands;  // Ordered bands covering [0,1]
};
```

**PitchSample** (in `PitchPadUtilities.h`):
```cpp
struct PitchSample {
  float step = 0.0f;        // Effective step (may be fractional)
  bool inRestingBand = false;  // true if in a resting band
  float localT = 0.0f;      // Position within current band [0,1]
};
```

**TouchpadMappingEntry** (in `MappingTypes.h`):
```cpp
struct TouchpadMappingEntry {
  int layerId = 0;                          // Layer ID (0-8)
  int eventId = 0;                          // TouchpadEvent::Finger1Down etc.
  MidiAction action;                        // The MIDI action to perform
  TouchpadConversionKind conversionKind;    // How to convert input to action
  TouchpadConversionParams conversionParams; // Conversion parameters
};
```

**CompiledContext** (in `MappingTypes.h`):
```cpp
struct CompiledContext {
  // ... other fields ...
  std::vector<TouchpadMappingEntry> touchpadMappings;  // Compiled touchpad mappings
  // ... other fields ...
};
```

### Helper Functions

**isTouchpadEventBoolean** (in `GridCompiler.cpp`):
```cpp
static bool isTouchpadEventBoolean(int eventId) {
  return (eventId == TouchpadEvent::Finger1Down ||
          eventId == TouchpadEvent::Finger1Up ||
          eventId == TouchpadEvent::Finger2Down ||
          eventId == TouchpadEvent::Finger2Up);
}
```

---

## Compilation Process

The compilation process converts `juce::ValueTree` mappings (from preset) into `TouchpadMappingEntry` structs stored in `CompiledContext::touchpadMappings`.

### Entry Point

**GridCompiler::compile** (in `GridCompiler.cpp`):
- Calls `compileMappingsForLayer` for each layer (0-8)
- Passes `touchpadMappingsOut` pointer to collect touchpad mappings
- Touchpad mappings are **appended** to the vector (not written to grid)

### Compilation Logic: `compileMappingsForLayer`

**Location**: `GridCompiler.cpp`, lines ~740-1098

**Process**:

1. **Get enabled mappings for layer**:
   ```cpp
   std::vector<juce::ValueTree> enabledList = presetMgr.getEnabledMappingsForLayer(layerId);
   ```

2. **Iterate through mappings**:
   ```cpp
   for (int i : order) {
     auto mapping = enabledList[i];
     // ... process mapping ...
   }
   ```

3. **Check if touchpad mapping**:
   ```cpp
   juce::String aliasName = mapping.getProperty("inputAlias", "").toString().trim();
   const bool isTouchpadMapping = aliasName.trim().equalsIgnoreCase("Touchpad");
   ```

4. **If touchpad mapping**:
   - Extract `inputTouchpadEvent` property (default: 0)
   - Clamp to valid range: `juce::jlimit(0, TouchpadEvent::Count - 1, eventId)`
   - Build `MidiAction` from mapping
   - Create `TouchpadMappingEntry`
   - Determine `conversionKind` based on:
     - Mapping type (Note vs Expression vs Command)
     - Event type (boolean vs continuous)

### Conversion Kind Determination

**For Note mappings**:
```cpp
if (typeStr.equalsIgnoreCase("Note")) {
  applyNoteOptionsFromMapping(mapping, zoneMgr, entry.action);
  if (inputBool) {
    entry.conversionKind = TouchpadConversionKind::BoolToGate;
  } else {
    entry.conversionKind = TouchpadConversionKind::ContinuousToGate;
    p.threshold = (float)mapping.getProperty("touchpadThreshold", 0.5);
    int triggerId = (int)mapping.getProperty("touchpadTriggerAbove", 2);
    p.triggerAbove = (triggerId == 2);  // 1 = below, 2 = above
  }
}
```

**For Expression mappings**:
```cpp
else if (typeStr.equalsIgnoreCase("Expression")) {
  juce::String adsrTargetStr = mapping.getProperty("adsrTarget", "CC").toString().trim();
  const bool isCC = adsrTargetStr.equalsIgnoreCase("CC");
  const bool isPB = adsrTargetStr.equalsIgnoreCase("PitchBend");
  const bool isSmartBend = adsrTargetStr.equalsIgnoreCase("SmartScaleBend");
  
  if (inputBool) {
    entry.conversionKind = TouchpadConversionKind::BoolToCC;
    if (isCC) {
      p.valueWhenOn = (int)mapping.getProperty("touchpadValueWhenOn", 127);
      p.valueWhenOff = (int)mapping.getProperty("touchpadValueWhenOff", 0);
    }
  } else {
    entry.conversionKind = TouchpadConversionKind::ContinuousToRange;
    // ... set input/output ranges ...
    // ... handle Pitch Pad config for PB/SmartScaleBend ...
  }
}
```

**For Command mappings**:
```cpp
else {
  entry.conversionKind = TouchpadConversionKind::BoolToGate;
}
```

### ContinuousToRange Parameter Setup

**For CC targets**:
```cpp
p.inputMin = (float)mapping.getProperty("touchpadInputMin", 0.0);
p.inputMax = (float)mapping.getProperty("touchpadInputMax", 1.0);
float r = p.inputMax - p.inputMin;
p.invInputRange = (r > 0.0f) ? (1.0f / r) : 0.0f;
p.outputMin = (int)mapping.getProperty("touchpadOutputMin", 0);
p.outputMax = (int)mapping.getProperty("touchpadOutputMax", 127);
p.pitchPadConfig.reset();  // No pitch pad for CC
```

**For PitchBend/SmartScaleBend targets**:
```cpp
if (isPB || isSmartBend) {
  p.outputMin = (int)mapping.getProperty("touchpadOutputMin", -1);
  p.outputMax = (int)mapping.getProperty("touchpadOutputMax", 3);
  
  PitchPadConfig cfg;
  juce::String modeStr = mapping.getProperty("pitchPadMode", "Absolute").toString();
  cfg.mode = modeStr.equalsIgnoreCase("Relative") ? PitchPadMode::Relative : PitchPadMode::Absolute;
  
  juce::String startStr = mapping.getProperty("pitchPadStart", "Center").toString();
  if (startStr.equalsIgnoreCase("Left"))
    cfg.start = PitchPadStart::Left;
  else if (startStr.equalsIgnoreCase("Right"))
    cfg.start = PitchPadStart::Right;
  else if (startStr.equalsIgnoreCase("Custom"))
    cfg.start = PitchPadStart::Custom;
  else
    cfg.start = PitchPadStart::Center;
  
  cfg.customStartX = (float)mapping.getProperty("pitchPadCustomStart", 0.5);
  cfg.minStep = p.outputMin;
  cfg.maxStep = p.outputMax;
  cfg.restingSpacePercent = (float)mapping.getProperty("pitchPadRestingPercent", 10.0);
  cfg.zeroStep = 0.0f;  // For absolute mode, zero is always 0
  
  p.pitchPadConfig = cfg;
  p.cachedPitchPadLayout = buildPitchPadLayout(cfg);  // Pre-build layout
}
```

### Expression ADSR Settings

**For all Expression mappings**:
```cpp
if (isPB)
  entry.action.adsrSettings.target = AdsrTarget::PitchBend;
else if (isSmartBend)
  entry.action.adsrSettings.target = AdsrTarget::SmartScaleBend;
else
  entry.action.adsrSettings.target = AdsrTarget::CC;

// Custom ADSR envelope is NOT supported for PitchBend/SmartScaleBend
entry.action.adsrSettings.useCustomEnvelope =
    (bool)mapping.getProperty("useCustomEnvelope", false) && !isPB && !isSmartBend;

if (!entry.action.adsrSettings.useCustomEnvelope) {
  entry.action.adsrSettings.attackMs = 0;
  entry.action.adsrSettings.decayMs = 0;
  entry.action.adsrSettings.sustainLevel = 1.0f;
  entry.action.adsrSettings.releaseMs = 0;
} else {
  entry.action.adsrSettings.attackMs = (int)mapping.getProperty("adsrAttack", 10);
  entry.action.adsrSettings.decayMs = (int)mapping.getProperty("adsrDecay", 10);
  entry.action.adsrSettings.sustainLevel = (float)mapping.getProperty("adsrSustain", 0.7);
  entry.action.adsrSettings.releaseMs = (int)mapping.getProperty("adsrRelease", 100);
}
```

**Release value handling**:
```cpp
bool defaultResetPitch = (isPB || isSmartBend);
entry.action.sendReleaseValue = (bool)mapping.getProperty("sendReleaseValue", defaultResetPitch);
entry.action.releaseValue = (int)mapping.getProperty("touchpadValueWhenOff", 0);
```

**CC-specific settings**:
```cpp
if (entry.action.adsrSettings.target == AdsrTarget::CC) {
  entry.action.adsrSettings.ccNumber = (int)mapping.getProperty("data1", 1);
  entry.action.adsrSettings.valueWhenOn = (int)mapping.getProperty("touchpadValueWhenOn", 127);
  entry.action.adsrSettings.valueWhenOff = (int)mapping.getProperty("touchpadValueWhenOff", 0);
  entry.action.data2 = entry.action.adsrSettings.valueWhenOn;
}
```

**PitchBend-specific settings**:
```cpp
else if (entry.action.adsrSettings.target == AdsrTarget::PitchBend) {
  const int pbRange = settingsMgr.getPitchBendRange();
  int semitones = (int)mapping.getProperty("data2", 0);
  entry.action.data2 = juce::jlimit(-juce::jmax(1, pbRange), juce::jmax(1, pbRange), semitones);
}
```

**SmartScaleBend-specific settings**:
```cpp
else if (entry.action.adsrSettings.target == AdsrTarget::SmartScaleBend) {
  buildSmartBendLookup(entry.action, mapping, zoneMgr, settingsMgr);
  entry.action.data2 = 8192;  // Peak from lookup per note
}
```

### Note Options Application

**applyNoteOptionsFromMapping** (called before setting conversionKind):
- Sets `releaseBehavior` from mapping property
- Sets `followTranspose` from mapping property
- Sets note number (`data1`), channel, velocity (`data2`)

### Disabled Mappings

**Critical**: Only **enabled** mappings are compiled. Check:
```cpp
bool enabled = mapping.getProperty("enabled", true);
if (!enabled) continue;  // Skip disabled mappings
```

**Note**: `getEnabledMappingsForLayer` already filters disabled mappings, but double-check is safe.

### Final Step

After processing all mappings for all layers:
```cpp
touchpadMappingsOut->push_back(std::move(entry));
```

The compiled `touchpadMappings` vector is stored in `CompiledContext` and used at runtime.

---

## Runtime Processing

The runtime processing happens in `InputProcessor::processTouchpadContacts`.

### Entry Point

**InputProcessor::processTouchpadContacts** (in `InputProcessor.cpp`, line ~1714):
```cpp
void InputProcessor::processTouchpadContacts(
    uintptr_t deviceHandle,
    const std::vector<TouchpadContact> &contacts);
```

### Step 1: Extract Contact State

**Extract finger states**:
```cpp
bool tip1 = false, tip2 = false;
float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;

for (const auto &c : contacts) {
  if (c.contactId == 0 && c.tipDown) {
    tip1 = true;
    x1 = c.normX;
    y1 = c.normY;
  } else if (c.contactId == 1 && c.tipDown) {
    tip2 = true;
    x2 = c.normX;
    y2 = c.normY;
  }
}
```

**Detect transitions** (compare with previous frame):
```cpp
bool finger1Down = tip1 && !prevTip1;  // New finger 1 down
bool finger1Up = !tip1 && prevTip1;    // Finger 1 lifted
bool finger2Down = tip2 && !prevTip2;
bool finger2Up = !tip2 && prevTip2;
```

**Calculate derived values**:
```cpp
float dist = 0.0f;
float avgX = 0.0f;
float avgY = 0.0f;

if (tip1 && tip2) {
  float dx = x2 - x1;
  float dy = y2 - y1;
  dist = std::sqrt(dx * dx + dy * dy);
  avgX = (x1 + x2) * 0.5f;
  avgY = (y1 + y2) * 0.5f;
}
```

### Step 2: Check Layout Consumption

**Before processing mappings**, check if layouts consume finger events:
```cpp
auto layout1 = contacts.size() >= 1 ? layoutPerContact[0] : std::nullopt;
auto layout2 = contacts.size() >= 2 ? layoutPerContact[1] : std::nullopt;
bool layoutConsumesFinger1Down = tip1 && layout1.has_value();
bool layoutConsumesFinger2Down = tip2 && layout2.has_value();
```

**Layouts consume Finger1Down/Finger2Down** when:
- Finger is within a layout region (Mixer, Drum Pad, Chord Pad)
- Layout has `regionLock` enabled (finger is locked to layout)

### Step 3: Process Touchpad Mappings

**Iterate through compiled mappings**:
```cpp
if (!ctx->touchpadMappings.empty()) {
  for (const auto &entry : ctx->touchpadMappings) {
    // Check layer is active
    if (!activeLayersSnapshot[(size_t)entry.layerId])
      continue;
    
    // Extract value for this event
    bool boolVal = false;
    float continuousVal = 0.0f;
    switch (entry.eventId) {
      case TouchpadEvent::Finger1Down: boolVal = finger1Down; break;
      case TouchpadEvent::Finger1Up: boolVal = finger1Up; break;
      case TouchpadEvent::Finger1X: continuousVal = x1; break;
      case TouchpadEvent::Finger1Y: continuousVal = y1; break;
      case TouchpadEvent::Finger2Down: boolVal = finger2Down; break;
      case TouchpadEvent::Finger2Up: boolVal = finger2Up; break;
      case TouchpadEvent::Finger2X: continuousVal = x2; break;
      case TouchpadEvent::Finger2Y: continuousVal = y2; break;
      case TouchpadEvent::Finger1And2Dist: continuousVal = dist; break;
      case TouchpadEvent::Finger1And2AvgX: continuousVal = avgX; break;
      case TouchpadEvent::Finger1And2AvgY: continuousVal = avgY; break;
      default: continue;
    }
    
    // Process based on conversionKind
    switch (entry.conversionKind) {
      // ... handle each kind ...
    }
  }
}
```

### Step 4: BoolToGate Processing

**For Note actions**:
```cpp
case TouchpadConversionKind::BoolToGate:
  if (act.type == ActionType::Note) {
    bool isDownEvent = (entry.eventId == TouchpadEvent::Finger1Down ||
                        entry.eventId == TouchpadEvent::Finger2Down);
    bool layoutConsumes = (entry.eventId == TouchpadEvent::Finger1Down && layoutConsumesFinger1Down) ||
                          (entry.eventId == TouchpadEvent::Finger2Down && layoutConsumesFinger2Down);
    
    // Skip if layout consumes this finger
    if (isDownEvent && layoutConsumes)
      continue;
    
    // Handle release on same frame as down (finger down then immediately up)
    bool releaseThisFrame = (entry.eventId == TouchpadEvent::Finger1Down && finger1Up) ||
                            (entry.eventId == TouchpadEvent::Finger2Down && finger2Up);
    
    InputID touchpadInput{deviceHandle, 0};
    
    if (isDownEvent && boolVal) {
      // Finger down: trigger note on
      triggerManualNoteOn(touchpadInput, act, true);  // true = latch applies
    } else if (releaseThisFrame) {
      // Finger released on same frame: trigger release
      triggerManualNoteRelease(touchpadInput, act);
    } else if (!isDownEvent && boolVal) {
      // Finger1Up/Finger2Up: trigger note when finger lifts (one-shot)
      triggerManualNoteOn(touchpadInput, act, false);  // false = latch doesn't apply
    }
  }
  break;
```

**For Command actions**:
```cpp
else if (act.type == ActionType::Command && boolVal) {
  int cmd = act.data1;
  if (cmd == static_cast<int>(MIDIQy::CommandID::SustainMomentary))
    voiceManager.setSustain(true);
  else if (cmd == static_cast<int>(MIDIQy::CommandID::SustainToggle))
    voiceManager.setSustain(!voiceManager.isSustainActive());
  else if (cmd == static_cast<int>(MIDIQy::CommandID::Panic))
    voiceManager.panic();
  // ... other commands ...
}
```

### Step 5: BoolToCC Processing

**For Expression actions**:
```cpp
case TouchpadConversionKind::BoolToCC:
  if (act.type == ActionType::Expression) {
    if (boolVal) {
      InputID touchpadExprInput{deviceHandle, static_cast<int>(entry.eventId)};
      int peakValue;
      
      if (act.adsrSettings.target == AdsrTarget::CC) {
        peakValue = act.adsrSettings.valueWhenOn;
      } else if (act.adsrSettings.target == AdsrTarget::PitchBend) {
        double stepsPerSemitone = settingsManager.getStepsPerSemitone();
        peakValue = static_cast<int>(8192.0 + (act.data2 * stepsPerSemitone));
        peakValue = juce::jlimit(0, 16383, peakValue);
      } else {  // SmartScaleBend
        if (!act.smartBendLookup.empty() && lastTriggeredNote >= 0 && lastTriggeredNote < 128)
          peakValue = act.smartBendLookup[lastTriggeredNote];
        else
          peakValue = 8192;
      }
      
      expressionEngine.triggerEnvelope(touchpadExprInput, act.channel, act.adsrSettings, peakValue);
      touchpadExpressionActive.insert(std::make_tuple(deviceHandle, entry.layerId, entry.eventId));
    }
  }
  break;
```

**Note**: Boolean Expression mappings only trigger on `true` (finger down). They do NOT send value when `false` (finger up) - the envelope handles release automatically.

### Step 6: ContinuousToGate Processing

**Threshold-based note triggering**:
```cpp
case TouchpadConversionKind::ContinuousToGate:
  bool above = continuousVal >= p.threshold;
  bool trigger = p.triggerAbove ? above : !above;
  InputID touchpadInput{deviceHandle, 0};
  auto key = std::make_tuple(deviceHandle, entry.layerId, entry.eventId);
  
  if (trigger) {
    if (touchpadNoteOnSent.find(key) == touchpadNoteOnSent.end()) {
      touchpadNoteOnSent.insert(key);
      triggerManualNoteOn(touchpadInput, act);
    }
  } else {
    if (touchpadNoteOnSent.erase(key))
      triggerManualNoteRelease(touchpadInput, act);
  }
  break;
```

**State tracking**: Use `touchpadNoteOnSent` set to prevent duplicate Note On messages when threshold is crossed multiple times.

### Step 7: ContinuousToRange Processing

**Most complex conversion kind**. Handles CC, PitchBend, and SmartScaleBend.

#### 7.1: Determine Event Active State

```cpp
case TouchpadConversionKind::ContinuousToRange:
  if (act.type == ActionType::Expression) {
    bool eventActive = true;
    switch (entry.eventId) {
      case TouchpadEvent::Finger1X:
      case TouchpadEvent::Finger1Y:
      case TouchpadEvent::Finger1And2AvgX:
      case TouchpadEvent::Finger1And2AvgY:
        eventActive = tip1;
        break;
      case TouchpadEvent::Finger2X:
      case TouchpadEvent::Finger2Y:
        eventActive = tip2;
        break;
      case TouchpadEvent::Finger1And2Dist:
        eventActive = tip1 && tip2;
        break;
      default:
        break;
    }
```

#### 7.2: Handle Release (Event No Longer Active)

```cpp
    const int ccNumberOrMinusOne = (act.adsrSettings.target == AdsrTarget::CC)
                                      ? act.adsrSettings.ccNumber
                                      : -1;
    auto keyCont = std::make_tuple(deviceHandle, entry.layerId, entry.eventId,
                                   act.channel, ccNumberOrMinusOne);
    
    if (!eventActive) {
      auto itLast = lastTouchpadContinuousValues.find(keyCont);
      if (itLast != lastTouchpadContinuousValues.end()) {
        if (act.sendReleaseValue) {
          if (act.adsrSettings.target == AdsrTarget::CC) {
            voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, act.releaseValue);
          } else if (act.adsrSettings.target == AdsrTarget::PitchBend ||
                     act.adsrSettings.target == AdsrTarget::SmartScaleBend) {
            voiceManager.sendPitchBend(act.channel, 8192);  // Center pitch
          }
        }
        lastTouchpadContinuousValues.erase(itLast);
      }
      break;  // Don't process further
    }
```

#### 7.3: Normalize Input Value

```cpp
    float t = (continuousVal - p.inputMin) * p.invInputRange;
    t = std::clamp(t, 0.0f, 1.0f);
```

#### 7.4: Calculate Output Value (CC) or Step Offset (Pitch)

**For CC targets** (simple linear mapping):
```cpp
    float outVal = static_cast<float>(p.outputMin) +
                   static_cast<float>(p.outputMax - p.outputMin) * t;
    int ccVal = juce::jlimit(0, 127, static_cast<int>(std::round(outVal)));
    
    // Change-only sending
    auto itLast = lastTouchpadContinuousValues.find(keyCont);
    if (itLast != lastTouchpadContinuousValues.end() && itLast->second == ccVal) {
      break;  // No change, skip send
    }
    voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, ccVal);
    lastTouchpadContinuousValues[keyCont] = ccVal;
```

**For PitchBend/SmartScaleBend targets** (with optional Pitch Pad):

**Without Pitch Pad** (simple linear mapping):
```cpp
    float stepOffset = juce::jmap(t, static_cast<float>(p.outputMin),
                                  static_cast<float>(p.outputMax));
```

**With Pitch Pad** (discrete step-based):
```cpp
    if (p.pitchPadConfig.has_value() &&
        (act.adsrSettings.target == AdsrTarget::PitchBend ||
         act.adsrSettings.target == AdsrTarget::SmartScaleBend)) {
      const PitchPadConfig &cfg = *p.pitchPadConfig;
      const PitchPadLayout &layout = p.cachedPitchPadLayout
                                        ? *p.cachedPitchPadLayout
                                        : buildPitchPadLayout(cfg);
      
      auto relKey = std::make_tuple(deviceHandle, entry.layerId, entry.eventId, act.channel);
      
      if (cfg.mode == PitchPadMode::Relative) {
        // Relative mode: anchor-based
        bool startGesture = false;
        if (entry.eventId == TouchpadEvent::Finger1X ||
            entry.eventId == TouchpadEvent::Finger1Y ||
            entry.eventId == TouchpadEvent::Finger1And2Dist ||
            entry.eventId == TouchpadEvent::Finger1And2AvgX ||
            entry.eventId == TouchpadEvent::Finger1And2AvgY) {
          startGesture = finger1Down;
        } else if (entry.eventId == TouchpadEvent::Finger2X ||
                   entry.eventId == TouchpadEvent::Finger2Y) {
          startGesture = finger2Down;
        }
        
        if (startGesture) {
          // Store anchor
          juce::ScopedLock al(anchorLock);
          pitchPadRelativeAnchorT[relKey] = t;
          float anchorXClamped = juce::jlimit(0.0f, 1.0f, t);
          PitchSample anchorSample = mapXToStep(layout, anchorXClamped);
          pitchPadRelativeAnchorStep[relKey] = anchorSample.step;
        }
        
        // Calculate step offset from anchor
        float anchorStepVal = 0.0f;
        bool hasAnchor = false;
        {
          juce::ScopedLock al(anchorLock);
          auto itAnchorStep = pitchPadRelativeAnchorStep.find(relKey);
          if (itAnchorStep != pitchPadRelativeAnchorStep.end()) {
            anchorStepVal = itAnchorStep->second;
            hasAnchor = true;
          }
        }
        
        if (hasAnchor) {
          float xClamped = juce::jlimit(0.0f, 1.0f, t);
          PitchSample sample = mapXToStep(layout, xClamped);
          stepOffset = sample.step - anchorStepVal;
        } else {
          stepOffset = 0.0f;
        }
      } else {
        // Absolute mode: fixed mapping
        float xClamped = juce::jlimit(0.0f, 1.0f, t);
        PitchSample sample = mapXToStep(layout, xClamped);
        stepOffset = sample.step - cfg.zeroStep;
      }
    }
```

#### 7.5: Convert Step Offset to PitchBend Value

**For SmartScaleBend**:
```cpp
    if (act.adsrSettings.target == AdsrTarget::SmartScaleBend) {
      int currentNote = voiceManager.getCurrentPlayingNote(act.channel);
      if (currentNote < 0)
        currentNote = lastTriggeredNote;
      
      if (currentNote >= 0 && currentNote < 128) {
        std::vector<int> intervals = zoneManager.getGlobalScaleIntervals();
        if (intervals.empty())
          intervals = {0, 2, 4, 5, 7, 9, 11};  // Major fallback
        int root = zoneManager.getGlobalRootNote();
        
        float baseStep = std::floor(stepOffset);
        float frac = stepOffset - baseStep;
        int s0 = static_cast<int>(baseStep);
        int s1 = (frac >= 0.0f) ? s0 + 1 : s0 - 1;
        
        int pb0 = ScaleUtilities::smartStepOffsetToPitchBend(
            currentNote, root, intervals, s0, pbRange);
        int pb1 = ScaleUtilities::smartStepOffsetToPitchBend(
            currentNote, root, intervals, s1, pbRange);
        
        float f = std::abs(frac);
        float blended = juce::jmap(f, 0.0f, 1.0f, static_cast<float>(pb0), static_cast<float>(pb1));
        pbVal = juce::jlimit(0, 16383, static_cast<int>(std::round(blended)));
      }
    }
```

**For PitchBend**:
```cpp
    else {
      float clampedOffset = juce::jlimit(static_cast<float>(-pbRange),
                                         static_cast<float>(pbRange), stepOffset);
      double stepsPerSemitone = settingsManager.getStepsPerSemitone();
      pbVal = static_cast<int>(std::round(
          8192.0 + (static_cast<double>(clampedOffset) * stepsPerSemitone)));
      pbVal = juce::jlimit(0, 16383, pbVal);
    }
    
    // Change-only sending
    auto itLast = lastTouchpadContinuousValues.find(keyCont);
    if (itLast != lastTouchpadContinuousValues.end() && itLast->second == pbVal) {
      break;  // No change, skip send
    }
    voiceManager.sendPitchBend(act.channel, pbVal);
    lastTouchpadContinuousValues[keyCont] = pbVal;
```

### State Tracking

**InputProcessor maintains**:
- `lastTouchpadContinuousValues`: Map of `(deviceHandle, layerId, eventId, channel, ccNumber/-1)` → last sent value
- `touchpadNoteOnSent`: Set of `(deviceHandle, layerId, eventId)` for ContinuousToGate
- `touchpadExpressionActive`: Set of `(deviceHandle, layerId, eventId)` for BoolToCC
- `pitchPadRelativeAnchorT`: Map of `(deviceHandle, layerId, eventId, channel)` → anchor normalized X
- `pitchPadRelativeAnchorStep`: Map of `(deviceHandle, layerId, eventId, channel)` → anchor step value

**Cleanup**: When event becomes inactive (finger lifts), remove from `lastTouchpadContinuousValues` after sending release value.

---

## Visualizer Integration

The visualizer (`TouchpadVisualizerPanel`) displays touchpad mappings and pitch pad layouts.

### Reading Mappings from Context

**Location**: `TouchpadVisualizerPanel.cpp`, lines ~200-238

```cpp
std::optional<PitchPadConfig> configX;
std::optional<PitchPadConfig> configY;
std::optional<std::pair<float, float>> yCcInputRange;
juce::String xControlLabel;
juce::String yControlLabel;

if (inputProcessor) {
  auto ctx = inputProcessor->getContext();
  if (ctx) {
    for (const auto &entry : ctx->touchpadMappings) {
      if (entry.layerId != currentVisualizedLayer)
        continue;
      
      if (entry.eventId == TouchpadEvent::Finger1X &&
          entry.conversionParams.pitchPadConfig.has_value()) {
        configX = entry.conversionParams.pitchPadConfig;
        auto target = entry.action.adsrSettings.target;
        if (target == AdsrTarget::PitchBend || target == AdsrTarget::SmartScaleBend)
          xControlLabel = "PitchBend";
        else if (target == AdsrTarget::CC)
          xControlLabel = "CC" + juce::String(entry.action.adsrSettings.ccNumber);
      } else if (entry.eventId == TouchpadEvent::Finger1Y) {
        auto target = entry.action.adsrSettings.target;
        if (entry.conversionParams.pitchPadConfig.has_value()) {
          configY = entry.conversionParams.pitchPadConfig;
          if (target == AdsrTarget::PitchBend || target == AdsrTarget::SmartScaleBend)
            yControlLabel = "PitchBend";
          else if (target == AdsrTarget::CC)
            yControlLabel = "CC" + juce::String(entry.action.adsrSettings.ccNumber);
        } else if (entry.conversionKind == TouchpadConversionKind::ContinuousToRange &&
                   target == AdsrTarget::CC) {
          yCcInputRange = {entry.conversionParams.inputMin, entry.conversionParams.inputMax};
          yControlLabel = "CC" + juce::String(entry.action.adsrSettings.ccNumber);
        }
      }
    }
  }
}
```

### Drawing Pitch Pad Layouts

**For X-axis Pitch Pad**:
```cpp
if (configX) {
  PitchPadLayout layoutX = buildPitchPadLayout(*configX);
  
  // Draw resting bands
  for (const auto &band : layoutX.bands) {
    if (band.isRest) {
      float xStart = touchpadRect.getX() + band.xStart * touchpadRect.getWidth();
      float xEnd = touchpadRect.getX() + band.xEnd * touchpadRect.getWidth();
      juce::Rectangle<float> restRect(xStart, touchpadRect.getY(), xEnd - xStart, touchpadRect.getHeight());
      g.setColour(xRestCol);
      g.fillRect(restRect);
    }
  }
  
  // Draw transition bands
  for (const auto &band : layoutX.bands) {
    if (!band.isRest) {
      float xStart = touchpadRect.getX() + band.xStart * touchpadRect.getWidth();
      float xEnd = touchpadRect.getX() + band.xEnd * touchpadRect.getWidth();
      juce::Rectangle<float> transRect(xStart, touchpadRect.getY(), xEnd - xStart, touchpadRect.getHeight());
      g.setColour(xTransCol);
      g.fillRect(transRect);
    }
  }
  
  // Draw relative mode anchor
  if (configX->mode == PitchPadMode::Relative) {
    std::optional<float> anchorNormX = inputProcessor->getPitchPadRelativeAnchorNormX(
        lastDeviceHandle_.load(std::memory_order_acquire), currentVisualizedLayer,
        TouchpadEvent::Finger1X);
    if (anchorNormX.has_value()) {
      float anchorX = touchpadRect.getX() + anchorNormX.value() * touchpadRect.getWidth();
      g.setColour(juce::Colours::yellow);
      g.drawVerticalLine((int)anchorX, touchpadRect.getY(), touchpadRect.getBottom());
    }
  }
}
```

**Same logic for Y-axis** (rotate coordinates).

### Drawing CC Input Range

**For Y-axis CC** (without pitch pad):
```cpp
if (yCcInputRange.has_value()) {
  float rangeMin = yCcInputRange->first;
  float rangeMax = yCcInputRange->second;
  float yStart = touchpadRect.getBottom() - rangeMax * touchpadRect.getHeight();
  float yEnd = touchpadRect.getBottom() - rangeMin * touchpadRect.getHeight();
  juce::Rectangle<float> activeRect(touchpadRect.getX(), yStart, touchpadRect.getWidth(), yEnd - yStart);
  g.setColour(yCcActiveCol);
  g.fillRect(activeRect);
  
  // Draw inactive regions
  if (rangeMin > 0.0f) {
    juce::Rectangle<float> inactiveTop(touchpadRect.getX(), touchpadRect.getY(),
                                       touchpadRect.getWidth(), yStart - touchpadRect.getY());
    g.setColour(yCcInactiveCol);
    g.fillRect(inactiveTop);
  }
  if (rangeMax < 1.0f) {
    juce::Rectangle<float> inactiveBottom(touchpadRect.getX(), yEnd,
                                          touchpadRect.getWidth(), touchpadRect.getBottom() - yEnd);
    g.setColour(yCcInactiveCol);
    g.fillRect(inactiveBottom);
  }
}
```

---

## Pitch Pad Feature

The Pitch Pad feature provides discrete step-based pitch control for PitchBend and SmartScaleBend targets.

### Layout Building: `buildPitchPadLayout`

**Location**: `PitchPadUtilities.cpp`, lines 4-67

**Algorithm**:
1. **Calculate step count**: `stepCount = maxStep - minStep + 1`
2. **Calculate resting width**: `restWidthTotal = (restingSpacePercent / 100.0) * stepCount`
3. **Calculate remaining width**: `remaining = max(0, 1.0 - restWidthTotal)`
4. **Calculate transition count**: `transitionCount = stepCount > 1 ? (stepCount - 1) : 0`
5. **Calculate widths**:
   - `restWidth = restWidthTotal / stepCount` (per step)
   - `transitionWidth = remaining / transitionCount` (per transition)
6. **Build bands**:
   - For each step `i` from `minStep` to `maxStep`:
     - Create resting band: `[x, x + restWidth]`, step = `minStep + i`
     - Create transition band: `[x + restWidth, x + restWidth + transitionWidth]`, step = `minStep + i` (interpolates to `minStep + i + 1`)
   - Ensure final band ends at `x = 1.0`

**Example** (minStep=-2, maxStep=+2, restingSpacePercent=10%):
- Step count: 5
- Resting width total: 0.5 (50% of pad)
- Remaining: 0.5
- Transition count: 4
- Resting width per step: 0.1 (10%)
- Transition width: 0.125 (12.5%)
- Bands: Rest(-2), Trans(-2→-1), Rest(-1), Trans(-1→0), Rest(0), Trans(0→+1), Rest(+1), Trans(+1→+2), Rest(+2)

### Step Mapping: `mapXToStep`

**Location**: `PitchPadUtilities.cpp`, lines 69-101

**Algorithm**:
1. Clamp X to `[0, 1]`
2. Find band containing X:
   - Iterate through bands
   - Check: `x >= band.xStart && x < band.xEnd`
3. Calculate local position within band:
   - `u = (x - band.xStart) * band.invSpan`
4. If resting band:
   - `step = band.step` (integer)
   - `inRestingBand = true`
   - `localT = 0.0`
5. If transition band:
   - `step = band.step + u` (fractional, interpolates to `band.step + 1`)
   - `inRestingBand = false`
   - `localT = u`
6. Return `PitchSample{step, inRestingBand, localT}`

**Example**: X=0.25 in layout above:
- Band: Transition(-2→-1), xStart=0.1, xEnd=0.225
- u = (0.25 - 0.1) / 0.125 = 1.2 → clamped to 1.0
- step = -2 + 1.0 = -1.0 (exactly at Rest(-1) boundary)

### Relative Mode Anchor Management

**Anchor storage** (in `InputProcessor`):
- `pitchPadRelativeAnchorT`: Map `(deviceHandle, layerId, eventId, channel)` → anchor normalized X
- `pitchPadRelativeAnchorStep`: Map `(deviceHandle, layerId, eventId, channel)` → anchor step value

**Anchor setting** (on gesture start):
- Detect `finger1Down` or `finger2Down` for the event
- Store current normalized X (`t`) in `pitchPadRelativeAnchorT`
- Map current X to step using `mapXToStep(layout, t)`
- Store step in `pitchPadRelativeAnchorStep`

**Step offset calculation** (during gesture):
- Read anchor step from `pitchPadRelativeAnchorStep`
- Map current X to step using `mapXToStep(layout, t)`
- Calculate: `stepOffset = currentStep - anchorStep`

**Anchor cleanup**: Anchors persist until gesture ends (finger lifts). No explicit cleanup needed (overwritten on next gesture start).

### Zero Step Calculation

**For Absolute mode**:
- `zeroStep = 0.0` (always center of range)
- In runtime: `stepOffset = sample.step - 0.0 = sample.step`

**For Relative mode**:
- `zeroStep` is not used (anchor-based)
- In runtime: `stepOffset = sample.step - anchorStep`

**Note**: `zeroStep` in `PitchPadConfig` is currently always `0.0f` and not used. Future enhancement: support Left/Right/Custom start positions.

---

## UI Schema Generation

The UI schema is generated by `MappingDefinition::getSchema` to create inspector controls.

### Touchpad Mapping Detection

**Check if mapping is touchpad**:
```cpp
static bool isTouchpadMapping(const juce::ValueTree &mapping) {
  return mapping.getProperty("inputAlias", "").toString().trim().equalsIgnoreCase("Touchpad");
}
```

**Get touchpad event**:
```cpp
const int touchpadEvent = touchpad ? (int)mapping.getProperty("inputTouchpadEvent", 0) : 0;
const bool touchpadInputBool = touchpad && isTouchpadEventBoolean(touchpadEvent);
```

### Note Mapping Schema

**Basic controls** (same as keyboard):
- Type (ComboBox: Note/Expression/Command)
- Channel (Slider: 1-16)
- Note (Slider: 0-127, Format: NoteName)
- Velocity (Slider: 0-127)
- Release Behavior (ComboBox)
- Follow Global Transpose (Toggle)

**Touchpad-specific** (for continuous events):
```cpp
if (touchpad && !touchpadInputBool) {
  schema.push_back(createSeparator("Touchpad: Continuous to Note", ...));
  
  InspectorControl thresh;
  thresh.propertyId = "touchpadThreshold";
  thresh.label = "Threshold";
  thresh.controlType = InspectorControl::Type::Slider;
  thresh.min = 0.0;
  thresh.max = 1.0;
  thresh.step = 0.01;
  schema.push_back(thresh);
  
  InspectorControl trigger;
  trigger.propertyId = "touchpadTriggerAbove";
  trigger.label = "Trigger";
  trigger.controlType = InspectorControl::Type::ComboBox;
  trigger.options[1] = "Below threshold";
  trigger.options[2] = "Above threshold";
  schema.push_back(trigger);
}
```

**Release Behavior restrictions** (for Up events):
```cpp
const bool touchpadUpEvent = touchpad && (touchpadEvent == TouchpadEvent::Finger1Up ||
                                         touchpadEvent == TouchpadEvent::Finger2Up);
if (touchpadUpEvent) {
  // Remove "Send Note Off" option (id 1)
  releaseBehavior.options.erase(1);
}
```

### Expression Mapping Schema

**Basic controls**:
- Type, Channel, Target (CC/PitchBend/SmartScaleBend)
- Controller (for CC)
- Use Custom ADSR Curve (Toggle)

**Touchpad boolean → CC**:
```cpp
if (touchpad && touchpadInputBool && adsrTargetStr.equalsIgnoreCase("CC")) {
  schema.push_back(createSeparator("Touchpad: Boolean to Expression", ...));
  
  InspectorControl valOn;
  valOn.propertyId = "touchpadValueWhenOn";
  valOn.label = "Value when On";
  valOn.controlType = InspectorControl::Type::Slider;
  valOn.min = 0.0;
  valOn.max = 127.0;
  valOn.step = 1.0;
  schema.push_back(valOn);
  
  InspectorControl valOff;
  valOff.propertyId = "touchpadValueWhenOff";
  valOff.label = "Value when Off";
  // ... same as valOn ...
  schema.push_back(valOff);
}
```

**Touchpad continuous → Expression**:
```cpp
if (touchpad && !touchpadInputBool) {
  schema.push_back(createSeparator("Touchpad: Range", ...));
  
  // Input range
  InspectorControl inMin;
  inMin.propertyId = "touchpadInputMin";
  inMin.label = "Input min";
  inMin.controlType = InspectorControl::Type::Slider;
  inMin.min = 0.0;
  inMin.max = 1.0;
  inMin.step = 0.01;
  inMin.widthWeight = 0.5f;
  schema.push_back(inMin);
  
  InspectorControl inMax;
  inMax.propertyId = "touchpadInputMax";
  // ... same as inMin, sameLine = true ...
  schema.push_back(inMax);
  
  // Output range
  const bool rangeIsSteps = adsrTargetStr.equalsIgnoreCase("PitchBend") ||
                            adsrTargetStr.equalsIgnoreCase("SmartScaleBend");
  const double stepMax = rangeIsSteps ? 12.0 : 127.0;
  const double stepMin = rangeIsSteps ? -12.0 : 0.0;
  
  InspectorControl outMin;
  outMin.propertyId = "touchpadOutputMin";
  outMin.label = rangeIsSteps ? "Output min (steps)" : "Output min";
  outMin.min = stepMin;
  outMin.max = stepMax;
  // ... sameLine, widthWeight ...
  schema.push_back(outMin);
  
  InspectorControl outMax;
  outMax.propertyId = "touchpadOutputMax";
  // ... same as outMin ...
  schema.push_back(outMax);
  
  // Pitch Pad controls (only for PitchBend/SmartScaleBend)
  if (rangeIsSteps) {
    schema.push_back(createSeparator("Touchpad: Pitch Pad", ...));
    
    InspectorControl padMode;
    padMode.propertyId = "pitchPadMode";
    padMode.label = "Mode";
    padMode.controlType = InspectorControl::Type::ComboBox;
    padMode.options[1] = "Absolute";
    padMode.options[2] = "Relative";
    schema.push_back(padMode);
    
    InspectorControl restPct;
    restPct.propertyId = "pitchPadRestingPercent";
    restPct.label = "Resting space % per step";
    restPct.controlType = InspectorControl::Type::Slider;
    restPct.min = 0.0;
    restPct.max = 40.0;
    restPct.step = 1.0;
    schema.push_back(restPct);
    
    // Warning if zero range
    int outMinVal = (int)mapping.getProperty("touchpadOutputMin", 0);
    int outMaxVal = (int)mapping.getProperty("touchpadOutputMax", 0);
    if (outMinVal == outMaxVal) {
      InspectorControl warn;
      warn.propertyId = "touchpadPitchPadWarning";
      warn.label = "Warning: effective pitch bend range across the touchpad is zero.";
      warn.controlType = InspectorControl::Type::LabelOnly;
      schema.push_back(warn);
    }
  }
}
```

**Bend (semitones) slider** (for PitchBend, non-touchpad):
```cpp
if (adsrTargetStr.equalsIgnoreCase("PitchBend")) {
  // Hide for touchpad continuous (range comes from Output min/max)
  if (!(touchpad && !touchpadInputBool)) {
    InspectorControl bend;
    bend.propertyId = "data2";
    bend.label = "Bend (semitones)";
    bend.min = -static_cast<double>(pitchBendRange);
    bend.max = static_cast<double>(pitchBendRange);
    bend.valueScaleRange = pitchBendRange;  // Display in semitones
    schema.push_back(bend);
  }
}
```

**Scale Steps slider** (for SmartScaleBend, non-touchpad):
```cpp
if (adsrTargetStr.equalsIgnoreCase("SmartScaleBend")) {
  // Hide for touchpad continuous (range comes from Output min/max)
  if (!(touchpad && !touchpadInputBool)) {
    InspectorControl steps;
    steps.propertyId = "smartStepShift";
    steps.label = "Scale Steps";
    steps.min = -12.0;
    steps.max = 12.0;
    schema.push_back(steps);
  }
}
```

### Command Mapping Schema

**Touchpad Layout Group Solo commands**:
```cpp
const bool isTouchpadSolo = (cmdId >= touchpadSoloMomentary && cmdId <= touchpadSoloClear);

if (isTouchpadSolo) {
  InspectorControl soloTypeCtrl;
  soloTypeCtrl.propertyId = "touchpadSoloType";  // Virtual: maps to CommandID
  soloTypeCtrl.label = "Solo type";
  soloTypeCtrl.controlType = InspectorControl::Type::ComboBox;
  soloTypeCtrl.options[1] = "Hold";
  soloTypeCtrl.options[2] = "Toggle";
  soloTypeCtrl.options[3] = "Set";
  soloTypeCtrl.options[4] = "Clear";
  schema.push_back(soloTypeCtrl);
  
  schema.push_back(createSeparator("Touchpad Layout Group Solo", ...));
  
  InspectorControl groupIdCtrl;
  groupIdCtrl.propertyId = "touchpadLayoutGroupId";
  groupIdCtrl.label = "Layout group";
  groupIdCtrl.controlType = InspectorControl::Type::ComboBox;
  groupIdCtrl.options[0] = "None";
  // ... populate with available groups ...
  schema.push_back(groupIdCtrl);
  
  InspectorControl scopeCtrl;
  scopeCtrl.propertyId = "touchpadSoloScope";
  scopeCtrl.label = "Solo scope";
  scopeCtrl.controlType = InspectorControl::Type::ComboBox;
  scopeCtrl.options[1] = "Global";
  scopeCtrl.options[2] = "Layer (forget on change)";
  scopeCtrl.options[3] = "Layer (remember)";
  schema.push_back(scopeCtrl);
}
```

---

## Tests

### MappingDefinitionTests.cpp

**Test: TouchpadEventNames**
- Verifies all 11 event names are correct
- Tests out-of-range handling (returns "Unknown")

**Test: TouchpadEventOptions**
- Verifies options map contains all 11 events
- Checks specific event names

**Test: TouchpadMappingSchemaHasNoteAndReleaseControls**
- Creates touchpad Note mapping (Finger1Down)
- Verifies schema has `releaseBehavior` and `data1` (Note) controls

**Test: TouchpadFingerUpSchemaNoSendNoteOffOption**
- Creates touchpad Note mapping (Finger1Up)
- Verifies `releaseBehavior` does NOT have option id 1 ("Send Note Off")
- Verifies options 2 and 3 are present

**Test: TouchpadContinuousNoteHasThresholdAndTrigger**
- Creates touchpad Note mapping (Finger1X)
- Verifies schema has `touchpadThreshold` and `touchpadTriggerAbove` controls

**Test: TouchpadContinuousPitchBendDisablesBendSemitonesSlider**
- Creates touchpad Expression mapping (Finger1X, PitchBend)
- Verifies schema does NOT have `data2` control (Bend slider hidden)

**Test: TouchpadContinuousSmartScaleHidesScaleStepsSlider**
- Creates touchpad Expression mapping (Finger1X, SmartScaleBend)
- Verifies schema does NOT have `smartStepShift` control

**Test: ExpressionTouchpadBooleanCCHasValueWhenOnOff**
- Creates touchpad Expression mapping (Finger1Down, CC)
- Verifies schema has `touchpadValueWhenOn` and `touchpadValueWhenOff` controls

**Test: TouchpadContinuousPitchBendHasPitchPadControls**
- Creates touchpad Expression mapping (Finger1X, PitchBend)
- Sets `touchpadOutputMin`/`touchpadOutputMax`
- Verifies schema has `pitchPadMode` and `pitchPadRestingPercent` controls

**Test: TouchpadPitchPadZeroRangeShowsWarning**
- Creates touchpad Expression mapping (Finger1X, PitchBend)
- Sets `touchpadOutputMin` = `touchpadOutputMax` = 0
- Verifies schema has `touchpadPitchPadWarning` label

**Test: TouchpadSoloCommandHasCategoryScopeTypeAndGroup**
- Creates Command mapping (TouchpadLayoutGroupSoloMomentary)
- Verifies schema has `commandCategory`, `touchpadSoloScope`, `touchpadSoloType`, `touchpadLayoutGroupId` controls

**Test: TouchpadSoloTypeHasCorrectOptions**
- Creates Command mapping (TouchpadLayoutGroupSoloMomentary)
- Verifies `touchpadSoloType` has 4 options: Hold, Toggle, Set, Clear

**Test: TouchpadSoloControlsAppearForAllSoloTypes**
- Tests all 4 solo command IDs (18-21)
- Verifies all have solo controls

**Test: SchemaWhenSwitchingToTouchpad_NoControlsThenHasControls**
- Creates Command mapping (SustainMomentary, data1=0)
- Verifies NO touchpad solo controls
- Changes to TouchpadLayoutGroupSoloMomentary (data1=18)
- Verifies touchpad solo controls appear

### GridCompilerTests.cpp

**Test: TouchpadMappingCompiledIntoContext**
- Adds touchpad mapping (Finger1Down → Note)
- Compiles context
- Verifies `touchpadMappings` has 1 entry
- Verifies entry has correct `layerId`, `eventId`, `action.type`, `conversionKind`

**Test: TouchpadNoteReleaseBehaviorApplied**
- Adds touchpad mapping (Finger1Down → Note, "Sustain until retrigger")
- Compiles context
- Verifies `action.releaseBehavior` = `SustainUntilRetrigger`

**Test: TouchpadNoteAlwaysLatchApplied**
- Adds touchpad mapping (Finger2Down → Note, "Always Latch")
- Compiles context
- Verifies `action.releaseBehavior` = `AlwaysLatch`

**Test: TouchpadContinuousEventCompiledAsContinuousToGate**
- Adds touchpad mapping (Finger1X → Note)
- Compiles context
- Verifies `conversionKind` = `ContinuousToGate`

**Test: TouchpadPitchPadConfigCompiledForPitchBend**
- Adds touchpad Expression mapping (Finger1X, PitchBend)
- Sets Pitch Pad properties (`pitchPadMode`, `pitchPadRestingPercent`, `touchpadOutputMin`/`Max`)
- Compiles context
- Verifies `conversionKind` = `ContinuousToRange`
- Verifies `pitchPadConfig` is present
- Verifies config has correct `minStep`, `maxStep`, `restingSpacePercent`, `mode`
- Verifies `cachedPitchPadLayout` is present
- Verifies `sendReleaseValue` defaults to `true` for PitchBend

**Test: TouchpadPitchPadHonoursResetPitchFlag**
- Adds touchpad Expression mapping (Finger1X, PitchBend)
- Sets `sendReleaseValue` = `false`
- Compiles context
- Verifies `action.sendReleaseValue` = `false`

**Test: DisabledTouchpadMappingNotInContext**
- Adds touchpad mapping (Finger1Down → Note)
- Sets `enabled` = `false`
- Compiles context
- Verifies `touchpadMappings` is empty

### InputProcessorTests.cpp

**Test: TouchpadFinger1DownSendsNoteOnThenNoteOff**
- Adds touchpad mapping (Finger1Down → Note C4, "Send Note Off")
- Sends finger down contact
- Verifies Note On (60, channel 1)
- Sends finger up contact
- Verifies Note Off (60, channel 1)

**Test: TouchpadFinger1DownSustainUntilRetrigger_NoNoteOffOnRelease**
- Adds touchpad mapping (Finger1Down → Note, "Sustain until retrigger")
- Sends finger down, then up
- Verifies only Note On (no Note Off)

**Test: TouchpadSustainUntilRetrigger_ReTrigger_NoNoteOffBeforeSecondNoteOn**
- Adds touchpad mapping (Finger1Down → Note, "Sustain until retrigger")
- Sends finger down, up, down again
- Verifies 2 Note On events (no Note Off between)

**Test: TouchpadFinger1UpTriggersNoteOnOnly**
- Adds touchpad mapping (Finger1Up → Note)
- Sends finger up contact
- Verifies Note On only (one-shot, no Note Off)

**Test: TouchpadDrumPadGridMappingCorrectNote**
- Adds touchpad mapping (Finger1Down → Note)
- Adds Drum Pad layout
- Sends finger down in layout region
- Verifies layout consumes finger (no Note On from mapping)

**Test: TouchpadDrumPadFinger2DownMappingSkipped**
- Adds touchpad mapping (Finger2Down → Note)
- Adds Drum Pad layout
- Sends 2 fingers down (F2 in layout region)
- Verifies F2Down mapping is skipped (layout consumes)

**Test: TouchpadDrumPadBoundaryMapping**
- Tests boundary detection for layout regions
- Verifies mappings work outside layout, skipped inside

**Test: TouchpadDrumPadFinger1UpMappingCoexists**
- Adds touchpad mapping (Finger1Up → Note)
- Adds Drum Pad layout
- Sends finger down in layout, then up
- Verifies Finger1Up mapping triggers (Up events not consumed by layouts)

**Test: TouchpadMixerSubRegionCoordinateRemapping**
- Tests coordinate remapping for Mixer layouts
- Verifies mappings use remapped coordinates

**Test: TouchpadDrumPadSubRegionCoordinateRemapping**
- Tests coordinate remapping for Drum Pad layouts
- Verifies mappings use remapped coordinates

### TouchpadPitchPadTest (specialized fixture)

**Test: AbsoluteModeUsesRangeCenterAsZero**
- Adds touchpad Expression mapping (Finger1X, PitchBend, Absolute mode, -2..+2 steps)
- Sends finger at X=0.5
- Verifies PitchBend ≈ 8192 (center)

**Test: RelativeModeAnchorAtCenterMatchesAbsolute**
- Adds touchpad Expression mapping (Finger1X, PitchBend, Relative mode, -2..+2 steps)
- Sends finger at X=0.5 (anchor), then X=0.5 again
- Verifies PitchBend ≈ 8192 (same as Absolute)

**Test: RelativeModeAnchorAt02Maps07ToPBPlus2**
- Adds touchpad Expression mapping (Finger1X, PitchBend, Relative mode, -2..+2 steps)
- Sends finger at X=0.2 (anchor), then X=0.7
- Verifies PitchBend ≈ +2 semitones

**Test: RelativeModeExtrapolatesBeyondConfiguredRange**
- Adds touchpad Expression mapping (Finger1X, PitchBend, Relative mode, -2..+2 steps)
- Sets global PB range = ±6 semitones
- Sends finger at X=0.0 (anchor), then X=1.0
- Verifies PitchBend reaches ±6 semitones (extrapolation)

### PitchPadUtilitiesTests.cpp

**Test: BuildPitchPadLayout_CreatesRestingAndTransitionBands**
- Creates config (minStep=-2, maxStep=+2, restingSpacePercent=10%)
- Builds layout
- Verifies 5 resting bands + 4 transition bands
- Verifies bands cover [0,1]

**Test: MapXToStep_ReturnsCorrectStepForPosition**
- Creates config and layout
- Tests `mapXToStep` at left (0.05), center (0.5), right (0.95)
- Verifies correct step values

---

## Edge Cases & Nuances

### 1. Layout Consumption Priority

**Rule**: Layouts (Mixer, Drum Pad, Chord Pad) consume `Finger1Down` and `Finger2Down` events when finger is in layout region.

**Implementation**:
- Check layout consumption BEFORE processing BoolToGate mappings
- Skip BoolToGate Note mappings if layout consumes finger
- `Finger1Up`/`Finger2Up` events are NOT consumed (mappings always trigger)

**Code**:
```cpp
bool layoutConsumesFinger1Down = tip1 && layout1.has_value();
bool layoutConsumesFinger2Down = tip2 && layout2.has_value();

if (isDownEvent && layoutConsumes)
  continue;  // Skip mapping
```

### 2. Release on Same Frame

**Rule**: If finger down and up happen on same frame, trigger release immediately.

**Implementation**:
```cpp
bool releaseThisFrame = (entry.eventId == TouchpadEvent::Finger1Down && finger1Up) ||
                        (entry.eventId == TouchpadEvent::Finger2Down && finger2Up);
if (releaseThisFrame)
  triggerManualNoteRelease(touchpadInput, act);
```

### 3. Finger Up One-Shot Behavior

**Rule**: `Finger1Up`/`Finger2Up` Note mappings trigger Note On only (no Note Off). Latch does NOT apply.

**Implementation**:
```cpp
else if (!isDownEvent && boolVal) {
  // Finger1Up/Finger2Up: trigger note when finger lifts (one-shot)
  triggerManualNoteOn(touchpadInput, act, false);  // false = latch doesn't apply
}
```

### 4. Change-Only Sending

**Rule**: Continuous Expression mappings only send MIDI when value changes.

**Implementation**:
```cpp
auto itLast = lastTouchpadContinuousValues.find(keyCont);
if (itLast != lastTouchpadContinuousValues.end() && itLast->second == ccVal) {
  break;  // No change, skip send
}
voiceManager.sendCC(...);
lastTouchpadContinuousValues[keyCont] = ccVal;
```

### 5. Event Active State

**Rule**: Continuous events are only active when underlying finger(s) are down.

**Implementation**:
```cpp
bool eventActive = true;
switch (entry.eventId) {
  case TouchpadEvent::Finger1X:
  case TouchpadEvent::Finger1Y:
  case TouchpadEvent::Finger1And2AvgX:
  case TouchpadEvent::Finger1And2AvgY:
    eventActive = tip1;
    break;
  case TouchpadEvent::Finger2X:
  case TouchpadEvent::Finger2Y:
    eventActive = tip2;
    break;
  case TouchpadEvent::Finger1And2Dist:
    eventActive = tip1 && tip2;
    break;
}
```

### 6. Release Value Handling

**Rule**: When event becomes inactive, send release value if `sendReleaseValue` is true, then remove from tracking.

**Implementation**:
```cpp
if (!eventActive) {
  auto itLast = lastTouchpadContinuousValues.find(keyCont);
  if (itLast != lastTouchpadContinuousValues.end()) {
    if (act.sendReleaseValue) {
      if (act.adsrSettings.target == AdsrTarget::CC) {
        voiceManager.sendCC(act.channel, act.adsrSettings.ccNumber, act.releaseValue);
      } else {
        voiceManager.sendPitchBend(act.channel, 8192);  // Center pitch
      }
    }
    lastTouchpadContinuousValues.erase(itLast);  // Remove after sending
  }
  break;
}
```

### 7. Pitch Pad Relative Mode Anchor Persistence

**Rule**: Anchor persists until gesture ends (finger lifts). New gesture starts new anchor.

**Implementation**:
- Anchor is set on `finger1Down`/`finger2Down` for the event
- Anchor is NOT cleared on finger up (just not used)
- New gesture overwrites anchor

### 8. SmartScaleBend Current Note Fallback

**Rule**: If no current playing note, use `lastTriggeredNote`.

**Implementation**:
```cpp
int currentNote = voiceManager.getCurrentPlayingNote(act.channel);
if (currentNote < 0)
  currentNote = lastTriggeredNote;
```

### 9. Input Range Normalization Edge Cases

**Rule**: If `inputMax == inputMin`, `invInputRange` = 0 (prevents division by zero).

**Implementation**:
```cpp
float r = p.inputMax - p.inputMin;
p.invInputRange = (r > 0.0f) ? (1.0f / r) : 0.0f;
```

### 10. Pitch Pad Layout Edge Cases

**Rule**: If `stepCount <= 0`, return empty layout. Final band always ends at 1.0.

**Implementation**:
```cpp
if (stepCount <= 0) {
  return layout;  // Empty
}
// ... build bands ...
if (!layout.bands.empty()) {
  auto &back = layout.bands.back();
  back.xEnd = 1.0f;  // Ensure coverage
  float span = back.xEnd - back.xStart;
  back.invSpan = (span > 0.0f) ? (1.0f / span) : 0.0f;
}
```

### 11. Disabled Mappings

**Rule**: Disabled mappings are NOT compiled into context.

**Implementation**:
- `getEnabledMappingsForLayer` filters disabled mappings
- Double-check: `mapping.getProperty("enabled", true)`

### 12. Layer Activation

**Rule**: Mappings only process if layer is active.

**Implementation**:
```cpp
if (!activeLayersSnapshot[(size_t)entry.layerId])
  continue;
```

### 13. Boolean Expression Envelope Triggering

**Rule**: Boolean Expression mappings only trigger envelope on `true` (finger down). No explicit release (envelope handles it).

**Implementation**:
```cpp
if (boolVal) {
  expressionEngine.triggerEnvelope(...);
  touchpadExpressionActive.insert(...);
}
// No else clause - envelope handles release automatically
```

### 14. ContinuousToGate State Tracking

**Rule**: Use set to track which mappings have sent Note On. Prevent duplicate Note On when threshold crossed multiple times.

**Implementation**:
```cpp
auto key = std::make_tuple(deviceHandle, entry.layerId, entry.eventId);
if (trigger) {
  if (touchpadNoteOnSent.find(key) == touchpadNoteOnSent.end()) {
    touchpadNoteOnSent.insert(key);
    triggerManualNoteOn(...);
  }
} else {
  if (touchpadNoteOnSent.erase(key))
    triggerManualNoteRelease(...);
}
```

### 15. Pitch Pad Extrapolation

**Rule**: Relative mode allows extrapolation beyond configured step range (up to global PB range).

**Implementation**:
- Step offset is NOT clamped before conversion to PitchBend
- Only final PB value is clamped to [0, 16383]
- For PitchBend: `clampedOffset = jlimit(-pbRange, +pbRange, stepOffset)` (clamps to global range, not config range)

---

## Summary

This implementation guide covers:

1. **Data structures**: All types, enums, structs used
2. **Compilation**: ValueTree → TouchpadMappingEntry conversion
3. **Runtime**: Contact processing, conversion kind handling, state tracking
4. **Visualizer**: Reading mappings, drawing pitch pads, CC ranges
5. **Pitch Pad**: Layout building, step mapping, relative mode anchors
6. **UI Schema**: Inspector control generation for all mapping types
7. **Tests**: All test cases with expected behavior
8. **Edge cases**: 15+ nuanced behaviors

**Key principles**:
- Touchpad mappings are compiled separately from keyboard mappings
- Layouts consume Finger Down events (not Up)
- Change-only sending for continuous values
- Pitch Pad provides discrete step control
- Relative mode uses anchor-based offset calculation
- State tracking prevents duplicate MIDI messages

This guide should enable accurate re-implementation of the touchpad mappings feature in another program.
