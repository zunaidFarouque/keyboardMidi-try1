#include "ZoneEditorComponent.h"

ZoneEditorComponent::ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr)
    : zoneManager(zoneMgr),
      deviceManager(deviceMgr),
      rawInputManager(rawInputMgr),
      globalPanel(zoneMgr),
      listPanel(zoneMgr),
      propertiesPanel(deviceMgr, rawInputMgr) {
  
  addAndMakeVisible(globalPanel);
  addAndMakeVisible(listPanel);
  addAndMakeVisible(propertiesPanel);

  // Wire up selection callback
  listPanel.onSelectionChanged = [this](std::shared_ptr<Zone> zone) {
    propertiesPanel.setZone(zone);
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

  // Split remaining space: left = list, right = properties
  auto leftArea = area.removeFromLeft(250);
  listPanel.setBounds(leftArea);
  
  area.removeFromLeft(4);
  propertiesPanel.setBounds(area);
}
