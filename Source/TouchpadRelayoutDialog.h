#pragma once

#include <JuceHeader.h>

// Simple modal dialog that shows a normalized touch surface divided into
// common split patterns (vertical, horizontal, 2x2) and lets the user pick
// a target region. All coordinates are returned in 0..1 normalized space.
class TouchpadRelayoutDialog : public juce::Component {
public:
  using RegionChosenCallback =
      std::function<void(float left, float top, float right, float bottom)>;

  explicit TouchpadRelayoutDialog(RegionChosenCallback cb);
  ~TouchpadRelayoutDialog() override = default;

  void paint(juce::Graphics &g) override;
  void resized() override;
  void mouseUp(const juce::MouseEvent &event) override;

private:
  struct Region {
    juce::Rectangle<float> normBounds; // normalized [0,1] space
    juce::Rectangle<float> pixelBounds;
  };

  RegionChosenCallback callback;
  std::vector<Region> regions;

  void layoutRegions();
};

