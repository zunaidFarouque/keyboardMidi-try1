#include "LogComponent.h"

LogComponent *LogComponent::s_instance = nullptr;

LogComponent::LogComponent() {
  s_instance = this; // Set static instance
  console.setMultiLine(true);
  console.setReadOnly(true);
  console.setFont(juce::Font("Consolas", 14.0f, juce::Font::plain));
  console.setColour(juce::TextEditor::backgroundColourId,
                    juce::Colour(0xff111111));
  console.setColour(juce::TextEditor::textColourId, juce::Colours::lightgreen);
  console.setScrollbarsShown(true);

  // Disable text editor's internal caret to speed up repaints
  console.setCaretVisible(false);

  addAndMakeVisible(console);
}

LogComponent::~LogComponent() {}

void LogComponent::paint(juce::Graphics &g) {
  g.fillAll(juce::Colour(0xff222222));
  g.setColour(juce::Colours::grey);
  g.drawRect(getLocalBounds(), 1);
}

void LogComponent::resized() { console.setBounds(getLocalBounds().reduced(1)); }

void LogComponent::addEntry(const juce::String &text) {
  // Always update UI on the Message Thread
  juce::MessageManager::callAsync([this, text]() {
    // 1. Add to buffer
    logBuffer.push_back(text);

    // 2. Trim old lines if we exceeded the limit
    while (logBuffer.size() > maxLines) {
      logBuffer.pop_front();
    }

    // 3. Reconstruct the text block
    // (This is much faster than manipulating the TextEditor directly for small
    // N)
    juce::String combinedText;
    for (const auto &line : logBuffer) {
      combinedText += line + "\n";
    }

    // 4. Update UI
    console.setText(combinedText, false); // false = don't send change message
    console.moveCaretToEnd();
  });
}

void LogComponent::clear() {
  logBuffer.clear();
  console.clear();
}