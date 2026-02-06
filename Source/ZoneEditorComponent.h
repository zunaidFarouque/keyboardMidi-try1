#pragma once
#include "ZoneListPanel.h"
#include "ZonePropertiesPanel.h"
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
  // 1. Data/Managers
  ZoneManager *zoneManager;
  DeviceManager *deviceManager;
  RawInputManager *rawInputManager;

  // 2. Content Components (Must live longer than containers)
  ZoneListPanel listPanel;
  ZonePropertiesPanel propertiesPanel;

  // 3. Containers (Must die first)
  juce::Viewport propertiesViewport;
  int savedPropertiesScrollY = 0;

  // Resizable layout for list and properties
  juce::StretchableLayoutManager horizontalLayout;
  juce::StretchableLayoutResizerBar resizerBar;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ZoneEditorComponent)
};
