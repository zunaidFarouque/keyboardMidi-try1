#include "MiniStatusWindow.h"
#include "RawInputManager.h"

MiniStatusWindow::MiniStatusWindow(SettingsManager& settingsMgr)
    : juce::DocumentWindow("OmniKey Status", juce::Colour(0xff2a2a2a), DocumentWindow::closeButton),
      settingsManager(settingsMgr) {
  // Configure Label
  statusLabel.setText("MIDI Mode is ON. Press " + RawInputManager::getKeyName(settingsManager.getToggleKey()) + " or Close to stop.", juce::dontSendNotification);
  statusLabel.setJustificationType(juce::Justification::centred);
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  // CRITICAL FIX: Use setContentNonOwned.
  // We own the label (it's a member), so the Window must NOT delete it.
  setContentNonOwned(&statusLabel, true);

  setAlwaysOnTop(true);
  setResizable(true, false);
  setSize(300, 50);
  addToDesktop();
  centreWithSize(300, 50);
  setVisible(false); // Start hidden
}

MiniStatusWindow::~MiniStatusWindow() {
  // Detach content without deleting (we own the member). With setContentNonOwned,
  // clearContentComponent() simply removes it; it does not delete.
  clearContentComponent();
}

void MiniStatusWindow::closeButtonPressed() {
  // Turn off MIDI mode
  settingsManager.setMidiModeActive(false);
  // Hide the window
  setVisible(false);
}
