#include "SettingsManager.h"

SettingsManager::SettingsManager() {
  rootNode = juce::ValueTree("OmniKeySettings");
  rootNode.setProperty("pitchBendRange", 12, nullptr);
  rootNode.setProperty("midiModeActive", false, nullptr);
  rootNode.setProperty("toggleKeyCode", 0x91, nullptr); // VK_SCROLL
  rootNode.setProperty("lastMidiDevice", "", nullptr);
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

bool SettingsManager::isMidiModeActive() const {
  return rootNode.getProperty("midiModeActive", false);
}

void SettingsManager::setMidiModeActive(bool active) {
  rootNode.setProperty("midiModeActive", active, nullptr);
  sendChangeMessage();
}

int SettingsManager::getToggleKey() const {
  return rootNode.getProperty("toggleKeyCode", 0x91); // VK_SCROLL default
}

void SettingsManager::setToggleKey(int vkCode) {
  rootNode.setProperty("toggleKeyCode", vkCode, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getLastMidiDevice() const {
  return rootNode.getProperty("lastMidiDevice", "").toString();
}

void SettingsManager::setLastMidiDevice(const juce::String& name) {
  rootNode.setProperty("lastMidiDevice", name, nullptr);
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
        rootNode.setProperty("midiModeActive", false, nullptr);
        rootNode.setProperty("toggleKeyCode", 0x91, nullptr);
        rootNode.setProperty("lastMidiDevice", "", nullptr);
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
