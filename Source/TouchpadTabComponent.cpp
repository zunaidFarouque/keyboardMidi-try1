#include "TouchpadTabComponent.h"

TouchpadTabComponent::TouchpadTabComponent(TouchpadMixerManager *mgr)
    : manager(mgr),
      listPanel(mgr),
      editorPanel(mgr),
      resizerBar(&layout, 1, true) {
  addAndMakeVisible(listPanel);
  addAndMakeVisible(editorViewport);
  editorViewport.setViewedComponent(&editorPanel, false);
  editorViewport.setScrollBarsShown(true, true);
  editorViewport.setScrollBarThickness(10);
  addAndMakeVisible(resizerBar);

  layout.setItemLayout(0, -0.25, -0.4, -0.3);
  layout.setItemLayout(1, 5, 5, 5);
  layout.setItemLayout(2, -0.6, -0.75, -0.7);

  listPanel.onSelectionChanged = [this](int index, const TouchpadMixerConfig *c) {
    editorPanel.setStrip(index, c);
    if (onSelectionChangedForVisualizer)
      onSelectionChangedForVisualizer(index, c ? c->layerId : 0);
    resized();
  };
  editorPanel.setStrip(-1, nullptr);
}

TouchpadTabComponent::~TouchpadTabComponent() {}

void TouchpadTabComponent::refreshVisualizerSelection() {
  if (!onSelectionChangedForVisualizer || !manager)
    return;
  int idx = listPanel.getSelectedStripIndex();
  if (idx < 0) {
    onSelectionChangedForVisualizer(-1, 0);
    return;
  }
  auto strips = manager->getStrips();
  if (idx < static_cast<int>(strips.size()))
    onSelectionChangedForVisualizer(idx, strips[(size_t)idx].layerId);
  else
    onSelectionChangedForVisualizer(-1, 0);
}

void TouchpadTabComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
}

void TouchpadTabComponent::resized() {
  auto area = getLocalBounds().reduced(4);
  juce::Component *comps[] = {&listPanel, &resizerBar, &editorViewport};
  layout.layOutComponents(comps, 3, area.getX(), area.getY(), area.getWidth(),
                          area.getHeight(), false, true);
  // Content size: fixed height + min width so all params (e.g. Output range) are visible
  static constexpr int kEditorContentHeight = 480;
  static constexpr int kEditorMinWidth = 400;
  int scrollW = 10;
  int cw = juce::jmax(kEditorMinWidth, editorViewport.getWidth() - scrollW);
  int ch = kEditorContentHeight;
  editorPanel.setSize(cw, ch);
}
