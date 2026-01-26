#include "SettingsManager.h"

SettingsManager::SettingsManager() {
  rootNode = juce::ValueTree("OmniKeySettings");
  rootNode.setProperty("pitchBendRange", 12, nullptr);
  rootNode.addListener(this);
}

SettingsManager::~SettingsManager() {
  rootNode.removeListener(this);
}

int SettingsManager::getPitchBendRange() const {
  return rootNode.getProperty("pitchBendRange", 12);
}

void SettingsManager::setPitchBendRange(int range) {
  rootNode.setProperty("pitchBendRange", juce::jlimit(1, 96, range), nullptr);
  sendChangeMessage();
}

void SettingsManager::saveToXml(juce::File file) {
  auto xml = rootNode.createXml();
  if (xml != nullptr) {
    file.getParentDirectory().createDirectory();
    xml->writeTo(file);
  }
}

void SettingsManager::loadFromXml(juce::File file) {
  if (file.existsAsFile()) {
    auto xml = juce::XmlDocument::parse(file);
    if (xml != nullptr) {
      rootNode = juce::ValueTree::fromXml(*xml);
      if (!rootNode.isValid()) {
        // If load failed, reset to defaults
        rootNode = juce::ValueTree("OmniKeySettings");
        rootNode.setProperty("pitchBendRange", 12, nullptr);
      }
      rootNode.addListener(this);
      sendChangeMessage();
    }
  }
}

void SettingsManager::valueTreePropertyChanged(juce::ValueTree& tree,
                                               const juce::Identifier& property) {
  if (tree == rootNode) {
    sendChangeMessage();
  }
}
