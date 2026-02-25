#include "MainComponent.h"
#include "CrashLogger.h"
#include <JuceHeader.h>

namespace {
juce::Rectangle<int> getDefaultWindowBounds() {
  auto displays = juce::Desktop::getInstance().getDisplays();
  if (displays.displays.isEmpty())
    return juce::Rectangle<int>(100, 100, 800, 600);

  auto *primary = displays.getPrimaryDisplay();
  if (primary == nullptr)
    return juce::Rectangle<int>(100, 100, 800, 600);

  auto user = primary->userArea;
  int w = (user.getWidth() * 4) / 5;
  int h = (user.getHeight() * 4) / 5;
  juce::Rectangle<int> r(0, 0, w, h);
  r.setCentre(user.getCentre());
  return r;
}

// Clamp a window's bounds to the nearest display's user area.
juce::Rectangle<int> clampToDisplayBounds(const juce::Rectangle<int> &bounds) {
  auto displays = juce::Desktop::getInstance().getDisplays();
  if (displays.displays.isEmpty())
    return bounds;

  // If width/height are invalid, fall back to default bounds.
  if (bounds.getWidth() <= 0 || bounds.getHeight() <= 0)
    return getDefaultWindowBounds();

  juce::Rectangle<int> result = bounds;

  int bestArea = -1;
  juce::Rectangle<int> best;
  for (auto &d : displays.displays) {
    auto area = d.userArea.getIntersection(result);
    int areaPx = area.getWidth() * area.getHeight();
    if (areaPx > bestArea) {
      bestArea = areaPx;
      best = d.userArea;
    }
  }

  if (bestArea <= 0) {
    if (auto *primary = displays.getPrimaryDisplay())
      best = primary->userArea;
  }

  if (!best.isEmpty()) {
    result.setSize(juce::jmin(result.getWidth(), best.getWidth()),
                   juce::jmin(result.getHeight(), best.getHeight()));
    if (result.getX() < best.getX())
      result.setX(best.getX());
    if (result.getRight() > best.getRight())
      result.setX(best.getRight() - result.getWidth());
    if (result.getY() < best.getY())
      result.setY(best.getY());
    if (result.getBottom() > best.getBottom())
      result.setY(best.getBottom() - result.getHeight());
  }

  return result;
}
} // namespace

class MIDIQyApplication : public juce::JUCEApplication {
public:
  MIDIQyApplication() {}
  const juce::String getApplicationName() override { return "MIDIQy"; }
  const juce::String getApplicationVersion() override { return "1.0.0"; }
  bool moreThanOneInstanceAllowed() override { return true; }

  void initialise(const juce::String &) override {
    CrashLogger::installGlobalHandlers();
    mainWindow.reset(new MainWindow(getApplicationName()));
  }

  void shutdown() override { mainWindow = nullptr; }

  class MainWindow : public juce::DocumentWindow {
  public:
    MainWindow(juce::String name)
        : DocumentWindow(
              name,
              juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(
                  juce::ResizableWindow::backgroundColourId),
              DocumentWindow::allButtons) {
      setUsingNativeTitleBar(true);
      auto *mainComp = new MainComponent();
      // Do not auto-resize the window to the content component's size.
      // We control the main window size explicitly (1200x800 default, or
      // restored from saved state).
      setContentOwned(mainComp, false);
      setMenuBar(mainComp); // Set MainComponent as menu bar model
      setResizable(true, true);
      // Initial placement: default centered size. Saved UI state (if any) will
      // be applied later from MainComponent once settings have been loaded
      // from disk by StartupManager.
      setBounds(getDefaultWindowBounds());

      setVisible(true);
    }

    void closeButtonPressed() override {
      JUCEApplication::getInstance()->systemRequestedQuit();
    }

    void moved() override {
      if (auto *mc = dynamic_cast<MainComponent *>(getContentComponent())) {
        SettingsManager &settings = mc->getSettingsManager();
        if (settings.getRememberUiState())
          settings.setMainWindowState(getWindowStateAsString());
      }
      juce::DocumentWindow::moved();
    }

    void resized() override {
      if (auto *mc = dynamic_cast<MainComponent *>(getContentComponent())) {
        SettingsManager &settings = mc->getSettingsManager();
        if (settings.getRememberUiState())
          settings.setMainWindowState(getWindowStateAsString());
      }
      juce::DocumentWindow::resized();
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

private:
  std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(MIDIQyApplication)