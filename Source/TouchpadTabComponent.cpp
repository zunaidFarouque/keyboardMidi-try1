#include "TouchpadTabComponent.h"
#include "SettingsManager.h"

TouchpadTabComponent::TouchpadTabComponent(TouchpadMixerManager *mgr,
                                           SettingsManager *settingsMgr,
                                           ScaleLibrary *scaleLib)
    : manager(mgr), settingsManager(settingsMgr), groupsPanel(mgr),
      listPanel(mgr), editorPanel(mgr, settingsMgr, scaleLib),
      groupsResizerBar(&layout, 1, true), resizerBar(&layout, 3, true) {
  addAndMakeVisible(groupsPanel);
  addAndMakeVisible(groupsResizerBar);
  addAndMakeVisible(listPanel);
  addAndMakeVisible(editorViewport);
  editorViewport.setViewedComponent(&editorPanel, false);
  editorViewport.setScrollBarsShown(true, true);
  editorViewport.setScrollBarThickness(10);
  addAndMakeVisible(resizerBar);

  groupsPanel.onGroupSelected = [this](int filterGroupId) {
    listPanel.setFilterGroupId(filterGroupId);
  };

  // 5 items: groupsPanel | groupsResizerBar | listPanel | resizerBar | editorViewport
  layout.setItemLayout(0, 80, 350, 120);   // groupsPanel: min 80px, max 350px, preferred 120px
  layout.setItemLayout(1, 5, 5, 5);       // groupsResizerBar
  layout.setItemLayout(2, -0.25, -0.4, -0.3);  // listPanel
  layout.setItemLayout(3, 5, 5, 5);       // resizerBar
  layout.setItemLayout(4, -0.6, -0.75, -0.7);  // editorViewport

  listPanel.onSelectionChanged =
      [this](TouchpadMixerListPanel::RowKind kind, int index,
             const TouchpadMixerConfig *layoutCfg,
             const TouchpadMappingConfig *mappingCfg, int combinedRowIndex) {
        if (kind == TouchpadMixerListPanel::RowKind::Layout) {
          editorPanel.setLayout(index, layoutCfg);
          if (onSelectionChangedForVisualizer)
            onSelectionChangedForVisualizer(
                index, layoutCfg ? layoutCfg->layerId : 0,
                layoutCfg ? layoutCfg->layoutGroupId : 0);
        } else {
          editorPanel.setMapping(index, mappingCfg);
          // Drive visualizer by mapping's layer and group so pitch-pad bands and
          // group filtering show when a mapping is selected.
          if (onSelectionChangedForVisualizer && mappingCfg)
            onSelectionChangedForVisualizer(-1, mappingCfg->layerId,
                                           mappingCfg->layoutGroupId);
        }
        // Persist selection immediately when it changes (not just at shutdown)
        // Use combinedRowIndex from callback instead of getSelectedRowIndex() to avoid stale values
        // Only persist valid selections (>= 0), not deselections (-1)
        juce::Logger::writeToLog("TouchpadTabComponent::onSelectionChanged: combinedRowIndex=" + juce::String(combinedRowIndex) + 
                                 ", isLoadingUiState=" + juce::String(isLoadingUiState ? 1 : 0) +
                                 ", rememberUiState=" + juce::String(settingsManager && settingsManager->getRememberUiState() ? 1 : 0));
        if (!isLoadingUiState && settingsManager && settingsManager->getRememberUiState() && combinedRowIndex >= 0) {
          juce::Logger::writeToLog("TouchpadTabComponent: Persisting touchpadSelectedRow=" + juce::String(combinedRowIndex));
          settingsManager->setTouchpadSelectedRow(combinedRowIndex);
        }
        resized();
      };
  editorPanel.onContentHeightMaybeChanged = [this]() { resized(); };
  editorPanel.setLayout(-1, nullptr);
  
  // Listen for manager changes (for timer backup)
  if (manager) {
    manager->addChangeListener(this);
  }
}

TouchpadTabComponent::~TouchpadTabComponent() {
  if (manager) {
    manager->removeChangeListener(this);
  }
}

void TouchpadTabComponent::refreshVisualizerSelection() {
  if (!onSelectionChangedForVisualizer || !manager)
    return;
  int layoutIdx = listPanel.getSelectedLayoutIndex();
  int mappingIdx = listPanel.getSelectedMappingIndex();
  if (layoutIdx >= 0) {
    auto layouts = manager->getLayouts();
    if (layoutIdx < static_cast<int>(layouts.size())) {
      const auto &layout = layouts[(size_t)layoutIdx];
      onSelectionChangedForVisualizer(layoutIdx, layout.layerId,
                                      layout.layoutGroupId);
      return;
    }
  }
  if (mappingIdx >= 0) {
    auto mappings = manager->getTouchpadMappings();
    if (mappingIdx < static_cast<int>(mappings.size())) {
      const auto &m = mappings[(size_t)mappingIdx];
      onSelectionChangedForVisualizer(-1, m.layerId, m.layoutGroupId);
      return;
    }
  }
  onSelectionChangedForVisualizer(-1, 0, -1);
}

void TouchpadTabComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void TouchpadTabComponent::resized() {
  auto area = getLocalBounds().reduced(4);
  juce::Component *comps[] = {&groupsPanel, &groupsResizerBar, &listPanel,
                              &resizerBar, &editorViewport};
  layout.layOutComponents(comps, 5, area.getX(), area.getY(), area.getWidth(),
                          area.getHeight(), false, true);
  static constexpr int kEditorMinWidth = 400;
  static constexpr int kEditorMinHeight = 120;
  int scrollW = 10;
  int cw = juce::jmax(kEditorMinWidth, editorViewport.getWidth() - scrollW);
  int ch = juce::jmax(kEditorMinHeight, editorPanel.getPreferredContentHeight());
  editorPanel.setSize(cw, ch);
}

void TouchpadTabComponent::saveUiState(SettingsManager &settings) const {
  if (!settings.getRememberUiState())
    return;
  // Only save valid selections (>= 0). Invalid selections are already persisted
  // via persist-on-change, so don't overwrite with -1 here.
  int currentRow = listPanel.getSelectedRowIndex();
  juce::Logger::writeToLog("TouchpadTabComponent::saveUiState: currentRow=" + juce::String(currentRow));
  if (currentRow >= 0) {
    juce::Logger::writeToLog("TouchpadTabComponent::saveUiState: Saving touchpadSelectedRow=" + juce::String(currentRow));
    settings.setTouchpadSelectedRow(currentRow);
  } else {
    juce::Logger::writeToLog("TouchpadTabComponent::saveUiState: Skipping save (invalid row)");
  }
}

void TouchpadTabComponent::loadUiState(SettingsManager &settings) {
  if (!settings.getRememberUiState())
    return;
  int row = settings.getTouchpadSelectedRow();
  if (row < 0)
    row = 0;
  
  // Stop any existing retry timer
  stopTimer();
  loadRetryCount = 0;
  
  // Check if list is populated yet
  if (listPanel.getNumRows() > 0) {
    // List is ready, set selection immediately
    juce::Logger::writeToLog("TouchpadTabComponent::loadUiState: List ready, setting selection to row=" + juce::String(row));
    isLoadingUiState = true;
    int indexToSet = juce::jmin(row, listPanel.getNumRows() - 1);
    if (indexToSet >= 0) {
      listPanel.setSelectedRowIndex(indexToSet);
    }
    isLoadingUiState = false;
    juce::Logger::writeToLog("TouchpadTabComponent::loadUiState: Selection set, current selectedRowIndex=" + juce::String(listPanel.getSelectedRowIndex()));
  } else {
    // List not ready yet - set pending selection on list panel
    // It will restore automatically when list updates
    juce::Logger::writeToLog("TouchpadTabComponent::loadUiState: List not ready, setting pending selection on list panel=" + juce::String(row));
    listPanel.setPendingSelection(row);
    // Keep timer as backup
    pendingSelectionIndex = row;
    startTimer(50);
  }
}

void TouchpadTabComponent::changeListenerCallback(juce::ChangeBroadcaster *source) {
  // Timer backup: if list becomes ready, clear pending selection
  if (source == manager && pendingSelectionIndex >= 0 && listPanel.getNumRows() > 0) {
    // List is ready, clear pending since list panel should have handled it
    stopTimer();
    pendingSelectionIndex = -1;
    loadRetryCount = 0;
  }
}

void TouchpadTabComponent::timerCallback() {
  if (pendingSelectionIndex >= 0) {
    if (listPanel.getNumRows() > 0) {
      // List is ready, restore selection (backup in case list panel didn't handle it)
      juce::Logger::writeToLog("TouchpadTabComponent::timerCallback: List ready (retry " + juce::String(loadRetryCount) + 
                               "), restoring selection to row=" + juce::String(pendingSelectionIndex));
      stopTimer();
      isLoadingUiState = true;
      int indexToSet = juce::jmin(pendingSelectionIndex, listPanel.getNumRows() - 1);
      if (indexToSet >= 0) {
        listPanel.setSelectedRowIndex(indexToSet);
        juce::Logger::writeToLog("TouchpadTabComponent::timerCallback: Selection restored, current selectedRowIndex=" + juce::String(listPanel.getSelectedRowIndex()));
      }
      isLoadingUiState = false;
      pendingSelectionIndex = -1;
      loadRetryCount = 0;
    } else {
      // List still not ready, retry
      loadRetryCount++;
      if (loadRetryCount >= 100) {
        // Max retries reached (5 seconds), give up
        juce::Logger::writeToLog("TouchpadTabComponent::timerCallback: Max retries reached, giving up");
        stopTimer();
        pendingSelectionIndex = -1;
        loadRetryCount = 0;
      }
    }
  } else {
    stopTimer();
  }
}
