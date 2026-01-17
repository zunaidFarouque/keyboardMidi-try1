#pragma once
#include "ScaleLibrary.h"
#include "MusicalKeyboardComponent.h"
#include <JuceHeader.h>

class ScaleEditorComponent : public juce::Component,
                             public juce::ChangeListener {
public:
  ScaleEditorComponent(ScaleLibrary* scaleLib);
  ~ScaleEditorComponent() override;

  void paint(juce::Graphics&) override;
  void resized() override;

private:
  ScaleLibrary* scaleLibrary;

  // UI Components
  juce::ListBox scaleListBox{"ScaleList", nullptr}; // Model set in constructor
  juce::TextEditor nameEditor;
  juce::Label nameLabel;
  
  // Musical keyboard visualizer
  MusicalKeyboardComponent keyboardComponent;
  
  // 12 interval toggle buttons (0 = Root, 1-11 = m2 to M7)
  juce::ToggleButton intervalButtons[12];
  juce::Label intervalLabels[12];
  
  juce::TextButton saveButton;
  juce::TextButton deleteButton;
  juce::TextButton newButton;

  // Current editing scale name
  juce::String currentScaleName;

  // ListBox model
  class ScaleListModel : public juce::ListBoxModel {
  public:
    ScaleListModel(ScaleLibrary* lib, ScaleEditorComponent* owner)
      : scaleLibrary(lib), owner(owner) {}

    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g,
                         int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;

  private:
    ScaleLibrary* scaleLibrary;
    ScaleEditorComponent* owner;
  };

  ScaleListModel listModel;

  // Helper methods
  void updateListBox();
  void loadScale(const juce::String& scaleName);
  void clearEditor();
  void updateButtonsFromIntervals(const std::vector<int>& intervals);
  void updateKeyboardFromButtons();
  std::vector<int> buildIntervalsFromButtons();

  // ChangeListener implementation
  void changeListenerCallback(juce::ChangeBroadcaster* source) override;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaleEditorComponent)
};
