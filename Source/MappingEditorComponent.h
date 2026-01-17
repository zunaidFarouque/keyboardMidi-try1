#pragma once
#include "PresetManager.h"
#include "RawInputManager.h"
#include <JuceHeader.h>

class MappingEditorComponent
    : public juce::Component,
      public juce::TableListBoxModel,
      public juce::ValueTree::Listener,
      public RawInputManager::Listener // <--- ADD LISTENER
{
public:
  MappingEditorComponent(PresetManager &pm, RawInputManager &rawInputMgr);
  ~MappingEditorComponent() override;

  // RawInputManager::Listener implementation
  void handleRawKeyEvent(uintptr_t deviceHandle, int keyCode,
                         bool isDown) override;

  void paint(juce::Graphics &) override;
  void resized() override;

  // TableListBoxModel methods
  int getNumRows() override;
  void paintRowBackground(juce::Graphics &, int rowNumber, int width,
                          int height, bool rowIsSelected) override;
  void paintCell(juce::Graphics &, int rowNumber, int columnId, int width,
                 int height, bool rowIsSelected) override;

  // ValueTree::Listener methods (Updates UI when data changes)

  // ValueTree::Listener methods
  void valueTreeChildAdded(juce::ValueTree &, juce::ValueTree &) override;
  void valueTreeChildRemoved(juce::ValueTree &, juce::ValueTree &,
                             int) override;
  void valueTreePropertyChanged(juce::ValueTree &,
                                const juce::Identifier &) override;
  void valueTreeParentChanged(juce::ValueTree &) override;

private:
  PresetManager &presetManager;
  RawInputManager &rawInputManager;
  juce::TableListBox table;
  juce::TextButton addButton;
  juce::ToggleButton learnButton;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MappingEditorComponent)
};