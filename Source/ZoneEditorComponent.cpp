#include "ZoneEditorComponent.h"

ZoneEditorComponent::ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr),
      deviceManager(deviceMgr),
      rawInputManager(rawInputMgr),
      globalPanel(zoneMgr),
      listPanel(zoneMgr),
      propertiesPanel(deviceMgr, rawInputMgr, scaleLib) {
  
  addAndMakeVisible(globalPanel);
  addAndMakeVisible(listPanel);
  
  // Setup viewport for properties panel
  addAndMakeVisible(propertiesViewport);
  propertiesViewport.setViewedComponent(&propertiesPanel, false);
  propertiesViewport.setScrollBarsShown(true, false); // Vertical only
  
  // Wire up resize callback
  propertiesPanel.onResizeRequested = [this] {
    resized();
  };

  // Wire up selection callback
  listPanel.onSelectionChanged = [this](std::shared_ptr<Zone> zone) {
    propertiesPanel.setZone(zone);
    resized(); // Update viewport bounds when zone changes
  };
}

ZoneEditorComponent::~ZoneEditorComponent() {
}

void ZoneEditorComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void ZoneEditorComponent::resized() {
  auto area = getLocalBounds().reduced(4);

  // Global panel at top
  globalPanel.setBounds(area.removeFromTop(50));
  area.removeFromTop(4);

  // Split remaining space: left = list, right = properties viewport
  auto leftArea = area.removeFromLeft(250);
  listPanel.setBounds(leftArea);
  
  area.removeFromLeft(4);
  
  // Set viewport bounds
  propertiesViewport.setBounds(area);
  
  // Set content bounds (accounting for scrollbar width of 15px)
  int contentWidth = propertiesViewport.getWidth() - 15;
  int contentHeight = propertiesPanel.getRequiredHeight();
  propertiesPanel.setBounds(0, 0, contentWidth, contentHeight);
}
