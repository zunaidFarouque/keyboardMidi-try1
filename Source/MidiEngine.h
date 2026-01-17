#pragma once
#include <JuceHeader.h>

class MidiEngine {
public:
  MidiEngine();
  ~MidiEngine();

  // Scans for devices and returns a list of names for the UI (ComboBox)
  juce::StringArray getDeviceNames();

  // Opens the device selected by the user (by index from the list above)
  void setOutputDevice(int deviceIndex);

  // MIDI Action methods
  void sendNoteOn(int channel, int note, float velocity);
  void sendNoteOff(int channel, int note);
  void sendCC(int channel, int controller, int value);

private:
  std::unique_ptr<juce::MidiOutput> currentOutput;

  // We cache the device info list so indexes match between UI and Engine
  juce::Array<juce::MidiDeviceInfo> availableDevices;
};