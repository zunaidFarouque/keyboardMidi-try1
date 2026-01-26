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
  // Create a SafePointer to 'this'
  // If LogComponent is deleted, safeThis becomes null automatically.
  juce::Component::SafePointer<LogComponent> safeThis(this);

  juce::MessageManager::callAsync([safeThis, text]() {
    // Check if we are still alive
    if (safeThis == nullptr) return;

    // Use safeThis-> instead of implicit this->
    safeThis->logBuffer.push_back(text);

    while (safeThis->logBuffer.size() > safeThis->maxLines) {
      safeThis->logBuffer.pop_front();
    }

    juce::String combinedText;
    for (const auto &line : safeThis->logBuffer) {
      combinedText += line + "\n";
    }

    safeThis->console.setText(combinedText, false);
    safeThis->console.moveCaretToEnd();
  });
}

void LogComponent::clear() {
  logBuffer.clear();
  console.clear();
}