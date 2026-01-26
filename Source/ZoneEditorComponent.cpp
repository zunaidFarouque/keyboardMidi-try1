#include "ZoneEditorComponent.h"

ZoneEditorComponent::ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib)
    : zoneManager(zoneMgr),
      deviceManager(deviceMgr),
      rawInputManager(rawInputMgr),
      globalPanel(zoneMgr),
      listPanel(zoneMgr),
      propertiesPanel(zoneMgr, deviceMgr, rawInputMgr, scaleLib),
      resizerBar(&horizontalLayout, 1, true) { // Item index 1, vertical bar
  
  addAndMakeVisible(globalPanel);
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

  // Use StretchableLayoutManager for horizontal split: List | Bar | Properties
  juce::Component* horizontalComps[] = { &listPanel, &resizerBar, &propertiesViewport };
  horizontalLayout.layOutComponents(
    horizontalComps, 3, area.getX(), area.getY(), area.getWidth(), area.getHeight(),
    false, true); // false = horizontal layout (left to right)
  
  // Set content bounds (accounting for scrollbar width of 15px)
  int contentWidth = propertiesViewport.getWidth() - 15;
  int contentHeight = propertiesPanel.getRequiredHeight();
  propertiesPanel.setBounds(0, 0, contentWidth, contentHeight);
}
