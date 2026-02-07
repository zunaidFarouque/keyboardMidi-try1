#include "../SettingsManager.h"
#include <gtest/gtest.h>

// Test helper that mirrors MainComponent's MIDI/Performance mode coupling.
// Spec: turn off MIDI -> both off; turn off Performance -> performance off, MIDI stays on.
class MidiPerformanceModeCoordinator : public juce::ChangeListener {
public:
  explicit MidiPerformanceModeCoordinator(SettingsManager &sm)
      : settingsManager(sm), performanceOn(false) {
    settingsManager.addChangeListener(this);
  }
  ~MidiPerformanceModeCoordinator() override {
    settingsManager.removeChangeListener(this);
  }

  void changeListenerCallback(juce::ChangeBroadcaster *source) override {
    if (source == &settingsManager && !settingsManager.isMidiModeActive()) {
      performanceOn = false; // MIDI off -> performance off
    }
  }

  void turnOnMidi() { settingsManager.setMidiModeActive(true); }
  void turnOffMidi() { settingsManager.setMidiModeActive(false); }
  void turnOnPerformance() {
    settingsManager.setMidiModeActive(true);
    performanceOn = true;
  }
  void turnOffPerformance() { performanceOn = false; }

  bool isMidiOn() const { return settingsManager.isMidiModeActive(); }
  bool isPerformanceOn() const { return performanceOn; }

private:
  SettingsManager &settingsManager;
  bool performanceOn;
};

class MidiPerformanceModeTest : public ::testing::Test {
protected:
  juce::ScopedJuceInitialiser_GUI juceInit; // Required for ChangeBroadcaster
  SettingsManager settingsMgr;
  MidiPerformanceModeCoordinator coordinator{settingsMgr};

  void flushChangeMessages() { settingsMgr.dispatchPendingMessages(); }

  void expectStandard() {
    EXPECT_FALSE(coordinator.isMidiOn()) << "Expected standard: MIDI off";
    EXPECT_FALSE(coordinator.isPerformanceOn())
        << "Expected standard: Performance off";
  }
  void expectMidiOnly() {
    EXPECT_TRUE(coordinator.isMidiOn()) << "Expected MIDI only: MIDI on";
    EXPECT_FALSE(coordinator.isPerformanceOn())
        << "Expected MIDI only: Performance off";
  }
  void expectBothOn() {
    EXPECT_TRUE(coordinator.isMidiOn()) << "Expected both on: MIDI on";
    EXPECT_TRUE(coordinator.isPerformanceOn())
        << "Expected both on: Performance on";
  }
};

// Standard -> Turn on MIDI mode -> MIDI only
TEST_F(MidiPerformanceModeTest, Standard_TurnOnMidi_ResultsInMidiOnly) {
  expectStandard();
  coordinator.turnOnMidi();
  expectMidiOnly();
}

// Standard -> Turn on Performance mode -> Both on
TEST_F(MidiPerformanceModeTest, Standard_TurnOnPerformance_ResultsInBothOn) {
  expectStandard();
  coordinator.turnOnPerformance();
  expectBothOn();
}

// MIDI only -> Turn off MIDI mode -> Standard
TEST_F(MidiPerformanceModeTest, MidiOnly_TurnOffMidi_ResultsInStandard) {
  coordinator.turnOnMidi();
  expectMidiOnly();
  coordinator.turnOffMidi();
  flushChangeMessages(); // Allow async ChangeListener callback to run
  expectStandard();
}

// MIDI only -> Turn on Performance mode -> Both on
TEST_F(MidiPerformanceModeTest, MidiOnly_TurnOnPerformance_ResultsInBothOn) {
  coordinator.turnOnMidi();
  expectMidiOnly();
  coordinator.turnOnPerformance();
  expectBothOn();
}

// Both on -> Turn off Performance mode -> MIDI only
TEST_F(MidiPerformanceModeTest, BothOn_TurnOffPerformance_ResultsInMidiOnly) {
  coordinator.turnOnPerformance();
  expectBothOn();
  coordinator.turnOffPerformance();
  expectMidiOnly();
}

// Both on -> Turn off MIDI mode -> Standard (both off)
TEST_F(MidiPerformanceModeTest, BothOn_TurnOffMidi_ResultsInStandard) {
  coordinator.turnOnPerformance();
  expectBothOn();
  coordinator.turnOffMidi();
  flushChangeMessages(); // Allow async ChangeListener callback to run
  expectStandard();
}
