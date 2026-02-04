#include "MidiEngine.h"
#include "SettingsManager.h"

MidiEngine::MidiEngine(SettingsManager *settingsMgr)
    : settingsManager(settingsMgr) {
  if (settingsManager) {
    settingsManager->addChangeListener(this);
    cachedDelayMidiEnabled = settingsManager->isDelayMidiEnabled();
  }
}

MidiEngine::~MidiEngine() {
  stopTimer();
  if (settingsManager)
    settingsManager->removeChangeListener(this);
  // std::unique_ptr automatically closes the device on destruction
}

void MidiEngine::changeListenerCallback(juce::ChangeBroadcaster *source) {
  if (source == settingsManager && settingsManager)
    cachedDelayMidiEnabled = settingsManager->isDelayMidiEnabled();
}

void MidiEngine::queueOrSendNow(const juce::MidiMessage &msg) {
  if (!cachedDelayMidiEnabled) {
    sendImmediately(msg);
    return;
  }
  if (!currentOutput)
    return;

  const double nowMs = juce::Time::getMillisecondCounterHiRes();
  const int delaySec =
      juce::jlimit(1, 10, settingsManager->getDelayMidiSeconds());
  const double sendAtMs = nowMs + delaySec * 1000.0;

  {
    const juce::ScopedLock sl(delayQueueLock);
    delayQueue.push_back({msg, sendAtMs});
  }
  if (!isTimerRunning())
    startTimer(kDelayTimerIntervalMs);
}

void MidiEngine::sendImmediately(const juce::MidiMessage &msg) {
  if (currentOutput)
    currentOutput->sendMessageNow(msg);
}

void MidiEngine::timerCallback() {
  const double nowMs = juce::Time::getMillisecondCounterHiRes();
  std::vector<juce::MidiMessage> toSend;

  {
    const juce::ScopedLock sl(delayQueueLock);
    auto it = delayQueue.begin();
    while (it != delayQueue.end()) {
      if (it->sendAtMs <= nowMs) {
        toSend.push_back(it->message);
        it = delayQueue.erase(it);
      } else {
        ++it;
      }
    }
    if (delayQueue.empty())
      stopTimer();
  }

  for (const auto &msg : toSend)
    sendImmediately(msg);
}

juce::StringArray MidiEngine::getDeviceNames() {
  // Update our internal cache of devices
  availableDevices = juce::MidiOutput::getAvailableDevices();

  juce::StringArray names;
  for (const auto &dev : availableDevices) {
    names.add(dev.name);
  }

  // Fallback text if user has no hardware
  if (names.isEmpty())
    names.add("<No MIDI Output Devices>");

  return names;
}

void MidiEngine::setOutputDevice(int deviceIndex) {
  // Close any currently open device
  currentOutput.reset();

  // Check if index is valid relative to our cached list
  if (deviceIndex >= 0 && deviceIndex < availableDevices.size()) {
    auto deviceId = availableDevices[deviceIndex].identifier;
    currentOutput = juce::MidiOutput::openDevice(deviceId);

    if (currentOutput) {
      DBG("MidiEngine: Opened " + availableDevices[deviceIndex].name);
    } else {
      DBG("MidiEngine: Failed to open device.");
    }
  }
}

void MidiEngine::sendNoteOn(int channel, int note, float velocity) {
  auto msg = juce::MidiMessage::noteOn(channel, note, velocity);
  queueOrSendNow(msg);
}

void MidiEngine::sendNoteOff(int channel, int note) {
  auto msg = juce::MidiMessage::noteOff(channel, note);
  queueOrSendNow(msg);
}

void MidiEngine::sendCC(int channel, int controller, int value) {
  auto msg = juce::MidiMessage::controllerEvent(channel, controller, value);
  queueOrSendNow(msg);
}

void MidiEngine::sendPitchBend(int channel, int value) {
  value = juce::jlimit(0, 16383, value);
  auto msg = juce::MidiMessage::pitchWheel(channel, value);
  queueOrSendNow(msg);
}

void MidiEngine::allNotesOff() {
  for (int ch = 1; ch <= 16; ++ch) {
    auto msg = juce::MidiMessage::controllerEvent(ch, 123, 0);
    queueOrSendNow(msg);
  }
}

void MidiEngine::sendPitchBendRangeRPN(int channel, int rangeSemitones) {
  if (!currentOutput)
    return;

  // RPN Setup: Pitch Bend Sensitivity is 00 00
  // Order: LSB (100) then MSB (101) is often safer for legacy/strict parsers
  queueOrSendNow(
      juce::MidiMessage::controllerEvent(channel, 100, 0));
  queueOrSendNow(
      juce::MidiMessage::controllerEvent(channel, 101, 0));

  // Data Entry: Set the Range
  queueOrSendNow(
      juce::MidiMessage::controllerEvent(channel, 6, rangeSemitones));
  queueOrSendNow(
      juce::MidiMessage::controllerEvent(channel, 38, 0)); // Cents = 0

  // NOTE: We are intentionally SKIPPING the "Null RPN" reset (101=127, 100=127).
  // Sending it immediately can sometimes interrupt the Data Entry processing in some VSTs
  // if the buffer is processed fast. Leaving RPN 00 selected is harmless.
}