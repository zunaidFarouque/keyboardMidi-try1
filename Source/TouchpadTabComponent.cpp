#include "TouchpadTabComponent.h"

TouchpadTabComponent::TouchpadTabComponent(TouchpadMixerManager *mgr)
    : manager(mgr), listPanel(mgr), editorPanel(mgr),
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

  listPanel.onSelectionChanged = [this](int index,
                                        const TouchpadMixerConfig *c) {
    editorPanel.setLayout(index, c);
    if (onSelectionChangedForVisualizer)
      onSelectionChangedForVisualizer(index, c ? c->layerId : 0);
    resized();
  };
  editorPanel.setLayout(-1, nullptr);
}

TouchpadTabComponent::~TouchpadTabComponent() {}

void TouchpadTabComponent::refreshVisualizerSelection() {
  if (!onSelectionChangedForVisualizer || !manager)
    return;
  int idx = listPanel.getSelectedLayoutIndex();
  if (idx < 0) {
    onSelectionChangedForVisualizer(-1, 0);
    return;
  }
  auto layouts = manager->getLayouts();
  if (idx < static_cast<int>(layouts.size()))
    onSelectionChangedForVisualizer(idx, layouts[(size_t)idx].layerId);
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
  static constexpr int kEditorMinWidth = 400;
  static constexpr int kEditorMinHeight = 120;
  int scrollW = 10;
  int cw = juce::jmax(kEditorMinWidth, editorViewport.getWidth() - scrollW);
  int ch = juce::jmax(kEditorMinHeight, editorPanel.getPreferredContentHeight());
  editorPanel.setSize(cw, ch);
}
