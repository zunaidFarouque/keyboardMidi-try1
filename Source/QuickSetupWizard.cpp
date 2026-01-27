#include "QuickSetupWizard.h"

QuickSetupWizard::QuickSetupWizard(DeviceManager& deviceMgr, RawInputManager& rawInputMgr)
    : deviceManager(deviceMgr), rawInputManager(rawInputMgr) {
  
  // Setup title
  titleLabel.setText("Action Required", juce::dontSendNotification);
  titleLabel.setJustificationType(juce::Justification::centred);
  titleLabel.setFont(juce::Font(24.0f, juce::Font::bold));
  addAndMakeVisible(titleLabel);
  
  // Setup instruction
  instructionLabel.setText("Press any key on the device to assign it.", juce::dontSendNotification);
  instructionLabel.setJustificationType(juce::Justification::centred);
  instructionLabel.setFont(juce::Font(18.0f));
  addAndMakeVisible(instructionLabel);
  
  // Setup buttons
  skipButton.setButtonText("Skip");
  skipButton.onClick = [this] { onSkip(); };
  addAndMakeVisible(skipButton);
  
  cancelButton.setButtonText("Cancel Setup");
  cancelButton.onClick = [this] { onCancel(); };
  addAndMakeVisible(cancelButton);
  
  setVisible(false);
}

QuickSetupWizard::~QuickSetupWizard() {
  // Unregister listener (always safe to call)
  rawInputManager.removeListener(this);
}

void QuickSetupWizard::startSequence(const juce::StringArray& aliasesToMapIn) {
  aliasesToMap = aliasesToMapIn;
  currentIndex = 0;
  
  if (aliasesToMap.size() == 0) {
    finishWizard();
    return;
  }
  
  // Register as listener
  rawInputManager.addListener(this);
  
  updateInstruction();
  setVisible(true);
  repaint();
}

void QuickSetupWizard::updateInstruction() {
  if (currentIndex >= aliasesToMap.size()) {
    finishWizard();
    return;
  }
  
  juce::String currentAlias = aliasesToMap[currentIndex];
  instructionLabel.setText("Press any key on: **" + currentAlias + "**", juce::dontSendNotification);
  repaint();
}

void QuickSetupWizard::handleRawKeyEvent(uintptr_t deviceHandle, int keyCode, bool isDown) {
  // Only process key down events
  if (!isDown)
    return;
  
  // Ignore Global/Virtual events (hash 0)
  if (deviceHandle == 0)
    return;
  
  // Ignore if no aliases to map
  if (currentIndex >= aliasesToMap.size())
    return;
  
  // Assign hardware to current alias
  juce::String currentAlias = aliasesToMap[currentIndex];
  deviceManager.assignHardware(currentAlias, deviceHandle);
  
  // Move to next alias
  currentIndex++;
  updateInstruction();
}

void QuickSetupWizard::handleAxisEvent(uintptr_t deviceHandle, int inputCode, float value) {
  // Ignore axis events for setup wizard
  (void)deviceHandle;
  (void)inputCode;
  (void)value;
}

void QuickSetupWizard::onSkip() {
  // Skip current alias and move to next
  currentIndex++;
  updateInstruction();
}

void QuickSetupWizard::onCancel() {
  // Cancel the entire wizard
  finishWizard();
}

void QuickSetupWizard::finishWizard() {
  // Unregister listener (always safe to call)
  rawInputManager.removeListener(this);
  
  setVisible(false);
  
  // Call finish callback
  if (onFinish) {
    onFinish();
  }
}

void QuickSetupWizard::paint(juce::Graphics& g) {
  // Draw semi-transparent overlay
  g.fillAll(juce::Colour(0x88000000)); // Dark overlay
  
  // Draw white background box
  juce::Rectangle<int> box = getLocalBounds().reduced(100);
  g.setColour(juce::Colours::white);
  g.fillRoundedRectangle(box.toFloat(), 10.0f);
  
  // Draw border
  g.setColour(juce::Colours::black);
  g.drawRoundedRectangle(box.toFloat(), 10.0f, 2.0f);
}

void QuickSetupWizard::resized() {
  juce::Rectangle<int> bounds = getLocalBounds();
  juce::Rectangle<int> contentArea = bounds.reduced(100);
  
  int titleHeight = 60;
  int instructionHeight = 80;
  int buttonHeight = 40;
  int buttonWidth = 120;
  int spacing = 20;
  
  // Title
  titleLabel.setBounds(contentArea.getX(), contentArea.getY() + spacing, 
                       contentArea.getWidth(), titleHeight);
  
  // Instruction
  instructionLabel.setBounds(contentArea.getX(), 
                              titleLabel.getBottom() + spacing,
                              contentArea.getWidth(), instructionHeight);
  
  // Buttons (centered horizontally)
  int buttonY = instructionLabel.getBottom() + spacing * 2;
  int buttonAreaWidth = buttonWidth * 2 + spacing;
  int buttonAreaX = contentArea.getCentreX() - buttonAreaWidth / 2;
  
  skipButton.setBounds(buttonAreaX, buttonY, buttonWidth, buttonHeight);
  cancelButton.setBounds(skipButton.getRight() + spacing, buttonY, buttonWidth, buttonHeight);
}
