#pragma once
#include "ZoneListPanel.h"
#include "ZonePropertiesPanel.h"
#include "GlobalPerformancePanel.h"
#include "ZoneManager.h"
#include "DeviceManager.h"
#include "RawInputManager.h"
#include "ScaleLibrary.h"
#include <JuceHeader.h>

class ZoneEditorComponent : public juce::Component {
public:
  ZoneEditorComponent(ZoneManager *zoneMgr, DeviceManager *deviceMgr, RawInputManager *rawInputMgr, ScaleLibrary *scaleLib);
  ~ZoneEditorComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

private:
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;

  GlobalPerformancePanel globalPanel;
  ZoneListPanel listPanel;
  ZonePropertiesPanel propertiesPanel;
  juce::Viewport propertiesViewport;

  // Resizable layout for list and properties
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar resizerBar;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneEditorComponent)
};
