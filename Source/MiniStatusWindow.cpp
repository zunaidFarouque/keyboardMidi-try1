#include "MiniStatusWindow.h"
#include "RawInputManager.h"

MiniStatusWindow::MiniStatusWindow(SettingsManager& settingsMgr)
    : juce::DocumentWindow("OmniKey Status", juce::Colour(0xff2a2a2a), DocumentWindow::closeButton),
      settingsManager(settingsMgr) {
  setAlwaysOnTop(true);
  setResizable(true, false);
  setSize(300, 50);
  setContentOwned(&statusLabel, true);
  
  statusLabel.setText("MIDI Mode is ON. Press " + RawInputManager::getKeyName(settingsManager.getToggleKey()) + " or Close to stop.", juce::dontSendNotification);
  statusLabel.setJustificationType(juce::Justification::centred);
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);
  
  addToDesktop();
  centreWithSize(300, 50);
  setVisible(false); // Start hidden
}

MiniStatusWindow::~MiniStatusWindow() {
  setContentOwned(nullptr, false);
}

void MiniStatusWindow::closeButtonPressed() {
  // Turn off MIDI mode
  settingsManager.setMidiModeActive(false);
  // Hide the window
  setVisible(false);
}
