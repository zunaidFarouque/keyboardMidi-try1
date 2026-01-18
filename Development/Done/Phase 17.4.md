# Post–17.3: Work Done After Phase 17.3 Implementation

This document summarizes all changes made **after** Phase 17.3 (Fix Chord Generation / Cache Integration) was implemented and confirmed working.

---

## 1. Compilation strategy (O(1) at play time)

**Goal:** Ensure chord/zone handling uses a *compile-at-config, O(1)-at-play* model, matching the rest of the project.

### 1.1 Config-time vs play-time

| When | What runs | Where |
|------|-----------|-------|
| **Config-time** | `ChordUtilities::generateChord`, `ScaleUtilities::calculateMidiNote`, `KeyboardLayoutUtils` | `Zone::rebuildCache(intervals)` |
| **Play-time** | `keyToChordCache.find(keyCode)` + O(k) transpose (k = chord size, ~3–5) | `Zone::getNotesForKey()`, `InputProcessor::processEvent` |

No chord/scale math at play-time; only hash lookups and simple arithmetic.

### 1.2 Removed (to keep play-time cheap)

- **TEST FLIGHT** G-key block in `InputProcessor::processEvent` (hardcoded C major triad).
- **Rebuild-on-miss** in `InputProcessor`: on cache miss we no longer call `rebuildCache`; we fall back to single-note only.
- **O(n) `std::find(keyInZone)`** in `Zone::getNotesForKey` on cache miss; now we simply return `std::nullopt`.
- **All `juce::Logger::writeToLog`** in play-time and rebuild paths:  
  `InputProcessor`, `Zone::rebuildCache`, `Zone::getNotesForKey`, `VoiceManager::noteOn`, `ZonePropertiesPanel` (chordType test lookup and logs).

### 1.3 Code / comment changes

- **`Zone.h`**: Comments on `keyToChordCache`, `rebuildCache`, `getNotesForKey` describing config-time vs play-time and O(1) + O(k).
- **`Zone.cpp`**:  
  - `rebuildCache`: “Compilation: chord generation runs only here.”  
  - `getNotesForKey`: “Play-time: O(1) hash lookup + O(k) transpose. Chords pre-compiled in `rebuildCache`.”  
  - `finalNotes.reserve(relativeNotes.size())` in `getNotesForKey`.
- **`InputProcessor.cpp`**: Comment that `getNotesForKey` is O(1) lookup + O(k) transpose and chords are pre-compiled at config-time.

---

## 2. Conflict indicator in the keyboard visualizer

**Goal:** If a key is in **multiple zones** or in **a zone and a manual mapping**, show it in red (backdrop + text).

### 2.1 Conflict definition

- **Multiple zones:** `keyCode` is in `inputKeyCodes` of ≥ 2 zones.
- **Zone + manual:** `keyCode` is in ≥ 1 zone **and** in `InputProcessor::keyMapping` (any `InputID` with that `keyCode`).

### 2.2 New APIs

- **`ZoneManager::getZoneCountForKey(int keyCode) const`**  
  - Returns how many zones contain `keyCode` in `inputKeyCodes`.  
  - Thread-safe (`zoneLock`).

- **`InputProcessor::hasManualMappingForKey(int keyCode)`**  
  - Returns whether any `keyMapping` entry has that `keyCode`.  
  - Thread-safe (`mapLock`).

### 2.3 Visualizer logic (`VisualizerComponent::paint`)

For each key:

```text
zoneCount   = zoneManager->getZoneCountForKey(keyCode)
hasManual   = inputProcessor && inputProcessor->hasManualMappingForKey(keyCode)
isConflict  = (zoneCount >= 2) || (zoneCount >= 1 && hasManual)
```

- **Underlay:** If `isConflict` → `juce::Colours::red.withAlpha(0.7f)`; else existing zone underlay.
- **Text:** If `isConflict` → red; else if pressed → black; else white.

### 2.4 Files touched

- `ZoneManager.h` / `ZoneManager.cpp`: `getZoneCountForKey`.
- `InputProcessor.h` / `InputProcessor.cpp`: `hasManualMappingForKey`.
- `VisualizerComponent.cpp`: conflict detection and red underlay + text.

---

## 3. Strum buffer fix

**Goal:** In Strum mode, chords should strum **on key press** (bottom to top), and a **new chord** should cancel the previous strum (note-off for played notes, drop pending ones) so fast chord changes don’t layer.

### 3.1 Previous behavior

- Strum mode: zone key press only **buffered** notes; **Spacebar** or **Shift** was required to strum. No MIDI on the zone key alone.
- No cancel when a new chord was played; notes could stack.

### 3.2 New behavior

1. **Auto-strum on zone key press**  
   - Pressing a zone key in Strum mode **immediately** starts strumming that chord, low to high, using `zone->strumSpeedMs` (or 50 ms if 0).

2. **Cancel on new chord**  
   - If another zone key is pressed while a strum is in progress:  
     - `handleKeyUp(lastStrumSource)`: note-off for already-played notes, `cancelPendingNotes` for the rest.  
     - Then start the new chord’s strum.  
   - Avoids muddy layering when changing chords quickly.

3. **Spacebar / Shift**  
   - No longer used as strum triggers; that block was removed.  
   - `noteBuffer` is still updated for the visualizer.

### 3.3 Correct note-off channel

**`VoiceManager`** previously sent note-off with a hardcoded channel. `activeNoteNumbers` now stores `(note, channel)` so note-off uses the right channel per note.

- **Type:**  
  `std::unordered_multimap<InputID, std::pair<int, int>>`  
  - `second.first` = note, `second.second` = channel.

- **Updated:**  
  `noteOn` (single and chord), `strumNotes`, `handleKeyUp`, `panic` to use `(note, channel)` and `sendNoteOff(channel, note)`.

### 3.4 InputProcessor changes

- **Removed:** The whole Spacebar/Shift block that called `voiceManager.strumNotes(noteBuffer, ...)`.
- **New member:** `InputID lastStrumSource{0, 0}` to remember the last strum’s source.
- **Strum branch (zone `PlayMode::Strum`):**
  1. `strumMs = (zone->strumSpeedMs > 0) ? zone->strumSpeedMs : 50`
  2. `voiceManager.handleKeyUp(lastStrumSource)` — cancel previous strum and note-off.
  3. `voiceManager.noteOn(input, *notes, data2, channel, strumMs)` — start new strum.
  4. `lastStrumSource = input`
  5. `noteBuffer = *notes` and `bufferedStrumSpeedMs = strumMs` (for visualizer) under `bufferLock`.

Key-up and `handleKeyUp(input)` are unchanged; they still note-off and cancel when the zone key is released.

### 3.5 StrumEngine

- Unchanged.  
- `triggerStrum` still plays notes in the order given (chords from `getNotesForKey` / `ChordUtilities::generateChord` are low→high for Close voicing).  
- `cancelPendingNotes(source)` is used by `VoiceManager::handleKeyUp`.

### 3.6 Files touched

- `InputProcessor.h`: `lastStrumSource`; comment that `noteBuffer` is for visualizer and strum is on key press.  
- `InputProcessor.cpp`: remove Spacebar/Shift block; Strum branch: `handleKeyUp(lastStrumSource)`, `noteOn(..., strumMs)`, `lastStrumSource = input`, `noteBuffer`/`bufferedStrumSpeedMs` write.  
- `VoiceManager.h`: `activeNoteNumbers` → `multimap<InputID, pair<int,int>>`.  
- `VoiceManager.cpp`: all `activeNoteNumbers` insert/iterate and `sendNoteOff(ch, note)`.

---

## 4. Summary table

| Area | Main change |
|------|-------------|
| **Compilation strategy** | Chords built only in `rebuildCache`; play-time is O(1) lookup + O(k) transpose. Removed test/debug and rebuild-on-miss. |
| **Conflict indicator** | `getZoneCountForKey` and `hasManualMappingForKey`; red underlay+text when a key is in ≥2 zones or zone+manual. |
| **Strum buffer** | Auto-strum on zone key; cancel previous on new chord; `handleKeyUp`/note-off use stored `(note, channel)`; Spacebar/Shift no longer trigger strum. |

---

## 5. Files modified (after 17.3)

- `Source/Zone.h`  
- `Source/Zone.cpp`  
- `Source/ZoneManager.h`  
- `Source/ZoneManager.cpp`  
- `Source/InputProcessor.h`  
- `Source/InputProcessor.cpp`  
- `Source/VoiceManager.h`  
- `Source/VoiceManager.cpp`  
- `Source/VisualizerComponent.cpp`  
- `Source/ZonePropertiesPanel.cpp`

---

*Document generated to record work completed after Phase 17.3 implementation.*
