#pragma once
#include "KeyNameUtilities.h"
#include <JuceHeader.h>
#include <vector>
#include <memory>
#include <functional>

class KeyChipList : public juce::Component {
public:
  KeyChipList();
  ~KeyChipList() override;

  void paint(juce::Graphics&) override;
  void resized() override;

  // Set the list of key codes to display
  void setKeys(const std::vector<int>& keyCodes);

  // Callback when a key is removed
  std::function<void(int keyCode)> onKeyRemoved;

private:
  class Chip : public juce::Component {
  public:
    Chip(int keyCode, std::function<void(int)> onRemove);
    ~Chip() override = default;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& e) override;

  private:
    int keyCode;
    std::function<void(int)> onRemoveCallback;
    juce::Rectangle<int> removeButtonArea;
  };

  std::vector<std::unique_ptr<Chip>> chips;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KeyChipList)
};
