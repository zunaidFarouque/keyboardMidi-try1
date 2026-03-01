#pragma once
#include <JuceHeader.h>
#include <functional>

/**
 * A resizer bar that updates a single divider position (0.0–1.0) when dragged.
 * The other divider stays fixed because we only update one percentage.
 */
class PercentageResizerBar : public juce::Component {
public:
  std::function<void(float)> onPositionChange;

  PercentageResizerBar(bool isVertical = true) : isVertical_(isVertical) {
    setRepaintsOnMouseActivity(true);
    setMouseCursor(isVertical ? juce::MouseCursor::LeftRightResizeCursor
                             : juce::MouseCursor::UpDownResizeCursor);
  }

  void paint(juce::Graphics &g) override {
    getLookAndFeel().drawStretchableLayoutResizerBar(
        g, getWidth(), getHeight(), isVertical_, isMouseOver(),
        isMouseButtonDown());
  }

  void mouseDrag(const juce::MouseEvent &e) override {
    if (!onPositionChange)
      return;
    if (auto *p = getParentComponent()) {
      auto pos = e.getEventRelativeTo(p).getPosition();
      float fraction = isVertical_ ? (float)pos.getX() / (float)p->getWidth()
                                  : (float)pos.getY() / (float)p->getHeight();
      fraction = juce::jlimit(0.0f, 1.0f, fraction);
      onPositionChange(fraction);
    }
  }

private:
  bool isVertical_;
};

/**
 * Helper to lay out 5 items (panel, bar, panel, bar, panel) using two divider
 * percentages. When dragging one bar, only that bar's percentage changes.
 */
namespace PercentageSplitLayout {
void apply(
    int x, int y, int width, int height, bool vertically,
    float divider1Fraction, float divider2Fraction,
    juce::Component &panel0, juce::Component &bar1, juce::Component &panel2,
    juce::Component &bar2, juce::Component &panel4,
    int barSize = 5,
    int min0 = 80, int min2 = 150, int min4 = 250);
}
