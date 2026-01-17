#pragma once
#include <JuceHeader.h>
#include <vector>

struct Scale {
  juce::String name;
  std::vector<int> intervals;
  bool isFactory;
};

class ScaleLibrary : public juce::ChangeBroadcaster,
                     public juce::ValueTree::Listener {
public:
  ScaleLibrary();
  ~ScaleLibrary() override;

  // Load default factory scales
  void loadDefaults();

  // Create a new user-defined scale
  void createScale(const juce::String& name, const std::vector<int>& intervals);

  // Delete a scale (cannot delete factory scales)
  void deleteScale(const juce::String& name);

  // Get intervals for a scale name (returns Major if not found)
  std::vector<int> getIntervals(const juce::String& name) const;

  // Get all scale names
  juce::StringArray getScaleNames() const;

  // Check if a scale exists
  bool hasScale(const juce::String& name) const;

  // Save/Load from XML
  void saveToXml(juce::File file) const;
  void loadFromXml(juce::File file);

  // ValueTree::Listener implementation
  void valueTreePropertyChanged(juce::ValueTree& tree,
                                const juce::Identifier& property) override;
  void valueTreeChildAdded(juce::ValueTree& parent,
                           juce::ValueTree& child) override;
  void valueTreeChildRemoved(juce::ValueTree& parent,
                             juce::ValueTree& child,
                             int indexOldChildRemoved) override;
  void valueTreeChildOrderChanged(juce::ValueTree& parent,
                                  int oldIndex,
                                  int newIndex) override;

  // Get ValueTree for serialization
  juce::ValueTree toValueTree() const;
  void restoreFromValueTree(const juce::ValueTree& vt);

private:
  juce::ValueTree rootNode;
  std::vector<Scale> scales;

  // Helper to find scale by name
  const Scale* findScaleByName(const juce::String& name) const;
  Scale* findScaleByNameMutable(const juce::String& name);

  // Update scales vector from ValueTree
  void rebuildScalesFromValueTree();

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ScaleLibrary)
};
