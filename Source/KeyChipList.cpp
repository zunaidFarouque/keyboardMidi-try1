#include "KeyChipList.h"

// =============================================================================
// KeyChipList::Chip
// =============================================================================

KeyChipList::Chip::Chip(int keyCode, std::function<void(int)> onRemove)
    : keyCode(keyCode), onRemoveCallback(onRemove) {
  setSize(60, 24);
}

void KeyChipList::Chip::paint(juce::Graphics& g) {
  auto bounds = getLocalBounds().toFloat().reduced(1.0f);
  
  // Draw rounded rectangle background
  g.setColour(juce::Colour(0xff3a3a3a));
  g.fillRoundedRectangle(bounds, 4.0f);
  
  g.setColour(juce::Colour(0xff5a5a5a));
  g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

  // Draw key name
  g.setColour(juce::Colours::white);
  g.setFont(12.0f);
  juce::String keyName = KeyNameUtilities::getKeyName(keyCode);
  
  // Layout: Key name on left, X button on right
  auto textArea = bounds.withTrimmedRight(20);
  removeButtonArea = getLocalBounds().removeFromRight(18).toNearestInt();
  
  g.drawText(keyName, textArea, juce::Justification::centredLeft, true);

  // Draw X button
  if (removeButtonArea.contains(getMouseXYRelative())) {
    g.setColour(juce::Colours::red.brighter(0.3f));
  } else {
    g.setColour(juce::Colours::lightgrey);
  }
  
  auto xArea = removeButtonArea.toFloat().reduced(4.0f);
  g.drawLine(xArea.getTopLeft().x, xArea.getTopLeft().y, 
             xArea.getBottomRight().x, xArea.getBottomRight().y, 1.5f);
  g.drawLine(xArea.getTopRight().x, xArea.getTopRight().y, 
             xArea.getBottomLeft().x, xArea.getBottomLeft().y, 1.5f);
}

void KeyChipList::Chip::mouseDown(const juce::MouseEvent& e) {
  if (removeButtonArea.contains(e.getPosition())) {
    if (onRemoveCallback) {
      onRemoveCallback(keyCode);
    }
  }
}

// =============================================================================
// KeyChipList
// =============================================================================

KeyChipList::KeyChipList() {
}

KeyChipList::~KeyChipList() {
}

void KeyChipList::paint(juce::Graphics& g) {
  // Background is handled by parent
}

void KeyChipList::resized() {
  // Use FlexBox for automatic wrapping
  juce::FlexBox flexBox;
  flexBox.flexWrap = juce::FlexBox::Wrap::wrap;
  flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;
  flexBox.alignContent = juce::FlexBox::AlignContent::flexStart;
  
  juce::Array<juce::FlexItem> items;
  for (auto& chip : chips) {
    items.add(juce::FlexItem(*chip).withMinWidth(60.0f).withMinHeight(24.0f)
                                      .withMargin(juce::FlexItem::Margin(2.0f)));
  }
  
  flexBox.items = items;
  flexBox.performLayout(getLocalBounds().toFloat());
}

void KeyChipList::setKeys(const std::vector<int>& keyCodes) {
  // Remove all existing chips from component tree
  for (auto& chip : chips) {
    removeChildComponent(chip.get());
  }
  
  chips.clear();
  
  // Create new chips
  for (int keyCode : keyCodes) {
    auto chip = std::make_unique<Chip>(keyCode, [this](int kc) {
      if (onKeyRemoved) {
        onKeyRemoved(kc);
      }
    });
    addAndMakeVisible(chip.get());
    chips.push_back(std::move(chip));
  }
  
  resized();
}
