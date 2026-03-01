#include "ZoneEditorComponent.h"
#include "PresetManager.h"
#include "SettingsManager.h"

ZoneEditorComponent::ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib, SettingsManager *settingsMgr, PresetManager *presetMgr)
    : zoneManager(zoneMgr),
      deviceManager(deviceMgr),
      rawInputManager(rawInputMgr),
      settingsManager(settingsMgr),
      listPanel(zoneMgr),
      propertiesPanel(zoneMgr, deviceMgr, rawInputMgr, scaleLib, presetMgr),
      resizerBar(&horizontalLayout, 1, true) { // Item index 1, vertical bar
  
  addAndMakeVisible(listPanel);
  
  // Setup viewport for properties panel
  addAndMakeVisible(propertiesViewport);
  propertiesViewport.setViewedComponent(&propertiesPanel, false);
  propertiesViewport.setScrollBarsShown(true, false); // Vertical only

  // Add resizer bar
  addAndMakeVisible(resizerBar);

  // Setup horizontal layout: List | Bar | Properties
  horizontalLayout.setItemLayout(0, -0.2, -0.6, -0.3); // Item 0 (List): Min 20%, Max 60%, Preferred 30%
  horizontalLayout.setItemLayout(1, 5, 5, 5);          // Item 1 (Bar): Fixed 5px width
  horizontalLayout.setItemLayout(2, -0.4, -0.8, -0.7); // Item 2 (Properties): Min 40%, Max 80%, Preferred 70%
  
  // Wire up resize callback
  propertiesPanel.onResizeRequested = [this] {
    resized();
  };

  // Preserve viewport scroll when panel rebuilds (e.g. instrument/polyphony change).
  propertiesPanel.onBeforeRebuild = [this] {
    savedPropertiesScrollY = propertiesViewport.getViewPosition().getY();
  };
  propertiesPanel.onAfterRebuild = [this] {
    int maxY = juce::jmax(0, propertiesPanel.getHeight() - propertiesViewport.getViewHeight());
    propertiesViewport.setViewPosition(0, juce::jmin(savedPropertiesScrollY, maxY));
  };

  // Wire up selection callback
  listPanel.onSelectionChanged = [this](std::shared_ptr<Zone> zone, int rowIndex) {
    propertiesPanel.setZone(zone);
    // Persist selection immediately when it changes (not just at shutdown)
    // Use rowIndex from callback instead of getSelectedRow() to avoid stale values
    // Only persist valid selections (>= 0), not deselections (-1)
    juce::Logger::writeToLog("ZoneEditorComponent::onSelectionChanged: rowIndex=" + juce::String(rowIndex) + 
                             ", isLoadingUiState=" + juce::String(isLoadingUiState ? 1 : 0) +
                             ", rememberUiState=" + juce::String(settingsManager && settingsManager->getRememberUiState() ? 1 : 0));
    if (!isLoadingUiState && settingsManager && settingsManager->getRememberUiState() && rowIndex >= 0) {
      juce::Logger::writeToLog("ZoneEditorComponent: Persisting zonesSelectedIndex=" + juce::String(rowIndex));
      settingsManager->setZonesSelectedIndex(rowIndex);
    }
    if (zone && onZoneSelectionChangedForVisualizer) {
      int layerId = juce::jlimit(0, 8, zone->layerID);
      onZoneSelectionChangedForVisualizer(layerId);
    }
    resized(); // Update viewport bounds when zone changes
  };
}

ZoneEditorComponent::~ZoneEditorComponent() {
  if (zoneManager) {
    zoneManager->removeChangeListener(this);
  }
}

void ZoneEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void ZoneEditorComponent::resized() {
  auto area = getLocalBounds().reduced(4);

  // Use StretchableLayoutManager for horizontal split: List | Bar | Properties
  juce::Component* horizontalComps[] = { &listPanel, &resizerBar, &propertiesViewport };
  horizontalLayout.layOutComponents(
    horizontalComps, 3, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
    false, true); // false = horizontal layout (left to right)
  
  // Set content size only; do not set position (0,0) or we reset viewport scroll.
  int contentWidth = propertiesViewport.getWidth() - 15;
  int contentHeight = propertiesPanel.getRequiredHeight();
  propertiesPanel.setSize(contentWidth, contentHeight);
}

void ZoneEditorComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  // Timer backup: if list becomes ready, clear pending selection
  if (source == zoneManager && pendingSelectionIndex >= 0 && listPanel.getNumRows() > 0) {
    // List is ready, clear pending since list panel should have handled it
    stopTimer();
    pendingSelectionIndex = -1;
    loadRetryCount = 0;
  }
}

void ZoneEditorComponent::timerCallback() {
  if (pendingSelectionIndex >= 0) {
    if (listPanel.getNumRows() > 0) {
      // List is ready, restore selection (backup in case list panel didn't handle it)
      juce::Logger::writeToLog("ZoneEditorComponent::timerCallback: List ready (retry " + juce::String(loadRetryCount) + 
                               "), restoring selection to index=" + juce::String(pendingSelectionIndex));
      stopTimer();
      isLoadingUiState = true;
      int indexToSet = juce::jmin(pendingSelectionIndex, listPanel.getNumRows() - 1);
      if (indexToSet >= 0) {
        listPanel.setSelectedRow(indexToSet);
        juce::Logger::writeToLog("ZoneEditorComponent::timerCallback: Selection restored, current selectedRow=" + juce::String(listPanel.getSelectedRow()));
      }
      isLoadingUiState = false;
      pendingSelectionIndex = -1;
      loadRetryCount = 0;
    } else {
      // List still not ready, retry
      loadRetryCount++;
      if (loadRetryCount >= 100) {
        // Max retries reached (5 seconds), give up
        juce::Logger::writeToLog("ZoneEditorComponent::timerCallback: Max retries reached, giving up");
        stopTimer();
        pendingSelectionIndex = -1;
        loadRetryCount = 0;
      }
    }
  } else {
    stopTimer();
  }
}

void ZoneEditorComponent::saveUiState(SettingsManager &settings) const {
  if (!settings.getRememberUiState())
    return;
  // Only save valid selections (>= 0). Invalid selections are already persisted
  // via persist-on-change, so don't overwrite with -1 here.
  int currentRow = listPanel.getSelectedRow();
  juce::Logger::writeToLog("ZoneEditorComponent::saveUiState: currentRow=" + juce::String(currentRow));
  if (currentRow >= 0) {
    juce::Logger::writeToLog("ZoneEditorComponent::saveUiState: Saving zonesSelectedIndex=" + juce::String(currentRow));
    settings.setZonesSelectedIndex(currentRow);
  } else {
    juce::Logger::writeToLog("ZoneEditorComponent::saveUiState: Skipping save (invalid row)");
  }
}

void ZoneEditorComponent::loadUiState(SettingsManager &settings) {
  if (!settings.getRememberUiState())
    return;
  int index = settings.getZonesSelectedIndex();
  juce::Logger::writeToLog("ZoneEditorComponent::loadUiState: loaded index=" + juce::String(index) + 
                           ", listPanel.getNumRows()=" + juce::String(listPanel.getNumRows()));
  if (index < 0)
    index = 0;
  
  // Stop any existing retry timer
  stopTimer();
  loadRetryCount = 0;
  
  // Check if list is populated yet
  if (listPanel.getNumRows() > 0) {
    // List is ready, set selection immediately
    juce::Logger::writeToLog("ZoneEditorComponent::loadUiState: List ready, setting selection to index=" + juce::String(index));
    isLoadingUiState = true;
    int indexToSet = juce::jmin(index, listPanel.getNumRows() - 1);
    if (indexToSet >= 0) {
      listPanel.setSelectedRow(indexToSet);
    }
    isLoadingUiState = false;
    juce::Logger::writeToLog("ZoneEditorComponent::loadUiState: Selection set, current selectedRow=" + juce::String(listPanel.getSelectedRow()));
  } else {
    // List not ready yet - set pending selection on list panel
    // It will restore automatically when list updates
    juce::Logger::writeToLog("ZoneEditorComponent::loadUiState: List not ready, setting pending selection on list panel=" + juce::String(index));
    listPanel.setPendingSelection(index);
    // Keep timer as backup
    pendingSelectionIndex = index;
    startTimer(50);
  }
}
