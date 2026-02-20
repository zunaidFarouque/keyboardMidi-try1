# Touchpad Encoder Feature Specification & Implementation Plan

## Overview
This document defines the encoder feature as a touchpad mapping type that integrates seamlessly with the existing touchpad mapping architecture. The encoder allows users to simulate MIDI rotary encoder behavior using touchpad gestures, supporting rotation (swipe) and push button (multi-touch) functionality.

---

## Architecture Alignment

### Integration with Existing Touchpad Mapping System

The encoder follows the same architecture pattern as existing touchpad mappings:

**Structure**:
- Uses `TouchpadMappingConfig` with shared header fields
- Uses `mapping` ValueTree with `type = "Expression"`
- Uses `expressionCCMode = "Encoder"` to distinguish from Position/Slide modes
- Follows schema-driven UI pattern via `MappingDefinition::getSchema()`

**Header Fields** (shared with all touchpad mappings):
- `name`: User-friendly name for the encoder
- `layerId`: Layer assignment (0-8)
- `layoutGroupId`: Optional group for visibility/solo control
- `midiChannel`: MIDI channel (1-16)
- `region`: Active touchpad area (normalized 0-1 coordinates)
- `zIndex`: Stacking order when regions overlap
- `regionLock`: Lock finger to region until release

**Mapping ValueTree Properties** (Expression type, Encoder mode):
- `type`: "Expression" (required)
- `adsrTarget`: "CC" (required for encoder)
- `expressionCCMode`: "Encoder" (distinguishes from Position/Slide)
- Encoder-specific properties (see below)

---

## Feature Specification

### 1. Rotation Input Configuration

#### 1.1 Movement Axis Selection
**Property**: `encoderAxis` (integer, 0-2)

**Options**:
- **0 = Vertical**: Swipe up/down controls encoder rotation
- **1 = Horizontal**: Swipe left/right controls encoder rotation  
- **2 = Both**: Both axes contribute simultaneously (diagonal swipes combine)

**Default**: 0 (Vertical)

**UI**: ComboBox in "Encoder" section
- Option 1: "Vertical"
- Option 2: "Horizontal"
- Option 3: "Both"

**User Experience**:
- Clear visual indication of active axis
- When "Both" selected, both axes contribute to same CC value
- Tooltip: "Choose which touchpad direction controls the encoder"

#### 1.2 Movement Sensitivity
**Property**: `encoderSensitivity` (float, 0.1-10.0)

**Purpose**: User controls how much finger movement equals ±1 encoder increment.

**Behavior**:
- Sensitivity = 1.0: Default (baseline movement for ±1 increment)
- Sensitivity < 1.0: Less sensitive (requires MORE movement - finer control)
- Sensitivity > 1.0: More sensitive (requires LESS movement - coarser control)

**Default**: 1.0

**UI**: Slider (0.1 to 10.0, step 0.1)
- Label: "Movement sensitivity"
- Tooltip: "Lower = finer control (more movement needed), Higher = coarser control (less movement needed)"

**User Experience**:
- Real-time feedback showing approximate movement needed
- Visual guide: "Move ~X% of pad for ±1 step" (calculated from sensitivity)

#### 1.3 Active Region
**Property**: Uses shared `region` from `TouchpadMappingConfig` header

**Behavior**: Only gestures within the defined region trigger encoder rotation.

**Default**: Full touchpad (0, 0, 1, 1)

**UI**: Uses existing region selector (shared with all touchpad mappings)

**User Experience**:
- Visual region overlay on touchpad preview
- Ability to resize/position region
- Clear indication of active vs inactive areas

---

### 2. MIDI Output Mode Selection

#### 2.1 Absolute Mode
**Property**: `encoderOutputMode` (string, "Absolute" | "Relative" | "NRPN")

**Purpose**: Send absolute CC values (0-127) representing encoder position.

**Configuration**:
- **Output Range**: Uses existing `touchpadOutputMin` / `touchpadOutputMax` (default: 0-127)
- **Initial Value**: Starting CC value when encoder first activated (default: 64)
- **Wrap Behavior**: `encoderWrap` (boolean)
  - `false`: Clamp to 0-127 range (default)
  - `true`: Wrap around (127+1 → 0, 0-1 → 127)

**Default**: "Absolute"

**UI**: ComboBox in "Encoder" section
- Option 1: "Absolute"
- Option 2: "Relative"
- Option 3: "High-Resolution (NRPN)"

**User Experience**:
- Clear indication that this sends absolute positions
- Warning tooltip if wrap enabled: "May cause jumps if software doesn't match hardware position"

#### 2.2 Relative Mode
**Property**: `encoderRelativeEncoding` (integer, 0-3)

**Purpose**: Send relative increments/decrements (native encoder behavior, avoids parameter jumps).

**Encoding Methods**:
- **0 = Offset Binary (64 ± n)**: 
  - 64 = no movement
  - 65-127 = clockwise (+1 to +63)
  - 63-0 = counter-clockwise (-1 to -64)
- **1 = Sign Magnitude**:
  - Bit 7 = direction (0=positive, 1=negative)
  - Bits 0-6 = step count (0-127)
- **2 = Two's Complement**:
  - +1 = 1, -1 = 127, +2 = 2, -2 = 126, etc.
- **3 = Simple Incremental (0/127)**:
  - Clockwise = 1
  - Counter-clockwise = 127 (represents -1)

**Default**: 0 (Offset Binary - most compatible)

**UI**: ComboBox (shown only when output mode = "Relative")
- Option 1: "Offset Binary (64 ± n)"
- Option 2: "Sign Magnitude"
- Option 3: "Two's Complement"
- Option 4: "Simple Incremental (0/127)"

**User Experience**:
- Tooltip for each encoding explaining compatibility
- Visual preview showing example CC values
- Recommended encoding highlighted (Offset Binary)

#### 2.3 High-Resolution Mode (NRPN)
**Property**: `encoderNRPNNumber` (integer, 0-16383)

**Purpose**: Support 14-bit precision (16,384 steps) for professional applications.

**Configuration**:
- **NRPN Number**: Parameter number (0-16383)
- **MSB/LSB**: Automatically handled (splits 14-bit value across two CC messages)

**Default**: 0

**UI**: Slider (shown only when output mode = "NRPN")
- Label: "NRPN Parameter Number"
- Range: 0-16383
- Tooltip: "Requires software/hardware supporting NRPN"

**User Experience**:
- Advanced option (collapsed by default)
- Clear indication of NRPN compatibility requirements

---

### 3. Push Button Configuration

#### 3.1 Push Detection Method
**Property**: `encoderPushDetection` (integer, 0-2)

**Options**:
- **0 = Two-Finger Tap**: Two fingers simultaneously in encoder region (default)
- **1 = Three-Finger Tap**: Three fingers simultaneously
- **2 = Pressure Threshold**: Single finger with pressure above threshold (future)

**Default**: 0 (Two-Finger Tap)

**UI**: ComboBox in "Push Button" section
- Option 1: "Two-finger tap"
- Option 2: "Three-finger tap"
- Option 3: "Pressure threshold" (disabled/grayed if not supported)

**User Experience**:
- Visual guide showing gesture pattern
- Test button to verify detection works
- Clear indication of current detection method

#### 3.2 Push Output Type
**Property**: `encoderPushOutputType` (string, "CC" | "Note" | "ProgramChange")

**Purpose**: Define what MIDI message is sent when push button activates.

**Options**:
- **CC (Control Change)**:
  - Uses `encoderPushValue` (0-127)
  - Behavior depends on `encoderPushMode`
- **Note**:
  - Uses `encoderPushNote` (0-127, MIDI note number)
  - Uses `encoderPushChannel` (1-16, MIDI channel)
  - Sends Note On (velocity 127) on press, Note Off on release
- **Program Change**:
  - Uses `encoderPushProgram` (0-127, program number)
  - Uses `encoderPushChannel` (1-16, MIDI channel)

**Default**: "CC"

**UI**: ComboBox in "Push Button" section
- Option 1: "Control Change (CC)"
- Option 2: "Note"
- Option 3: "Program Change"

**User Experience**:
- Conditional controls appear based on output type selection
- Clear separation between rotation output and push output
- Independent channel/CC number configuration

#### 3.3 Push Behavior Mode
**Property**: `encoderPushMode` (integer, 0-3)

**Purpose**: Define how push button behaves over time.

**Options**:
- **0 = Off**: Push button disabled
- **1 = Momentary**: Press sends "on", release sends "off"
- **2 = Toggle**: Each press alternates on/off state
- **3 = Trigger**: Each press sends single pulse (no release message)

**Default**: 0 (Off)

**UI**: ComboBox (shown only when push output type is not "Off")
- Option 1: "Off"
- Option 2: "Momentary"
- Option 3: "Toggle"
- Option 4: "Trigger"

**User Experience**:
- Visual feedback of current state (for toggle mode)
- Real-time indication when push is active
- Clear state indicator in UI

#### 3.4 Push Output Values
**Properties**:
- `encoderPushValue` (integer, 0-127): CC value when push fires (for CC output type)
- `encoderPushNote` (integer, 0-127): MIDI note number (for Note output type)
- `encoderPushProgram` (integer, 0-127): Program number (for Program Change output type)
- `encoderPushChannel` (integer, 1-16): MIDI channel (for Note/Program Change)

**Defaults**:
- `encoderPushValue`: 127
- `encoderPushNote`: 60 (C4)
- `encoderPushProgram`: 0
- `encoderPushChannel`: Same as rotation channel (from header)

**UI**: Sliders (shown conditionally based on output type)
- CC: "Push value" slider (0-127)
- Note: "Note" slider (0-127, NoteName format), "Channel" slider (1-16)
- Program Change: "Program" slider (0-127), "Channel" slider (1-16)

---

### 4. Step Size Configuration

#### 4.1 Step Size Multiplier
**Property**: `encoderStepSize` (integer, 1-16)

**Purpose**: Control how many CC increments sent per detected movement unit.

**Behavior**:
- Step size = 1: Fine control (1 movement = 1 increment)
- Step size = 8: Coarse control (1 movement = 8 increments)

**Default**: 1

**UI**: Slider in "Encoder" section
- Label: "Step size"
- Range: 1-16
- Tooltip: "Multiplies base movement sensitivity"

**Note**: When `encoderAxis = 2` (Both), separate step sizes:
- `encoderStepSizeX` (1-16): Horizontal step size
- `encoderStepSizeY` (1-16): Vertical step size

**User Experience**:
- Separate X/Y controls appear when "Both" axis selected
- Clear indication that this multiplies movement sensitivity
- Tooltip explaining interaction with sensitivity setting

---

### 5. Dead Zone Configuration

#### 5.1 Movement Dead Zone
**Property**: `encoderDeadZone` (float, 0.0-0.5)

**Purpose**: Ignore small movements to prevent accidental activation.

**Behavior**: Movements smaller than this threshold (as percentage of axis) are ignored.

**Default**: 0.0 (no dead zone)

**UI**: Slider in "Advanced" section (collapsible)
- Label: "Dead zone"
- Range: 0-50%
- Tooltip: "Ignore movements smaller than this threshold"

**User Experience**:
- Visual representation on touchpad preview
- Test mode to verify dead zone behavior
- Advanced option (collapsed by default)

---

## UI Organization

### Schema Structure (Following Existing Pattern)

The encoder configuration follows the same schema-driven pattern as other Expression modes:

**Section Order**:
1. **Header Section** (shared with all touchpad mappings):
   - Name, Layer, Layout Group, Channel, Z-index, Enabled
   - Region selector

2. **Expression Section**:
   - Target: "CC" (fixed for encoder)
   - Controller: CC number (data1)
   - **CC input mode**: ComboBox ("Position" | "Slide" | "Encoder")
     - When "Encoder" selected, show encoder-specific controls

3. **Encoder Section** (shown when CC input mode = "Encoder"):
   - **Rotation**:
     - Axis (Vertical/Horizontal/Both)
     - Movement sensitivity
     - Step size (or Step size X/Y when Both selected)
   - **Output**:
     - Output mode (Absolute/Relative/NRPN)
     - Relative encoding (when Relative selected)
     - Wrap at 0/127 (when Absolute selected)
     - Output min/max (uses existing touchpadOutputMin/Max)

4. **Push Button Section** (shown when encoder configured):
   - Detection method (Two-finger/Three-finger)
   - Output type (CC/Note/Program Change)
   - Behavior mode (Off/Momentary/Toggle/Trigger)
   - Output values (conditional based on type)

5. **Advanced Section** (collapsible):
   - Dead zone

### Visual Feedback

**Requirements**:
- **Touchpad Preview**: Show encoder region overlay
- **Real-Time Value Display**: Current encoder CC value and push state
- **Gesture Visualization**: Highlight active swipe direction and finger count
- **MIDI Monitor**: Display sent MIDI messages (optional, in settings)

---

## Implementation Plan

### Phase 1: Data Structures & Compilation

#### 1.1 Update TouchpadConversionParams
**File**: `Source/MappingTypes.h`

**Changes**:
- Add encoder-specific fields to `TouchpadConversionParams`:
  ```cpp
  // EncoderCC fields
  uint8_t encoderAxis = 0;              // 0=Vertical, 1=Horizontal, 2=Both
  float encoderSensitivity = 1.0f;      // Movement scale (0.1-10.0)
  int encoderStepSize = 1;               // Used for Vertical/Horizontal
  int encoderStepSizeX = 1;              // Used for Both axis
  int encoderStepSizeY = 1;              // Used for Both axis
  uint8_t encoderOutputMode = 0;         // 0=Absolute, 1=Relative, 2=NRPN
  uint8_t encoderRelativeEncoding = 0;   // 0-3 encoding method
  bool encoderWrap = false;              // Wrap at boundaries
  uint8_t encoderPushDetection = 0;      // 0=Two-finger, 1=Three-finger
  uint8_t encoderPushOutputType = 0;     // 0=CC, 1=Note, 2=ProgramChange
  uint8_t encoderPushMode = 0;           // 0=Off, 1=Momentary, 2=Toggle, 3=Trigger
  int encoderPushValue = 127;            // CC value
  int encoderPushNote = 60;              // Note number
  int encoderPushProgram = 0;            // Program number
  int encoderPushChannel = 1;            // Channel for Note/ProgramChange
  float encoderDeadZone = 0.0f;         // Dead zone threshold (0-0.5)
  ```

#### 1.2 Update MappingDefaults
**File**: `Source/MappingDefaults.h`

**Changes**:
- Add default values for all encoder properties
- Follow existing naming convention (`Encoder*`)

#### 1.3 Update GridCompiler
**File**: `Source/GridCompiler.cpp`

**Changes**:
- In `compileTouchpadMappingFromValueTree()`, when `expressionCCMode = "Encoder"`:
  - Read all encoder properties from mapping ValueTree
  - Populate `TouchpadConversionParams` encoder fields
  - Set `conversionKind = TouchpadConversionKind::EncoderCC`
  - Handle auto-promotion of boolean events to continuous (Finger1Y) if needed

**Pattern**: Follow existing Slide mode compilation pattern

---

### Phase 2: Schema Definition

#### 2.1 Update MappingDefinition Schema
**File**: `Source/MappingDefinition.cpp`

**Changes**:
- In `getSchema()`, when `expressionCCMode = "Encoder"`:
  - Add "Encoder" separator
  - Add rotation controls (Axis, Sensitivity, Step size)
  - Add output mode selector (Absolute/Relative/NRPN)
  - Add conditional controls based on output mode
  - Add push button section
  - Add advanced section (dead zone)

**Pattern**: Follow existing Slide mode schema pattern (lines 369-432)

#### 2.2 Update Defaults Map
**File**: `Source/MappingDefinition.cpp`

**Changes**:
- Add all encoder properties to `getDefaultsMap()`
- Use `MappingDefaults::Encoder*` constants

---

### Phase 3: Editor Component Integration

#### 3.1 Update TouchpadMixerEditorComponent
**File**: `Source/TouchpadMixerEditorComponent.cpp`

**Changes**:
- In `getConfigValue()`:
  - Handle encoder property ID conversions (ComboBox IDs ↔ stored values)
  - Follow existing pattern for `encoderAxis`, `encoderPushMode`
- In `applyConfigValue()`:
  - Handle encoder property writes
  - Convert ComboBox IDs back to stored values
  - Trigger `rebuildUI()` when encoder mode changes

**Pattern**: Follow existing `expressionCCMode`, `encoderAxis`, `encoderPushMode` handling

---

### Phase 4: Runtime Processing

#### 4.1 Update InputProcessor - Rotation Logic
**File**: `Source/InputProcessor.cpp`

**Changes**:
- In `processTouchpadContacts()`, encoder rotation branch:
  - Apply `encoderSensitivity` to movement delta before computing stepCount
  - Handle `encoderAxis` selection (Vertical/Horizontal/Both)
  - Apply `encoderDeadZone` threshold
  - Compute stepCount based on axis and sensitivity
  - Apply step size multiplier
  - Generate MIDI based on output mode:
    - **Absolute**: Update internal value, send CC with wrap/clamp
    - **Relative**: Encode delta using selected encoding method, send CC
    - **NRPN**: Send NRPN messages (MSB/LSB)

**Pattern**: Follow existing encoder logic but with new sensitivity/dead zone support

#### 4.2 Update InputProcessor - Push Logic
**File**: `Source/InputProcessor.cpp`

**Changes**:
- Detect push gesture based on `encoderPushDetection`:
  - Two-finger: `inRegion.size() >= 2`
  - Three-finger: `inRegion.size() >= 3`
- Handle push modes:
  - **Momentary**: Send on press, send off on release
  - **Toggle**: Track state, alternate on each press
  - **Trigger**: Send single message on press
- Send appropriate MIDI based on `encoderPushOutputType`:
  - **CC**: Send CC message
  - **Note**: Send Note On/Off
  - **Program Change**: Send Program Change message

**Pattern**: Extend existing push detection logic (currently two-finger only)

---

### Phase 5: State Management

#### 5.1 Add Encoder State Maps
**File**: `Source/InputProcessor.h`

**Changes**:
- Add state maps for encoder:
  ```cpp
  // Encoder rotation state
  std::map<key, int> lastTouchpadEncoderCCValues;      // Current CC value
  std::map<key, float> touchpadEncoderPrevPos;          // Previous position (single axis)
  std::map<key, float> touchpadEncoderPrevPosX;         // Previous X (Both axis)
  std::map<key, float> touchpadEncoderPrevPosY;         // Previous Y (Both axis)
  
  // Encoder push state
  std::map<key, bool> touchpadEncoderPushOn;             // Toggle state
  std::map<key, bool> touchpadEncoderTwoFingersPrev;    // Previous two-finger state
  std::map<key, bool> touchpadEncoderThreeFingersPrev;   // Previous three-finger state
  ```

**Pattern**: Follow existing encoder state map pattern

---

### Phase 6: Testing

#### 6.1 Unit Tests
**File**: `Source/Tests/InputProcessorTests.cpp`

**Add Tests**:
- `TouchpadEncoderCC_AbsoluteMode_SendsCC`
- `TouchpadEncoderCC_RelativeMode_OffsetBinary_SendsIncrements`
- `TouchpadEncoderCC_RelativeMode_SignMagnitude_SendsIncrements`
- `TouchpadEncoderCC_RelativeMode_TwosComplement_SendsIncrements`
- `TouchpadEncoderCC_RelativeMode_SimpleIncremental_SendsIncrements`
- `TouchpadEncoderCC_Sensitivity_ControlsMovementRequired`
- `TouchpadEncoderCC_DeadZone_IgnoresSmallMovements`
- `TouchpadEncoderCC_PushButton_TwoFinger_Momentary_SendsCC`
- `TouchpadEncoderCC_PushButton_Toggle_AlternatesState`
- `TouchpadEncoderCC_PushButton_Note_SendsNoteOnOff`
- `TouchpadEncoderCC_BothAxis_CombinesXYMovement`

**Pattern**: Follow existing `TouchpadEncoderCC_*` test patterns

#### 6.2 Compiler Tests
**File**: `Source/Tests/GridCompilerTests.cpp`

**Add Tests**:
- `TouchpadEncoderCC_CompilesAllProperties`
- `TouchpadEncoderCC_DefaultsAppliedCorrectly`
- `TouchpadEncoderCC_RelativeEncoding_StoredCorrectly`

**Pattern**: Follow existing `TouchpadExpressionCCModeEncoder_CompilesEncoderCC` test

---

### Phase 7: UI Polish

#### 7.1 Conditional Controls
**Implementation**:
- Show/hide controls based on selections:
  - Relative encoding selector: only when output mode = "Relative"
  - Wrap toggle: only when output mode = "Absolute"
  - NRPN number: only when output mode = "NRPN"
  - Push controls: only when push mode != "Off"
  - Step size X/Y: only when axis = "Both"

**Pattern**: Use `rebuildUI()` when relevant properties change

#### 7.2 Tooltips & Help
**Implementation**:
- Add tooltips to all encoder controls
- Add context-sensitive help
- Add MIDI encoding method explanations

**Pattern**: Follow existing tooltip patterns in inspector

---

## Migration & Backward Compatibility

### Existing Encoder Mappings
**Handling**:
- Existing encoder mappings use old property names
- Add migration logic in `GridCompiler`:
  - Map old `encoderStepSize` → new `encoderStepSize`
  - Map old `encoderAxis` → new `encoderAxis`
  - Set defaults for new properties (`encoderSensitivity = 1.0`, etc.)

**Pattern**: Follow existing backward compatibility patterns

---

## Success Criteria

### Functionality
- All MIDI encoder protocols supported (Absolute, Relative with 4 encodings, NRPN)
- Movement sensitivity works correctly (user can control movement-to-increment ratio)
- Push button works with all detection methods and output types
- Dead zone prevents accidental activation
- Both axis mode combines X/Y movement correctly

### Usability
- Configuration is intuitive and discoverable
- Conditional controls appear/disappear appropriately
- Tooltips explain all options clearly
- Visual feedback shows encoder state

### Performance
- Low latency (<10ms from gesture to MIDI)
- Smooth, responsive feel
- No performance impact with multiple encoders

### Code Quality
- Follows existing touchpad mapping architecture patterns
- Reuses existing infrastructure (schema, editor, compiler)
- Maintains backward compatibility
- Comprehensive test coverage

---

## File Changes Summary

### Modified Files
1. `Source/MappingTypes.h` - Add encoder fields to TouchpadConversionParams
2. `Source/MappingDefaults.h` - Add encoder default values
3. `Source/MappingDefinition.cpp` - Add encoder schema controls
4. `Source/GridCompiler.cpp` - Compile encoder properties
5. `Source/InputProcessor.h` - Add encoder state maps
6. `Source/InputProcessor.cpp` - Implement encoder rotation and push logic
7. `Source/TouchpadMixerEditorComponent.cpp` - Handle encoder property read/write

### New Test Files
- Additional tests in `Source/Tests/InputProcessorTests.cpp`
- Additional tests in `Source/Tests/GridCompilerTests.cpp`

### No New Files Required
- Encoder integrates into existing touchpad mapping system
- No new UI components needed (uses existing inspector pattern)
- No new data structures needed (extends existing TouchpadConversionParams)

---

## Implementation Order

1. **Phase 1**: Data structures and compilation (foundation)
2. **Phase 2**: Schema definition (UI structure)
3. **Phase 3**: Editor integration (UI wiring)
4. **Phase 4**: Runtime processing (core functionality)
5. **Phase 5**: State management (persistence)
6. **Phase 6**: Testing (validation)
7. **Phase 7**: UI polish (user experience)

Each phase builds on the previous, allowing incremental testing and validation.
