#include "DetachableContainer.h"

DetachableContainer::FloatingWindow::FloatingWindow(const juce::String &title, DetachableContainer *container)
    : juce::DocumentWindow(title, juce::Colour(0xff2a2a2a), juce::DocumentWindow::allButtons),
      parentContainer(container) {
  setUsingNativeTitleBar(true);
  setResizable(true, true);
  setDropShadowEnabled(true);
}

DetachableContainer::FloatingWindow::~FloatingWindow() {
}

void DetachableContainer::FloatingWindow::closeButtonPressed() {
  if (parentContainer) {
    parentContainer->redock();
  }
}

DetachableContainer::IconButton::IconButton(const juce::String &buttonName, IconType type)
    : juce::Button(buttonName), iconType(type) {
  setSize(24, 24);
}

void DetachableContainer::IconButton::paintButton(juce::Graphics &g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) {
  auto bounds = getLocalBounds().toFloat().reduced(2.0f);
  
  // Draw subtle background when highlighted
  if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown) {
    g.setColour(juce::Colours::white.withAlpha(0.1f));
    g.fillRoundedRectangle(bounds, 3.0f);
  }
  
  // Set icon color
  g.setColour(shouldDrawButtonAsHighlighted ? juce::Colours::white : juce::Colours::lightgrey);

  auto iconBounds = bounds.reduced(4.0f);

  if (iconType == PopOut) {
    // Draw pop-out icon (arrow pointing up-right with window)
    juce::Path path;
    path.addArrow({iconBounds.getX(), iconBounds.getBottom(), iconBounds.getRight(), iconBounds.getY()}, 2.0f, 6.0f, 4.0f);
    g.strokePath(path, juce::PathStrokeType(2.0f));
    // Draw window outline
    g.drawRect(iconBounds.reduced(6.0f, 2.0f), 1.0f);
  } else if (iconType == Hide) {
    // Draw hide icon (minus sign)
    g.drawLine(iconBounds.getX(), iconBounds.getCentreY(), iconBounds.getRight(), iconBounds.getCentreY(), 2.0f);
  }
}

DetachableContainer::DetachableContainer(const juce::String &title, juce::Component &contentToWrap)
    : content(&contentToWrap),
      isCurrentlyHidden(false),
      popOutButton("PopOut", IconButton::PopOut),
      hideButton("Hide", IconButton::Hide) {
  addAndMakeVisible(titleLabel);
  titleLabel.setText(title, juce::dontSendNotification);
  titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  titleLabel.setFont(12.0f);

  addAndMakeVisible(popOutButton);
  popOutButton.onClick = [this] { togglePopOut(); };
  popOutButton.setTooltip("Pop Out");

  addAndMakeVisible(hideButton);
  hideButton.onClick = [this] { hide(); };
  hideButton.setTooltip("Hide");

  addAndMakeVisible(placeholderLabel);
  placeholderLabel.setText("Popped Out", juce::dontSendNotification);
  placeholderLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
  placeholderLabel.setJustificationType(juce::Justification::centred);
  placeholderLabel.setVisible(false);

  addAndMakeVisible(*content);
}

DetachableContainer::~DetachableContainer() {
  if (window) {
    window->clearContentComponent();
  }
}

void DetachableContainer::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
  
  // Draw header bar
  auto headerArea = getLocalBounds().removeFromTop(24);
  g.setColour(juce::Colour(0xff1a1a1a));
  g.fillRect(headerArea);
  
  g.setColour(juce::Colours::white.withAlpha(0.2f));
  g.drawLine(0.0f, 24.0f, static_cast<float>(getWidth()), 24.0f, 1.0f);
}

void DetachableContainer::resized() {
  auto area = getLocalBounds();
  auto headerArea = area.removeFromTop(24);

  titleLabel.setBounds(headerArea.removeFromLeft(120).reduced(4, 2));
  hideButton.setBounds(headerArea.removeFromRight(24).reduced(2));
  popOutButton.setBounds(headerArea.removeFromRight(24).reduced(2));

  if (content && content->isVisible()) {
    content->setBounds(area);
  } else {
    placeholderLabel.setBounds(area);
  }
}

void DetachableContainer::setContent(juce::Component &newContent) {
  if (content) {
    removeChildComponent(content);
  }
  
  content = &newContent;
  
  if (!window || !window->isVisible()) {
    addAndMakeVisible(*content);
  }
  
  resized();
}

void DetachableContainer::togglePopOut() {
  if (window && window->isVisible()) {
    // Currently floating - redock
    redock();
  } else {
    // Currently docked - pop out
    if (content) {
      removeChildComponent(content);
      
      // Create floating window
      window = std::make_unique<FloatingWindow>(titleLabel.getText(), this);
      
      // Set content non-owned (so MainComponent retains ownership)
      window->setContentNonOwned(content, true);
      window->centreWithSize(600, 400);
      window->setVisible(true);
      
      // Show placeholder in container
      placeholderLabel.setVisible(true);
    }
    
    resized();
  }
}

void DetachableContainer::redock() {
  if (window && window->isVisible()) {
    // Clear content from window
    window->clearContentComponent();
    window.reset();
    
    // Re-add content to container
    if (content) {
      addAndMakeVisible(*content);
      placeholderLabel.setVisible(false);
    }
    
    resized();
  }
}

void DetachableContainer::hide() {
  if (!isCurrentlyHidden) {
    isCurrentlyHidden = true;
    setVisible(false);
    if (onVisibilityChanged) {
      onVisibilityChanged(this, true);
    }
  }
}

void DetachableContainer::show() {
  if (isCurrentlyHidden) {
    isCurrentlyHidden = false;
    setVisible(true);
    if (onVisibilityChanged) {
      onVisibilityChanged(this, false);
    }
    resized();
  }
}
