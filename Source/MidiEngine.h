#pragma once
#include <JuceHeader.h>
#include <vector>

class SettingsManager;

class MidiEngine : public juce::Timer, public juce::ChangeListener {
public:
  explicit MidiEngine(SettingsManager *settingsMgr = nullptr);
  ~MidiEngine() override;

  // Scans for devices and returns a list of names for the UI (ComboBox)
  juce::StringArray getDeviceNames();

  // Opens the device selected by the user (by index from the list above)
  void setOutputDevice(int deviceIndex);

  // MIDI Action methods (virtual for test overrides)
  virtual void sendNoteOn(int channel, int note, float velocity);
  virtual void sendNoteOff(int channel, int note);
  virtual void sendCC(int channel, int controller, int value);
  virtual void sendPitchBend(int channel, int value); // Value: 0-16383 (center = 8192)
  virtual void sendProgramChange(int channel, int program); // Program: 0-127

  // All Notes Off (CC 123) on all 16 channels
  void allNotesOff();

  // Send Pitch Bend Range RPN (Registered Parameter Number) to configure synth
  void sendPitchBendRangeRPN(int channel, int rangeSemitones);

  void timerCallback() override;
  void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
  void queueOrSendNow(const juce::MidiMessage &msg);
  void sendImmediately(const juce::MidiMessage &msg);

  SettingsManager *settingsManager;
  bool cachedDelayMidiEnabled = false;
  std::unique_ptr<juce::MidiOutput> currentOutput;

  // We cache the device info list so indexes match between UI and Engine
  juce::Array<juce::MidiDeviceInfo> availableDevices;

  // Delay MIDI: queue of (message, sendAtMs)
  struct PendingMessage {
    juce::MidiMessage message;
    double sendAtMs;
  };
  std::vector<PendingMessage> delayQueue;
  juce::CriticalSection delayQueueLock;
  static constexpr int kDelayTimerIntervalMs = 50;
};