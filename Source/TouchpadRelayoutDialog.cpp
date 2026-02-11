#include "TouchpadRelayoutDialog.h"

TouchpadRelayoutDialog::TouchpadRelayoutDialog(RegionChosenCallback cb)
    : callback(std::move(cb)) {
  setSize(300, 220);
}

void TouchpadRelayoutDialog::resized() { layoutRegions(); }

void TouchpadRelayoutDialog::layoutRegions() {
  regions.clear();

  auto bounds = getLocalBounds().reduced(10).toFloat();
  if (bounds.getWidth() <= 0.0f || bounds.getHeight() <= 0.0f)
    return;

  // Tall aspect rectangle for touch surface
  float surfaceWidth = bounds.getWidth();
  float surfaceHeight = bounds.getHeight() - 40.0f; // leave space for label
  juce::Rectangle<float> surface(bounds.getX(),
                                 bounds.getY() + 20.0f,
                                 surfaceWidth,
                                 surfaceHeight);

  // 3x3 selection grid. pixelBounds define the clickable/drawn cell; normBounds
  // define the resulting touchpad region in normalized space.
  const float cellW = surface.getWidth() / 3.0f;
  const float cellH = surface.getHeight() / 3.0f;

  for (int row = 0; row < 3; ++row) {
    for (int col = 0; col < 3; ++col) {
      Region r;

      // Clickable cell in pixel space.
      r.pixelBounds = juce::Rectangle<float>(
          surface.getX() + static_cast<float>(col) * cellW,
          surface.getY() + static_cast<float>(row) * cellH, cellW, cellH);

      // Target region on the touchpad in normalized [0,1] space.
      // Center: full screen.
      if (row == 1 && col == 1) {
        r.normBounds = {0.0f, 0.0f, 1.0f, 1.0f};
      }
      // Top full / Bottom full (horizontal halves).
      else if (row == 0 && col == 1) {
        // Top half
        r.normBounds = {0.0f, 0.0f, 1.0f, 0.5f};
      } else if (row == 2 && col == 1) {
        // Bottom half
        r.normBounds = {0.0f, 0.5f, 1.0f, 0.5f};
      }
      // Left full / Right full (vertical halves).
      else if (row == 1 && col == 0) {
        // Left half
        r.normBounds = {0.0f, 0.0f, 0.5f, 1.0f};
      } else if (row == 1 && col == 2) {
        // Right half
        r.normBounds = {0.5f, 0.0f, 0.5f, 1.0f};
      }
      // Corners: quadrants.
      else if (row == 0 && col == 0) {
        // Left Top
        r.normBounds = {0.0f, 0.0f, 0.5f, 0.5f};
      } else if (row == 0 && col == 2) {
        // Right Top
        r.normBounds = {0.5f, 0.0f, 0.5f, 0.5f};
      } else if (row == 2 && col == 0) {
        // Left Bottom
        r.normBounds = {0.0f, 0.5f, 0.5f, 0.5f};
      } else { // row == 2 && col == 2
        // Right Bottom
        r.normBounds = {0.5f, 0.5f, 0.5f, 0.5f};
      }

      regions.push_back(r);
    }
  }
}

void TouchpadRelayoutDialog::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xf0222222));

  g.setColour(juce::Colours::white);
  g.setFont(14.0f);
  g.drawText("Choose a region for this layout",
             getLocalBounds().removeFromTop(20),
             juce::Justification::centred);

  if (regions.empty())
    layoutRegions();

  // Draw surface outline from union of all pixel regions
  juce::Rectangle<float> surface;
  for (const auto &r : regions) {
    surface = surface.isEmpty() ? r.pixelBounds
                                : surface.getUnion(r.pixelBounds);
  }

  g.setColour(juce::Colours::darkgrey);
  g.fillRect(surface);
  g.setColour(juce::Colours::white.withAlpha(0.8f));
  g.drawRect(surface, 1.0f);

  // Draw candidate regions
  for (size_t i = 0; i < regions.size(); ++i) {
    const auto &r = regions[i];
    // Colour-code by role: center (full), edges (halves), corners (quadrants).
    int row = static_cast<int>(i / 3);
    int col = static_cast<int>(i % 3);
    bool isCenter = (row == 1 && col == 1);
    bool isCorner = (row != 1 && col != 1);

    juce::Colour fillColour;
    if (isCenter)
      fillColour = juce::Colours::darkgreen;
    else if (isCorner)
      fillColour = juce::Colours::darkcyan;
    else
      fillColour = juce::Colours::darkorange;

    g.setColour(fillColour.withAlpha(0.4f));
    g.fillRect(r.pixelBounds.reduced(1.0f));
    g.setColour(fillColour.brighter());
    g.drawRect(r.pixelBounds.reduced(1.0f), 1.0f);
  }
}

void TouchpadRelayoutDialog::mouseUp(const juce::MouseEvent &event) {
  if (!callback)
    return;

  auto pos = event.position;
  for (const auto &r : regions) {
    if (r.pixelBounds.contains(pos)) {
      callback(r.normBounds.getX(), r.normBounds.getY(),
               r.normBounds.getRight(), r.normBounds.getBottom());
      break;
    }
  }
}

