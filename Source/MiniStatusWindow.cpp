#include "MiniStatusWindow.h"
#include "InputProcessor.h"
#include "RawInputManager.h"
#include "TouchpadVisualizerPanel.h"

static constexpr int kMiniWindowStatusOnlyW = 300;
static constexpr int kMiniWindowStatusOnlyH = 50;
static constexpr int kMiniWindowWithTouchpadW = 240;
static constexpr int kMiniWindowWithTouchpadH = 240;

MiniStatusWindow::MiniStatusWindow(SettingsManager &settingsMgr,
                                   InputProcessor *inputProc)
    : juce::DocumentWindow("MIDIQy Status", juce::Colour(0xff2a2a2a),
                           DocumentWindow::closeButton),
      settingsManager(settingsMgr), inputProcessor(inputProc) {
  statusLabel.setText(
      "MIDI Mode is ON. Press " +
          RawInputManager::getKeyName(settingsManager.getToggleKey()) +
          " or Close to stop. Press " +
          RawInputManager::getKeyName(settingsManager.getPerformanceModeKey()) +
          " to unlock cursor.",
      juce::dontSendNotification);
  statusLabel.setJustificationType(juce::Justification::centred);
  statusLabel.setColour(juce::Label::textColourId, juce::Colours::white);

  setContentNonOwned(&statusLabel, true);
  setAlwaysOnTop(true);
  setResizable(true, false);
  setSize(kMiniWindowStatusOnlyW, kMiniWindowStatusOnlyH);
  addToDesktop();

  juce::String savedPos = settingsManager.getMiniWindowPosition();
  if (savedPos.isNotEmpty()) {
    restoreWindowStateFromString(savedPos);
  } else {
    centreWithSize(kMiniWindowStatusOnlyW, kMiniWindowStatusOnlyH);
  }
  setVisible(false);

  settingsManager.addChangeListener(this);
  refreshContent();
}

MiniStatusWindow::~MiniStatusWindow() {
  settingsManager.removeChangeListener(this);
  if (isOnDesktop())
    settingsManager.setMiniWindowPosition(getWindowStateAsString());
  clearContentComponent();
}

void MiniStatusWindow::closeButtonPressed() {
  if (isOnDesktop())
    settingsManager.setMiniWindowPosition(getWindowStateAsString());
  settingsManager.setMidiModeActive(false);
  setVisible(false);
}

void MiniStatusWindow::moved() {
  if (isOnDesktop() && isVisible())
    settingsManager.setMiniWindowPosition(getWindowStateAsString());
}

void MiniStatusWindow::resetToDefaultPosition() {
  int w = getWidth();
  int h = getHeight();
  centreWithSize(w, h);
  settingsManager.setMiniWindowPosition("");
}

void MiniStatusWindow::updateTouchpadContacts(
    const std::vector<TouchpadContact> &contacts, uintptr_t deviceHandle) {
  if (touchpadPanelHolder) {
    if (auto *panel = dynamic_cast<TouchpadVisualizerPanel *>(
            touchpadPanelHolder.get())) {
      panel->setContacts(contacts, deviceHandle);
    }
  }
}

void MiniStatusWindow::setVisualizedLayer(int layerId) {
  if (touchpadPanelHolder) {
    if (auto *panel = dynamic_cast<TouchpadVisualizerPanel *>(
            touchpadPanelHolder.get())) {
      panel->setVisualizedLayer(layerId);
    }
  }
}

void MiniStatusWindow::setSelectedTouchpadStrip(int stripIndex, int layerId) {
  if (touchpadPanelHolder) {
    if (auto *panel = dynamic_cast<TouchpadVisualizerPanel *>(
            touchpadPanelHolder.get())) {
      panel->setSelectedStrip(stripIndex, layerId);
    }
  }
}

void MiniStatusWindow::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == &settingsManager) {
    refreshContent();
  }
}

void MiniStatusWindow::refreshContent() {
  const bool showTouchpad =
      settingsManager.getShowTouchpadVisualizerInMiniWindow();

  if (showTouchpad && inputProcessor) {
    if (!touchpadPanelHolder) {
      auto *panel =
          new TouchpadVisualizerPanel(inputProcessor, &settingsManager);
      panel->setShowContactCoordinates(false); // Mini window: circles suffice
      touchpadPanelHolder.reset(panel);
    }
    clearContentComponent();
    setContentNonOwned(touchpadPanelHolder.get(), true);
    setSize(kMiniWindowWithTouchpadW, kMiniWindowWithTouchpadH);
  } else {
    if (touchpadPanelHolder)
      touchpadPanelHolder.reset();
    clearContentComponent();
    setContentNonOwned(&statusLabel, true);
    setSize(kMiniWindowStatusOnlyW, kMiniWindowStatusOnlyH);
  }
}
