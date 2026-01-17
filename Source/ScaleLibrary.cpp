#include "ScaleLibrary.h"
#include <algorithm>

ScaleLibrary::ScaleLibrary() {
  rootNode = juce::ValueTree("ScaleLibrary");
  rootNode.addListener(this);
  loadDefaults();
}

ScaleLibrary::~ScaleLibrary() {
  rootNode.removeListener(this);
}

void ScaleLibrary::loadDefaults() {
  // Factory scales
  createScale("Chromatic", {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11});
  createScale("Major", {0, 2, 4, 5, 7, 9, 11});
  createScale("Minor", {0, 2, 3, 5, 7, 8, 10});
  createScale("Pentatonic Major", {0, 2, 4, 7, 9});
  createScale("Pentatonic Minor", {0, 3, 5, 7, 10});
  createScale("Blues", {0, 3, 5, 6, 7, 10});
  createScale("Dorian", {0, 2, 3, 5, 7, 9, 10});
  createScale("Mixolydian", {0, 2, 4, 5, 7, 9, 10});
  createScale("Lydian", {0, 2, 4, 6, 7, 9, 11});
  createScale("Phrygian", {0, 1, 3, 5, 7, 8, 10});
  createScale("Locrian", {0, 1, 3, 5, 6, 8, 10});
}

void ScaleLibrary::createScale(const juce::String& name, const std::vector<int>& intervals) {
  if (name.isEmpty() || intervals.empty())
    return;

  bool isFactory = false;
  
  // Check if scale already exists
  if (hasScale(name)) {
    const Scale* existingScale = findScaleByName(name);
    if (existingScale && existingScale->isFactory) {
      // Cannot modify factory scale
      return;
    }
    // Update existing user scale
    auto* scale = findScaleByNameMutable(name);
    if (scale) {
      isFactory = scale->isFactory;
      scale->intervals = intervals;
    }
  } else {
    // Create new scale
    Scale newScale;
    newScale.name = name;
    newScale.intervals = intervals;
    newScale.isFactory = false;
    isFactory = false;
    scales.push_back(newScale);
  }

  // Update ValueTree
  juce::ValueTree scaleVT("Scale");
  scaleVT.setProperty("name", name, nullptr);
  
  // Serialize intervals as comma-separated string (ValueTree doesn't support vector directly)
  juce::StringArray intervalsArray;
  for (int interval : intervals) {
    intervalsArray.add(juce::String(interval));
  }
  scaleVT.setProperty("intervals", intervalsArray.joinIntoString(","), nullptr);
  scaleVT.setProperty("factory", isFactory, nullptr);
  
  // Remove old child if exists
  for (int i = 0; i < rootNode.getNumChildren(); ++i) {
    if (rootNode.getChild(i).getProperty("name") == name) {
      rootNode.removeChild(i, nullptr);
      break;
    }
  }
  
  rootNode.addChild(scaleVT, -1, nullptr);
  sendChangeMessage();
}

void ScaleLibrary::deleteScale(const juce::String& name) {
  if (name.isEmpty())
    return;

  const Scale* scale = findScaleByName(name);
  if (!scale || scale->isFactory)
    return; // Cannot delete factory scales

  // Remove from ValueTree
  for (int i = 0; i < rootNode.getNumChildren(); ++i) {
    if (rootNode.getChild(i).getProperty("name") == name) {
      rootNode.removeChild(i, nullptr);
      break;
    }
  }

  // Remove from scales vector
  scales.erase(
    std::remove_if(scales.begin(), scales.end(),
                   [&name](const Scale& s) { return s.name == name; }),
    scales.end()
  );

  sendChangeMessage();
}

std::vector<int> ScaleLibrary::getIntervals(const juce::String& name) const {
  const Scale* scale = findScaleByName(name);
  if (scale)
    return scale->intervals;

  // Fallback to Major if not found
  const Scale* majorScale = findScaleByName("Major");
  if (majorScale)
    return majorScale->intervals;

  // Ultimate fallback
  return {0, 2, 4, 5, 7, 9, 11};
}

juce::StringArray ScaleLibrary::getScaleNames() const {
  juce::StringArray names;
  for (const auto& scale : scales) {
    names.add(scale.name);
  }
  return names;
}

bool ScaleLibrary::hasScale(const juce::String& name) const {
  return findScaleByName(name) != nullptr;
}

void ScaleLibrary::saveToXml(juce::File file) const {
  auto xml = rootNode.createXml();
  if (xml)
    xml->writeTo(file);
}

void ScaleLibrary::loadFromXml(juce::File file) {
  if (!file.existsAsFile())
    return;

  auto xml = juce::XmlDocument::parse(file);
  if (xml) {
    auto vt = juce::ValueTree::fromXml(*xml);
    if (vt.isValid() && vt.hasType("ScaleLibrary")) {
      restoreFromValueTree(vt);
    }
  }
}

void ScaleLibrary::valueTreePropertyChanged(juce::ValueTree& tree,
                                            const juce::Identifier& property) {
  rebuildScalesFromValueTree();
  sendChangeMessage();
}

void ScaleLibrary::valueTreeChildAdded(juce::ValueTree& parent,
                                       juce::ValueTree& child) {
  rebuildScalesFromValueTree();
  sendChangeMessage();
}

void ScaleLibrary::valueTreeChildRemoved(juce::ValueTree& parent,
                                         juce::ValueTree& child,
                                         int indexOldChildRemoved) {
  rebuildScalesFromValueTree();
  sendChangeMessage();
}

void ScaleLibrary::valueTreeChildOrderChanged(juce::ValueTree& parent,
                                              int oldIndex,
                                              int newIndex) {
  rebuildScalesFromValueTree();
  sendChangeMessage();
}

juce::ValueTree ScaleLibrary::toValueTree() const {
  return rootNode.createCopy();
}

void ScaleLibrary::restoreFromValueTree(const juce::ValueTree& vt) {
  if (!vt.isValid() || !vt.hasType("ScaleLibrary"))
    return;

  rootNode.removeAllChildren(nullptr);
  
  for (int i = 0; i < vt.getNumChildren(); ++i) {
    auto child = vt.getChild(i);
    if (child.hasType("Scale")) {
      rootNode.addChild(child.createCopy(), -1, nullptr);
    }
  }

  rebuildScalesFromValueTree();
  sendChangeMessage();
}

const Scale* ScaleLibrary::findScaleByName(const juce::String& name) const {
  for (const auto& scale : scales) {
    if (scale.name == name)
      return &scale;
  }
  return nullptr;
}

Scale* ScaleLibrary::findScaleByNameMutable(const juce::String& name) {
  for (auto& scale : scales) {
    if (scale.name == name)
      return &scale;
  }
  return nullptr;
}

void ScaleLibrary::rebuildScalesFromValueTree() {
  scales.clear();

  for (int i = 0; i < rootNode.getNumChildren(); ++i) {
    auto scaleVT = rootNode.getChild(i);
    if (scaleVT.hasType("Scale")) {
      Scale scale;
      scale.name = scaleVT.getProperty("name").toString();
      scale.isFactory = scaleVT.getProperty("factory", false);

      // Parse intervals from ValueTree property (stored as comma-separated string)
      juce::String intervalsStr = scaleVT.getProperty("intervals").toString();
      if (intervalsStr.isNotEmpty()) {
        juce::StringArray intervalsArray;
        intervalsArray.addTokens(intervalsStr, ",", "");
        for (const auto& intervalStr : intervalsArray) {
          int interval = intervalStr.getIntValue();
          scale.intervals.push_back(interval);
        }
      }

      scales.push_back(scale);
    }
  }
}
