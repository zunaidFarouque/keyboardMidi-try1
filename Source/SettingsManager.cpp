#include "SettingsManager.h"

SettingsManager::SettingsManager() {
  rootNode = juce::ValueTree("OmniKeySettings");
  rootNode.setProperty("pitchBendRange", 12, nullptr);
  rootNode.setProperty("midiModeActive", false, nullptr);
  rootNode.setProperty("toggleKeyCode", 0x91, nullptr);          // VK_SCROLL
  rootNode.setProperty("performanceModeKeyCode", 0x7A, nullptr); // VK_F11
  rootNode.setProperty("lastMidiDevice", "", nullptr);
  rootNode.setProperty("studioMode", false,
                       nullptr); // Default: Studio Mode OFF
  rootNode.setProperty("capWindowRefresh30Fps", true,
                       nullptr); // Default: cap at 30 FPS
  // Visualizer opacity defaults: semi-transparent overlays so X/Y can be
  // layered.
  rootNode.setProperty("visualizerXOpacity", 0.45, nullptr);
  rootNode.setProperty("visualizerYOpacity", 0.45, nullptr);
  rootNode.addListener(this);
}

SettingsManager::~SettingsManager() { rootNode.removeListener(this); }

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

int SettingsManager::getPerformanceModeKey() const {
  return rootNode.getProperty("performanceModeKeyCode", 0x7A); // VK_F11 default
}

void SettingsManager::setPerformanceModeKey(int vkCode) {
  rootNode.setProperty("performanceModeKeyCode", vkCode, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getLastMidiDevice() const {
  return rootNode.getProperty("lastMidiDevice", "").toString();
}

void SettingsManager::setLastMidiDevice(const juce::String &name) {
  rootNode.setProperty("lastMidiDevice", name, nullptr);
  sendChangeMessage();
}

bool SettingsManager::isStudioMode() const {
  return rootNode.getProperty("studioMode", false);
}

void SettingsManager::setStudioMode(bool active) {
  rootNode.setProperty("studioMode", active, nullptr);
  sendChangeMessage();
}

bool SettingsManager::isCapWindowRefresh30Fps() const {
  return rootNode.getProperty("capWindowRefresh30Fps", true);
}

void SettingsManager::setCapWindowRefresh30Fps(bool cap) {
  rootNode.setProperty("capWindowRefresh30Fps", cap, nullptr);
  sendChangeMessage();
}

int SettingsManager::getWindowRefreshIntervalMs() const {
  return isCapWindowRefresh30Fps() ? 34 : 16; // 30 FPS cap vs ~60 FPS
}

float SettingsManager::getVisualizerXOpacity() const {
  double v = rootNode.getProperty("visualizerXOpacity", 0.45);
  return juce::jlimit(0.0, 1.0, v);
}

void SettingsManager::setVisualizerXOpacity(float alpha) {
  double v = juce::jlimit(0.0f, 1.0f, alpha);
  rootNode.setProperty("visualizerXOpacity", v, nullptr);
  sendChangeMessage();
}

float SettingsManager::getVisualizerYOpacity() const {
  double v = rootNode.getProperty("visualizerYOpacity", 0.45);
  return juce::jlimit(0.0, 1.0, v);
}

void SettingsManager::setVisualizerYOpacity(float alpha) {
  double v = juce::jlimit(0.0f, 1.0f, alpha);
  rootNode.setProperty("visualizerYOpacity", v, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getTypePropertyName(ActionType type) const {
  switch (type) {
  case ActionType::Note:
    return "color_Note";
  case ActionType::Expression:
    return "color_Expression";
  case ActionType::Command:
    return "color_Command";
  case ActionType::Macro:
    return "color_Macro";
  default:
    return "color_Note";
  }
}

namespace {
juce::Colour getTypeColorDefault(ActionType type) {
  switch (type) {
  case ActionType::Note:
    return juce::Colours::skyblue;
  case ActionType::Expression:
    return juce::Colours::orange;
  case ActionType::Command:
    return juce::Colours::red;
  case ActionType::Macro:
    return juce::Colours::yellow;
  default:
    return juce::Colours::grey;
  }
}
} // namespace

juce::Colour SettingsManager::getTypeColor(ActionType type) const {
  juce::Identifier key(getTypePropertyName(type));
  juce::var v = rootNode.getProperty(key);
  if (v.isVoid() || !v.toString().isNotEmpty())
    return getTypeColorDefault(type);
  juce::Colour c = juce::Colour::fromString(v.toString());
  if (c == juce::Colours::transparentBlack)
    return getTypeColorDefault(type);
  return c;
}

void SettingsManager::setTypeColor(ActionType type, juce::Colour colour) {
  rootNode.setProperty(juce::Identifier(getTypePropertyName(type)),
                       colour.toString(), nullptr);
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
        rootNode.setProperty("performanceModeKeyCode", 0x7A, nullptr);
        rootNode.setProperty("lastMidiDevice", "", nullptr);
        rootNode.setProperty("studioMode", false, nullptr);
        rootNode.setProperty("capWindowRefresh30Fps", true, nullptr);
        rootNode.setProperty("visualizerXOpacity", 0.45, nullptr);
        rootNode.setProperty("visualizerYOpacity", 0.45, nullptr);
      } else {
        // Phase 43: Validate (sanitize) – prevent divide-by-zero from bad saved
        // data
        int pb = rootNode.getProperty("pitchBendRange", 12);
        if (pb < 1) {
          rootNode.setProperty("pitchBendRange", 12, nullptr);
        }
        // Ensure new visualizer properties exist even for older settings files.
        if (!rootNode.hasProperty("visualizerXOpacity"))
          rootNode.setProperty("visualizerXOpacity", 0.45, nullptr);
        if (!rootNode.hasProperty("visualizerYOpacity"))
          rootNode.setProperty("visualizerYOpacity", 0.45, nullptr);
      }
      rootNode.addListener(this);
      sendChangeMessage();
    }
  }
}

void SettingsManager::valueTreePropertyChanged(
    juce::ValueTree &tree, const juce::Identifier &property) {
  if (tree == rootNode) {
    sendChangeMessage();
  }
}
