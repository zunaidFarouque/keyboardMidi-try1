#include "MidiEngine.h"

MidiEngine::MidiEngine() {}

MidiEngine::~MidiEngine() {
  // std::unique_ptr automatically closes the device on destruction
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
  if (currentOutput) {
    auto msg = juce::MidiMessage::noteOn(channel, note, velocity);
    currentOutput->sendMessageNow(msg);
  }
}

void MidiEngine::sendNoteOff(int channel, int note) {
  if (currentOutput) {
    auto msg = juce::MidiMessage::noteOff(channel, note);
    currentOutput->sendMessageNow(msg);
  }
}

void MidiEngine::sendCC(int channel, int controller, int value) {
  if (currentOutput) {
    auto msg = juce::MidiMessage::controllerEvent(channel, controller, value);
    currentOutput->sendMessageNow(msg);
  }
}