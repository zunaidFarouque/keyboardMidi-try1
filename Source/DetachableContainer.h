#pragma once
#include <JuceHeader.h>

class DetachableContainer : public juce::Component {
public:
  DetachableContainer(const juce::String &title, juce::Component &contentToWrap);
  ~DetachableContainer() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void setContent(juce::Component &newContent);
  
  juce::String getTitle() const { return titleLabel.getText(); }

  // Visibility control
  void hide();
  void show();
  bool isHidden() const { return isCurrentlyHidden; }

  // Floating window state
  bool isPoppedOut() const { return window != nullptr; }
  void popOut();
  void dock();
  juce::DocumentWindow *getFloatingWindow() const { return window.get(); }

  // Callback when hide state changes
  std::function<void(DetachableContainer*, bool)> onVisibilityChanged;

private:
  class FloatingWindow : public juce::DocumentWindow {
  public:
    FloatingWindow(const juce::String &title, DetachableContainer *container);
    ~FloatingWindow() override;

    void closeButtonPressed() override;

  private:
    DetachableContainer *parentContainer;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FloatingWindow)
  };

  class IconButton : public juce::Button {
  public:
    enum IconType { PopOut, Hide };
    IconButton(const juce::String &buttonName, IconType type);
    void paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

  private:
    IconType iconType;
  };

  juce::Component *content;
  std::unique_ptr<FloatingWindow> window;
  IconButton popOutButton;
  IconButton hideButton;
  juce::Label titleLabel;
  juce::Label placeholderLabel;
  bool isCurrentlyHidden = false;

  void togglePopOut();
  void redock();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DetachableContainer)
};
