# UI State Persistence Fix - Context and Problem Statement

## Project Context

You are working on a JUCE C++ application called **MIDIQy** located at:
`d:\_installed\VScode repos\AI\Cursor\keyboardMidi-try1`

The application has a tabbed UI with these main tabs:
- **Mappings** tab (working correctly)
- **Zones** tab (NOT working)
- **Touchpad** tab (NOT working)
- **Settings** tab

## What We're Trying to Achieve

The application has a "Remember UI layout" feature that should persist and restore:
1. ❌ Main window size and position (NOT WORKING)
2. ✅ Panel visibility (Visualizer, Editor, Log) (WORKING)
3. ✅ Active main tab (Mappings/Zones/Touchpad/Settings) (WORKING)
4. ❌ **Selected row in Zones tab** (NOT WORKING)
5. ❌ **Selected row in Touchpad tab** (NOT WORKING)
6. ✅ Selected row in Mappings tab (WORKING)

## Current Implementation

### Settings Storage

Settings are stored in `build/MIDIQy_artefacts/Debug/MIDIQy_data/settings.xml`:

```xml
<MIDIQySettings rememberUiState="1">
  <UIState mainWindowState="154 84 1228 667" 
           mainTabIndex="2" 
           touchpadSelectedRow="1"  <!-- This IS being saved correctly -->
           zonesSelectedIndex="-1"   <!-- This is NOT being saved correctly -->
           mappingsSelectedLayerId="0" 
           mappingsSelectedRow="-1" />
</MIDIQySettings>
```

**Key observation**: `touchpadSelectedRow="1"` IS being saved, but when the app restarts, the selection is NOT restored.

### SettingsManager

`Source/SettingsManager.h` and `Source/SettingsManager.cpp` handle persistence:
- `setTouchpadSelectedRow(int)` / `getTouchpadSelectedRow()` 
- `setZonesSelectedIndex(int)` / `getZonesSelectedIndex()`
- These methods work correctly for saving/loading from XML

### Component Structure

**ZoneEditorComponent** (`Source/ZoneEditorComponent.h` / `.cpp`):
- Contains `ZoneListPanel listPanel` (wraps JUCE `ListBox`)
- Has `saveUiState()` and `loadUiState()` methods
- Has persist-on-change callback: `listPanel.onSelectionChanged` that calls `settingsManager->setZonesSelectedIndex(rowIndex)`
- Has `isLoadingUiState` flag to prevent persist during load
- Has `pendingSelectionIndex` for deferred restoration
- Implements `Timer` for retry mechanism (checks every 50ms, max 100 retries = 5 seconds)

**TouchpadTabComponent** (`Source/TouchpadTabComponent.h` / `.cpp`):
- Contains `TouchpadMixerListPanel listPanel` (wraps JUCE `ListBox`)
- Has `saveUiState()` and `loadUiState()` methods
- Has persist-on-change callback: `listPanel.onSelectionChanged` that calls `settingsManager->setTouchpadSelectedRow(combinedRowIndex)`
- Has `isLoadingUiState` flag to prevent persist during load
- Has `pendingSelectionIndex` for deferred restoration
- Implements `Timer` for retry mechanism (checks every 50ms, max 100 retries = 5 seconds)

**ZoneListPanel** (`Source/ZoneListPanel.h` / `.cpp`):
- Implements `juce::ListBoxModel`
- Callback signature: `std::function<void(std::shared_ptr<Zone>, int rowIndex)> onSelectionChanged`
- `selectedRowsChanged(int lastRowSelected)` calls the callback with `(zone, lastRowSelected)`

**TouchpadMixerListPanel** (`Source/TouchpadMixerListPanel.h` / `.cpp`):
- Implements `juce::ListBoxModel`
- Callback signature: `std::function<void(RowKind, int, const TouchpadMixerConfig*, const TouchpadMappingConfig*, int combinedRowIndex)> onSelectionChanged`
- `selectedRowsChanged(int lastRowSelected)` calls the callback with `combinedRowIndex=lastRowSelected`

### MainComponent Integration

`Source/MainComponent.cpp` calls `loadUiState()` after initialization:

```cpp
// Restore per-tab UI state after settings/presets are loaded.
if (mappingEditor)
  mappingEditor->loadUiState(settingsManager);

// Defer Zones and Touchpad selection restore to allow async list updates to complete
juce::Component::SafePointer<MainComponent> weakThisForUiState(this);
juce::Timer::callAfterDelay(100, [weakThisForUiState]() {
  if (weakThisForUiState == nullptr)
    return;
  if (weakThisForUiState->zoneEditor)
    weakThisForUiState->zoneEditor->loadUiState(weakThisForUiState->settingsManager);
  if (weakThisForUiState->touchpadTab)
    weakThisForUiState->touchpadTab->loadUiState(weakThisForUiState->settingsManager);
});
```

### Current loadUiState Implementation

**TouchpadTabComponent::loadUiState()**:
```cpp
void TouchpadTabComponent::loadUiState(SettingsManager &settings) {
  if (!settings.getRememberUiState())
    return;
  int row = settings.getTouchpadSelectedRow();
  if (row < 0)
    row = 0;
  
  stopTimer();
  loadRetryCount = 0;
  
  if (listPanel.getNumRows() > 0) {
    // List is ready, set selection immediately
    isLoadingUiState = true;
    listPanel.setSelectedRowIndex(row);
    isLoadingUiState = false;
  } else {
    // List not ready yet, store for later restoration and start retry timer
    pendingSelectionIndex = row;
    startTimer(50); // Check every 50ms, max 100 retries = 5 seconds
  }
}
```

**TouchpadTabComponent::timerCallback()**:
```cpp
void TouchpadTabComponent::timerCallback() {
  if (pendingSelectionIndex >= 0) {
    if (listPanel.getNumRows() > 0) {
      // List is ready, restore selection
      stopTimer();
      isLoadingUiState = true;
      int indexToSet = juce::jmin(pendingSelectionIndex, listPanel.getNumRows() - 1);
      if (indexToSet >= 0) {
        listPanel.setSelectedRowIndex(indexToSet);
      }
      isLoadingUiState = false;
      pendingSelectionIndex = -1;
      loadRetryCount = 0;
    } else {
      // List still not ready, retry
      loadRetryCount++;
      if (loadRetryCount >= 100) {
        // Max retries reached (5 seconds), give up
        stopTimer();
        pendingSelectionIndex = -1;
        loadRetryCount = 0;
      }
    }
  } else {
    stopTimer();
  }
}
```

### Persist-on-Change Implementation

**TouchpadTabComponent callback**:
```cpp
listPanel.onSelectionChanged = [this](RowKind kind, int index, 
                                     const TouchpadMixerConfig *layoutCfg,
                                     const TouchpadMappingConfig *mappingCfg, 
                                     int combinedRowIndex) {
  // ... update editor panel ...
  
  // Only persist valid selections (>= 0), not deselections (-1)
  if (!isLoadingUiState && settingsManager && 
      settingsManager->getRememberUiState() && combinedRowIndex >= 0) {
    settingsManager->setTouchpadSelectedRow(combinedRowIndex);
  }
  resized();
};
```

## What's NOT Working

### Problem 1: Selection is NOT restored on app restart

**Observed behavior:**
1. User selects row 1 in Touchpad tab
2. `settings.xml` shows `touchpadSelectedRow="1"` ✅ (saving works!)
3. User closes app
4. User reopens app
5. Touchpad tab is shown (correct tab restored) ✅
6. **BUT: No row is selected** ❌

**Debug logs show:**
- `TouchpadTabComponent::loadUiState: loaded row=1, listPanel.getNumRows()=0`
- `TouchpadTabComponent::loadUiState: List not ready, storing pendingSelectionIndex=1, starting retry timer`
- **NO further logs** - timer callback never fires or list never becomes ready

### Problem 2: Selection might be cleared before save

**Observed behavior:**
- Sometimes `zonesSelectedIndex="-1"` is saved even when user had selected a row
- This suggests selection is being cleared before `saveUiState()` is called

## What We've Tried

1. ✅ Fixed persist-on-change to use callback row index (not `getSelectedRow()`)
2. ✅ Added `isLoadingUiState` flag to prevent persist during load
3. ✅ Added guards to only persist valid selections (>= 0)
4. ✅ Implemented timer-based retry mechanism (50ms intervals, 5 seconds max)
5. ✅ Added change listener approach (as backup)
6. ✅ Added extensive debug logging

**None of these approaches have worked.**

## Key Questions to Investigate

1. **Why doesn't `listPanel.getNumRows()` ever return > 0 during the 5-second retry window?**
   - Is the list panel's model not being populated?
   - Is `getNumRows()` being called before the model is set?
   - Is there a timing issue with when data loads?

2. **Why doesn't the timer callback fire?**
   - Is the timer being stopped prematurely?
   - Is there a message thread issue?

3. **Why does Mappings tab work but Zones/Touchpad don't?**
   - Mappings uses `TableListBox`, Zones/Touchpad use `ListBox`
   - Mappings data loads synchronously, Zones/Touchpad load asynchronously
   - What's different about how Mappings restores selection?

4. **Is `setSelectedRowIndex()` / `setSelectedRow()` actually working?**
   - Maybe the selection is being set but then immediately cleared?
   - Maybe there's a race condition?

## Files to Examine

**Core components:**
- `Source/ZoneEditorComponent.h` / `.cpp`
- `Source/TouchpadTabComponent.h` / `.cpp`
- `Source/ZoneListPanel.h` / `.cpp`
- `Source/TouchpadMixerListPanel.h` / `.cpp`
- `Source/MainComponent.cpp` (around line 407-423)
- `Source/SettingsManager.h` / `.cpp`

**For comparison (working implementation):**
- `Source/MappingEditorComponent.h` / `.cpp` (Mappings tab - this works!)

**Data loading:**
- `Source/StartupManager.cpp` (loads settings and presets)
- `Source/ZoneManager.h` / `.cpp` (manages zones)
- `Source/TouchpadMixerManager.h` / `.cpp` (manages touchpad layouts)

## Success Criteria

The fix should ensure:
1. When user selects a row in Zones or Touchpad tab, it's immediately persisted to `settings.xml`
2. When app restarts, the previously selected row is restored and visible
3. Selection persists through list updates (add/remove items)
4. No race conditions or timing issues

## Build and Test

```bash
cd "d:\_installed\VScode repos\AI\Cursor\keyboardMidi-try1"
cmake --build build --target MIDIQy -j 4
```

Run the app, select a row, close, reopen, and verify selection is restored.
