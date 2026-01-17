#include "MainComponent.h"
#include <JuceHeader.h>

class OmniKeyApplication : public juce::JUCEApplication {
public:
  OmniKeyApplication() {}
  const juce::String getApplicationName() override { return "OmniKey"; }
  const juce::String getApplicationVersion() override { return "1.0.0"; }
  bool moreThanOneInstanceAllowed() override { return true; }

  void initialise(const juce::String &) override {
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
      auto* mainComp = new MainComponent();
      setContentOwned(mainComp, true);
      setMenuBar(mainComp); // Set MainComponent as menu bar model
      setResizable(true, true);
      centreWithSize(getWidth(), getHeight());
      setVisible(true);
    }

    void closeButtonPressed() override {
      JUCEApplication::getInstance()->systemRequestedQuit();
    }

  private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
  };

private:
  std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(OmniKeyApplication)