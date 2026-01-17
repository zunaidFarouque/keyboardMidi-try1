#include "ScaleEditorComponent.h"
#include "ScaleLibrary.h"

// Interval names: Root, m2, M2, m3, M3, P4, TT, P5, m6, M6, m7, M7
static const char* intervalNames[] = {
  "Root", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"
};

// Constructor
ScaleEditorComponent::ScaleEditorComponent(ScaleLibrary* scaleLib)
  : scaleLibrary(scaleLib),
    listModel(scaleLib, this) {
  
  // Initialize ListBox AFTER everything else is set up
  scaleListBox.setModel(&listModel);
  scaleListBox.setName("ScaleList");
  
  // Setup labels
  nameLabel.setText("Scale Name:", juce::dontSendNotification);
  nameLabel.attachToComponent(&nameEditor, true);
  addAndMakeVisible(nameLabel);
  addAndMakeVisible(nameEditor);

  // Setup musical keyboard
  addAndMakeVisible(keyboardComponent);
  keyboardComponent.onIntervalToggled = [this](int interval) {
    // Toggle the corresponding button
    if (interval >= 0 && interval < 12) {
      bool newState = !intervalButtons[interval].getToggleState();
      intervalButtons[interval].setToggleState(newState, juce::sendNotification);
    }
  };

  // Setup interval buttons
  for (int i = 0; i < 12; ++i) {
    intervalLabels[i].setText(intervalNames[i], juce::dontSendNotification);
    intervalLabels[i].attachToComponent(&intervalButtons[i], true);
    addAndMakeVisible(intervalLabels[i]);
    addAndMakeVisible(intervalButtons[i]);
    
    // Root (index 0) is always ON and disabled
    if (i == 0) {
      intervalButtons[i].setToggleState(true, juce::dontSendNotification);
      intervalButtons[i].setEnabled(false);
    }
    
    // Wire up button callbacks to update keyboard
    intervalButtons[i].onClick = [this, i] {
      if (i > 0) { // Root can't be toggled
        updateKeyboardFromButtons();
      }
    };
  }

  // Setup buttons
  saveButton.setButtonText("Save Scale");
  saveButton.onClick = [this] {
    if (scaleLibrary && nameEditor.getText().isNotEmpty()) {
      auto intervals = buildIntervalsFromButtons();
      if (!intervals.empty()) {
        scaleLibrary->createScale(nameEditor.getText(), intervals);
        updateListBox();
      }
    }
  };
  addAndMakeVisible(saveButton);

  deleteButton.setButtonText("Delete");
  deleteButton.onClick = [this] {
    if (scaleLibrary && currentScaleName.isNotEmpty()) {
      scaleLibrary->deleteScale(currentScaleName);
      clearEditor();
      updateListBox();
    }
  };
  addAndMakeVisible(deleteButton);

  newButton.setButtonText("New Scale");
  newButton.onClick = [this] {
    clearEditor();
  };
  addAndMakeVisible(newButton);

  // Set initial size to ensure proper layout BEFORE adding components
  // Increased height to accommodate keyboard
  setSize(600, 500);
  
  // Setup list box - do this AFTER setting size
  addAndMakeVisible(scaleListBox);
  
  // Listen to ScaleLibrary changes
  if (scaleLibrary) {
    scaleLibrary->addChangeListener(this);
  }
  
  // Initial keyboard update
  updateKeyboardFromButtons();
  
  // Initial update - defer slightly to ensure component is fully constructed
  juce::Timer::callAfterDelay(100, [this] {
    if (scaleLibrary != nullptr) {
      updateListBox();
    }
  });
}

ScaleEditorComponent::~ScaleEditorComponent() {
  if (scaleLibrary) {
    scaleLibrary->removeChangeListener(this);
  }
}

void ScaleEditorComponent::paint(juce::Graphics& g) {
  g.fillAll(juce::Colour(0xff2a2a2a));
}

void ScaleEditorComponent::resized() {
  auto area = getLocalBounds().reduced(10);
  
  if (area.isEmpty())
    return; // Safety check - don't layout if we have no space
  
  // Split: left = list, right = editor
  auto listArea = area.removeFromLeft(200);
  scaleListBox.setBounds(listArea);
  
  area.removeFromLeft(10); // Gap
  
  // Right side: name editor at top
  auto nameArea = area.removeFromTop(30);
  if (!nameArea.isEmpty()) {
    int nameEditorWidth = juce::jmax(150, static_cast<int>(nameArea.getWidth() * 2.0f / 3.0f));
    nameEditor.setBounds(nameArea.removeFromRight(nameEditorWidth).reduced(2));
    nameLabel.setBounds(nameArea);
  }
  area.removeFromTop(10);
  
  // Musical keyboard visualizer
  auto keyboardArea = area.removeFromTop(90);
  // Center the keyboard in the available space
  int keyboardWidth = keyboardComponent.getWidth();
  int keyboardX = keyboardArea.getX() + (keyboardArea.getWidth() - keyboardWidth) / 2;
  keyboardComponent.setBounds(keyboardX, keyboardArea.getY(), keyboardWidth, keyboardArea.getHeight());
  area.removeFromTop(10);
  
  // Interval buttons in a grid (3 columns)
  auto buttonArea = area.removeFromTop(180);
  if (!buttonArea.isEmpty()) {
    int buttonWidth = juce::jmax(80, buttonArea.getWidth() / 3);
    int buttonHeight = 25;
    int spacing = 5;
    
    for (int i = 0; i < 12; ++i) {
      int row = i / 3;
      int col = i % 3;
      int x = buttonArea.getX() + col * (buttonWidth + spacing);
      int y = buttonArea.getY() + row * (buttonHeight + spacing);
      
      int buttonW = juce::jmax(50, buttonWidth - 60);
      intervalButtons[i].setBounds(x, y, buttonW, buttonHeight);
      intervalLabels[i].setBounds(x + buttonW, y, 60, buttonHeight);
    }
  }
  
  area.removeFromTop(10);
  
  // Buttons at bottom
  if (!area.isEmpty()) {
    auto buttonRow = area.removeFromBottom(30);
    int buttonSpace = juce::jmax(80, buttonRow.getWidth() / 3);
    newButton.setBounds(buttonRow.removeFromLeft(buttonSpace).reduced(2));
    saveButton.setBounds(buttonRow.removeFromLeft(buttonSpace).reduced(2));
    deleteButton.setBounds(buttonRow.reduced(2));
  }
}

// ScaleListModel methods
int ScaleEditorComponent::ScaleListModel::getNumRows() {
  if (!scaleLibrary)
    return 0;
  
  // Safety check - ensure we can safely access the library
  try {
    return scaleLibrary->getScaleNames().size();
  } catch (...) {
    return 0;
  }
}

void ScaleEditorComponent::ScaleListModel::paintListBoxItem(int rowNumber, juce::Graphics& g,
                                                            int width, int height, bool rowIsSelected) {
  if (rowIsSelected)
    g.fillAll(juce::Colours::lightblue.withAlpha(0.3f));
  else if (rowNumber % 2)
    g.fillAll(juce::Colours::white.withAlpha(0.05f));

  if (!scaleLibrary)
    return;

  try {
    auto scaleNames = scaleLibrary->getScaleNames();
    if (rowNumber >= 0 && rowNumber < scaleNames.size()) {
      g.setColour(juce::Colours::white);
      g.setFont(14.0f);
      g.drawText(scaleNames[rowNumber], 4, 0, width - 4, height, juce::Justification::centredLeft);
    }
  } catch (...) {
    // Silently handle any errors during painting
  }
}

void ScaleEditorComponent::ScaleListModel::listBoxItemClicked(int row, const juce::MouseEvent&) {
  if (!scaleLibrary || !owner)
    return;

  try {
    auto scaleNames = scaleLibrary->getScaleNames();
    if (row >= 0 && row < scaleNames.size()) {
      owner->loadScale(scaleNames[row]);
    }
  } catch (...) {
    // Silently handle any errors
  }
}

// Helper methods
void ScaleEditorComponent::updateListBox() {
  if (!scaleLibrary)
    return;
    
  try {
    scaleListBox.updateContent();
    scaleListBox.repaint();
  } catch (...) {
    // Silently handle any errors during update
  }
}

void ScaleEditorComponent::loadScale(const juce::String& scaleName) {
  currentScaleName = scaleName;
  nameEditor.setText(scaleName, juce::dontSendNotification);
  
  if (scaleLibrary) {
    auto intervals = scaleLibrary->getIntervals(scaleName);
    updateButtonsFromIntervals(intervals); // This also updates the keyboard
    
    // Enable/disable delete button based on whether it's a factory scale
    // For now, we'll check if the scale exists and is not one of the factory defaults
    // A better approach would be to check the isFactory flag, but we don't expose that
    bool isFactory = (scaleName == "Chromatic" || scaleName == "Major" || scaleName == "Minor" ||
                      scaleName == "Pentatonic Major" || scaleName == "Pentatonic Minor" ||
                      scaleName == "Blues" || scaleName == "Dorian" || scaleName == "Mixolydian" ||
                      scaleName == "Lydian" || scaleName == "Phrygian" || scaleName == "Locrian");
    deleteButton.setEnabled(!isFactory);
  }
}

void ScaleEditorComponent::clearEditor() {
  currentScaleName = "";
  nameEditor.clear();
  
  // Reset buttons: Root ON, others OFF
  intervalButtons[0].setToggleState(true, juce::dontSendNotification);
  for (int i = 1; i < 12; ++i) {
    intervalButtons[i].setToggleState(false, juce::dontSendNotification);
  }
  
  // Update keyboard
  updateKeyboardFromButtons();
  
  deleteButton.setEnabled(false);
}

void ScaleEditorComponent::updateButtonsFromIntervals(const std::vector<int>& intervals) {
  // First, turn all buttons off (except Root which is always on)
  for (int i = 1; i < 12; ++i) {
    intervalButtons[i].setToggleState(false, juce::dontSendNotification);
  }
  
  // Then, turn on buttons that match the intervals
  for (int interval : intervals) {
    if (interval >= 0 && interval < 12) {
      intervalButtons[interval].setToggleState(true, juce::dontSendNotification);
    }
  }
  
  // Update keyboard visualizer
  updateKeyboardFromButtons();
}

void ScaleEditorComponent::updateKeyboardFromButtons() {
  std::vector<int> activeIntervals;
  for (int i = 0; i < 12; ++i) {
    if (intervalButtons[i].getToggleState()) {
      activeIntervals.push_back(i);
    }
  }
  keyboardComponent.setActiveIntervals(activeIntervals);
}

std::vector<int> ScaleEditorComponent::buildIntervalsFromButtons() {
  std::vector<int> intervals;
  
  // Iterate through buttons (0 to 11) and add intervals for toggled buttons
  for (int i = 0; i < 12; ++i) {
    if (intervalButtons[i].getToggleState()) {
      intervals.push_back(i);
    }
  }
  
  return intervals;
}

void ScaleEditorComponent::changeListenerCallback(juce::ChangeBroadcaster* source) {
  if (source == scaleLibrary) {
    updateListBox();
  }
}
