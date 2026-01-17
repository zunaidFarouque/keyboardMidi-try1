#pragma once
#include <JuceHeader.h>
#include <deque>

class LogComponent : public juce::Component {
public:
  LogComponent();
  ~LogComponent() override;

  void paint(juce::Graphics &) override;
  void resized() override;

  void addEntry(const juce::String &text);
  void clear();

private:
  juce::TextEditor console;

  // The efficient buffer
  std::deque<juce::String> logBuffer;
  const size_t maxLines = 10; // Keep it snappy

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LogComponent)
};