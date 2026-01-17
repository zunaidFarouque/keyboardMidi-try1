#pragma once
#include <JuceHeader.h>
#include <vector>

class MusicalKeyboardComponent : public juce::Component {
public:
  MusicalKeyboardComponent();
  ~MusicalKeyboardComponent() override;

  void paint(juce::Graphics&) override;
  void resized() override;
  void mouseDown(const juce::MouseEvent& e) override;

  // Set the root note (0-11, where 0 = C, 1 = C#, etc.)
  void setRootNote(int rootNote);

  // Set which intervals are active (0-11, where 0 = Root, 1 = m2, etc.)
  void setActiveIntervals(const std::vector<int>& intervals);

  // Get the intervals that are currently active
  std::vector<int> getActiveIntervals() const;

  // Callback when a key is toggled
  std::function<void(int interval)> onIntervalToggled;

private:
  int rootNote = 0; // 0 = C
  std::vector<bool> activeIntervals; // 12 booleans, one for each semitone (0-11)

  // Helper methods
  int getKeyAtPosition(int x, int y) const;
  bool isBlackKey(int note) const;
  juce::Rectangle<int> getKeyBounds(int note) const;
  int getNoteFromInterval(int interval) const; // Returns MIDI note (0-127) for interval 0-11 relative to root

  static constexpr int numKeys = 12; // One octave
  static constexpr int whiteKeyHeight = 80;
  static constexpr int blackKeyHeight = 50;
  static constexpr int whiteKeyWidth = 30; // Increased for better visibility
  static constexpr int blackKeyWidth = 18;  // Increased for better visibility
};
