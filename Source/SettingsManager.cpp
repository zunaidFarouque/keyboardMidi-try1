#include "SettingsManager.h"

SettingsManager::SettingsManager() {
  rootNode = juce::ValueTree("MIDIQySettings");
  rootNode.setProperty("pitchBendRange", 12, nullptr);
  rootNode.setProperty("midiModeActive", false, nullptr);
  rootNode.setProperty("toggleKeyCode", 0x7B, nullptr);          // VK_F12
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
  rootNode.setProperty("showTouchpadVisualizerInMiniWindow", false, nullptr);
  rootNode.setProperty("hideCursorInPerformanceMode", false, nullptr);
  rootNode.setProperty("miniWindowPosition", "", nullptr);
  rootNode.setProperty("rememberUiState", true, nullptr);
  rootNode.setProperty("delayMidiEnabled", false, nullptr);
  rootNode.setProperty("delayMidiSeconds", 1, nullptr);

  // Ensure UIState child exists for layout persistence
  if (!rootNode.getChildWithName("UIState").isValid()) {
    rootNode.addChild(juce::ValueTree("UIState"), -1, nullptr);
  }
  rootNode.addListener(this);
  updateCachedMidiModeActive();
}

SettingsManager::~SettingsManager() { rootNode.removeListener(this); }

int SettingsManager::getPitchBendRange() const {
  return rootNode.getProperty("pitchBendRange", 12);
}

void SettingsManager::updateCachedStepsPerSemitone() {
  int range = juce::jmax(1, getPitchBendRange());
  cachedStepsPerSemitone = 8192.0 / static_cast<double>(range);
}

double SettingsManager::getStepsPerSemitone() const {
  return cachedStepsPerSemitone;
}

void SettingsManager::setPitchBendRange(int range) {
  rootNode.setProperty("pitchBendRange", juce::jlimit(1, 96, range), nullptr);
  updateCachedStepsPerSemitone();
  sendChangeMessage();
}

bool SettingsManager::isMidiModeActive() const { return cachedMidiModeActive; }

void SettingsManager::updateCachedMidiModeActive() {
  cachedMidiModeActive = rootNode.getProperty("midiModeActive", false);
}

void SettingsManager::setMidiModeActive(bool active) {
  rootNode.setProperty("midiModeActive", active, nullptr);
  updateCachedMidiModeActive();
  sendChangeMessage();
}

int SettingsManager::getToggleKey() const {
  return rootNode.getProperty("toggleKeyCode", 0x7B); // VK_F12 default
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

bool SettingsManager::isDelayMidiEnabled() const {
  return rootNode.getProperty("delayMidiEnabled", false);
}

void SettingsManager::setDelayMidiEnabled(bool enabled) {
  rootNode.setProperty("delayMidiEnabled", enabled, nullptr);
  sendChangeMessage();
}

int SettingsManager::getDelayMidiSeconds() const {
  return juce::jlimit(
      1, 10, static_cast<int>(rootNode.getProperty("delayMidiSeconds", 1)));
}

void SettingsManager::setDelayMidiSeconds(int seconds) {
  rootNode.setProperty("delayMidiSeconds", juce::jlimit(1, 10, seconds),
                       nullptr);
  sendChangeMessage();
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

bool SettingsManager::getShowTouchpadVisualizerInMiniWindow() const {
  return rootNode.getProperty("showTouchpadVisualizerInMiniWindow", false);
}

void SettingsManager::setShowTouchpadVisualizerInMiniWindow(bool show) {
  rootNode.setProperty("showTouchpadVisualizerInMiniWindow", show, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getHideCursorInPerformanceMode() const {
  return rootNode.getProperty("hideCursorInPerformanceMode", false);
}

void SettingsManager::setHideCursorInPerformanceMode(bool hide) {
  rootNode.setProperty("hideCursorInPerformanceMode", hide, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getMiniWindowPosition() const {
  return rootNode.getProperty("miniWindowPosition", "").toString();
}

void SettingsManager::setMiniWindowPosition(const juce::String &state) {
  rootNode.setProperty("miniWindowPosition", state, nullptr);
  sendChangeMessage();
}

void SettingsManager::resetMiniWindowPosition() {
  rootNode.setProperty("miniWindowPosition", "", nullptr);
  sendChangeMessage();
}

juce::ValueTree SettingsManager::getUiStateNode() {
  auto ui = rootNode.getChildWithName("UIState");
  if (!ui.isValid()) {
    ui = juce::ValueTree("UIState");
    rootNode.addChild(ui, -1, nullptr);
  }
  return ui;
}

juce::ValueTree SettingsManager::getUiStateNode() const {
  return rootNode.getChildWithName("UIState");
}

bool SettingsManager::getRememberUiState() const {
  return static_cast<bool>(rootNode.getProperty("rememberUiState", true));
}

void SettingsManager::setRememberUiState(bool remember) {
  rootNode.setProperty("rememberUiState", remember, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getMainWindowState() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return {};
  return ui.getProperty("mainWindowState", "").toString();
}

void SettingsManager::setMainWindowState(const juce::String &state) {
  auto ui = getUiStateNode();
  ui.setProperty("mainWindowState", state, nullptr);
  sendChangeMessage();
}

int SettingsManager::getMainTabIndex() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return 0;
  int idx = static_cast<int>(ui.getProperty("mainTabIndex", 0));
  // Clamp to valid range of main tabs (Mappings=0, Zones=1, Touchpad=2, Settings=3).
  if (idx < 0 || idx > 8)
    idx = 0;
  return idx;
}

void SettingsManager::setMainTabIndex(int index) {
  auto ui = getUiStateNode();
  ui.setProperty("mainTabIndex", index, nullptr);
  sendChangeMessage();
}

int SettingsManager::getVerticalSplitPos() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return -1;
  return static_cast<int>(ui.getProperty("verticalSplitPos", -1));
}

void SettingsManager::setVerticalSplitPos(int pos) {
  auto ui = getUiStateNode();
  ui.setProperty("verticalSplitPos", pos, nullptr);
  sendChangeMessage();
}

int SettingsManager::getHorizontalSplitPos() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return -1;
  return static_cast<int>(ui.getProperty("horizontalSplitPos", -1));
}

void SettingsManager::setHorizontalSplitPos(int pos) {
  auto ui = getUiStateNode();
  ui.setProperty("horizontalSplitPos", pos, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getVisualizerVisible() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return true;
  return static_cast<bool>(ui.getProperty("visualizerVisible", true));
}

void SettingsManager::setVisualizerVisible(bool visible) {
  auto ui = getUiStateNode();
  ui.setProperty("visualizerVisible", visible, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getVisualizerPoppedOut() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return false;
  return static_cast<bool>(ui.getProperty("visualizerPoppedOut", false));
}

void SettingsManager::setVisualizerPoppedOut(bool poppedOut) {
  auto ui = getUiStateNode();
  ui.setProperty("visualizerPoppedOut", poppedOut, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getVisualizerWindowState() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return {};
  return ui.getProperty("visualizerWindowState", "").toString();
}

void SettingsManager::setVisualizerWindowState(const juce::String &state) {
  auto ui = getUiStateNode();
  ui.setProperty("visualizerWindowState", state, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getEditorVisible() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return true;
  return static_cast<bool>(ui.getProperty("editorVisible", true));
}

void SettingsManager::setEditorVisible(bool visible) {
  auto ui = getUiStateNode();
  ui.setProperty("editorVisible", visible, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getEditorPoppedOut() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return false;
  return static_cast<bool>(ui.getProperty("editorPoppedOut", false));
}

void SettingsManager::setEditorPoppedOut(bool poppedOut) {
  auto ui = getUiStateNode();
  ui.setProperty("editorPoppedOut", poppedOut, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getEditorWindowState() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return {};
  return ui.getProperty("editorWindowState", "").toString();
}

void SettingsManager::setEditorWindowState(const juce::String &state) {
  auto ui = getUiStateNode();
  ui.setProperty("editorWindowState", state, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getLogVisible() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return true;
  return static_cast<bool>(ui.getProperty("logVisible", true));
}

void SettingsManager::setLogVisible(bool visible) {
  auto ui = getUiStateNode();
  ui.setProperty("logVisible", visible, nullptr);
  sendChangeMessage();
}

bool SettingsManager::getLogPoppedOut() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return false;
  return static_cast<bool>(ui.getProperty("logPoppedOut", false));
}

void SettingsManager::setLogPoppedOut(bool poppedOut) {
  auto ui = getUiStateNode();
  ui.setProperty("logPoppedOut", poppedOut, nullptr);
  sendChangeMessage();
}

juce::String SettingsManager::getLogWindowState() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return {};
  return ui.getProperty("logWindowState", "").toString();
}

void SettingsManager::setLogWindowState(const juce::String &state) {
  auto ui = getUiStateNode();
  ui.setProperty("logWindowState", state, nullptr);
  sendChangeMessage();
}

int SettingsManager::getMappingsSelectedLayerId() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return 0;
  int layerId =
      static_cast<int>(ui.getProperty("mappingsSelectedLayerId", 0));
  if (layerId < 0 || layerId > 8)
    layerId = 0;
  return layerId;
}

void SettingsManager::setMappingsSelectedLayerId(int layerId) {
  auto ui = getUiStateNode();
  ui.setProperty("mappingsSelectedLayerId", layerId, nullptr);
  sendChangeMessage();
}

int SettingsManager::getMappingsSelectedRow() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return -1;
  int row = static_cast<int>(ui.getProperty("mappingsSelectedRow", -1));
  if (row < -1)
    row = -1;
  return row;
}

void SettingsManager::setMappingsSelectedRow(int row) {
  auto ui = getUiStateNode();
  ui.setProperty("mappingsSelectedRow", row, nullptr);
  sendChangeMessage();
}

int SettingsManager::getZonesSelectedIndex() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return -1;
  int idx = static_cast<int>(ui.getProperty("zonesSelectedIndex", -1));
  if (idx < -1)
    idx = -1;
  return idx;
}

void SettingsManager::setZonesSelectedIndex(int index) {
  auto ui = getUiStateNode();
  ui.setProperty("zonesSelectedIndex", index, nullptr);
  sendChangeMessage();
}

int SettingsManager::getTouchpadSelectedRow() const {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return -1;
  int row = static_cast<int>(ui.getProperty("touchpadSelectedRow", -1));
  if (row < -1)
    row = -1;
  return row;
}

void SettingsManager::setTouchpadSelectedRow(int row) {
  auto ui = getUiStateNode();
  ui.setProperty("touchpadSelectedRow", row, nullptr);
  sendChangeMessage();
}

void SettingsManager::resetUiStateToDefaults() {
  // Remove any existing UIState child and recreate a fresh one.
  auto ui = rootNode.getChildWithName("UIState");
  if (ui.isValid())
    rootNode.removeChild(ui, nullptr);
  rootNode.addChild(juce::ValueTree("UIState"), -1, nullptr);
  // Turn off UI state remembering so the next run uses hard-coded defaults
  // (1200x800 centred) and does not immediately overwrite them with the old
  // window size. The user can re-enable "Remember UI layout" in Settings.
  rootNode.setProperty("rememberUiState", false, nullptr);
  sendChangeMessage();
}

void SettingsManager::sanitizeUiStateNode() {
  auto ui = getUiStateNode();
  if (!ui.isValid())
    return;

  // Ensure required properties exist with safe defaults.
  if (!ui.hasProperty("mainWindowState"))
    ui.setProperty("mainWindowState", "", nullptr);

  int mainTabIndex =
      static_cast<int>(ui.getProperty("mainTabIndex", getMainTabIndex()));
  if (mainTabIndex < 0 || mainTabIndex > 8)
    mainTabIndex = 0;
  ui.setProperty("mainTabIndex", mainTabIndex, nullptr);

  bool visVisible =
      static_cast<bool>(ui.getProperty("visualizerVisible", true));
  ui.setProperty("visualizerVisible", visVisible, nullptr);

  bool edVisible = static_cast<bool>(ui.getProperty("editorVisible", true));
  ui.setProperty("editorVisible", edVisible, nullptr);

  bool lgVisible = static_cast<bool>(ui.getProperty("logVisible", true));
  ui.setProperty("logVisible", lgVisible, nullptr);

  int mappingsLayer =
      static_cast<int>(ui.getProperty("mappingsSelectedLayerId", 0));
  if (mappingsLayer < 0 || mappingsLayer > 8)
    mappingsLayer = 0;
  ui.setProperty("mappingsSelectedLayerId", mappingsLayer, nullptr);

  int mappingsRow =
      static_cast<int>(ui.getProperty("mappingsSelectedRow", -1));
  if (mappingsRow < -1)
    mappingsRow = -1;
  ui.setProperty("mappingsSelectedRow", mappingsRow, nullptr);

  int zonesIndex =
      static_cast<int>(ui.getProperty("zonesSelectedIndex", -1));
  if (zonesIndex < -1)
    zonesIndex = -1;
  ui.setProperty("zonesSelectedIndex", zonesIndex, nullptr);

  int touchpadRow =
      static_cast<int>(ui.getProperty("touchpadSelectedRow", -1));
  if (touchpadRow < -1)
    touchpadRow = -1;
  ui.setProperty("touchpadSelectedRow", touchpadRow, nullptr);
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
        rootNode = juce::ValueTree("MIDIQySettings");
        rootNode.setProperty("pitchBendRange", 12, nullptr);
        rootNode.setProperty("midiModeActive", false, nullptr);
        rootNode.setProperty("toggleKeyCode", 0x7B, nullptr);
        rootNode.setProperty("performanceModeKeyCode", 0x7A, nullptr);
        rootNode.setProperty("lastMidiDevice", "", nullptr);
        rootNode.setProperty("studioMode", false, nullptr);
        rootNode.setProperty("capWindowRefresh30Fps", true, nullptr);
        rootNode.setProperty("visualizerXOpacity", 0.45, nullptr);
        rootNode.setProperty("visualizerYOpacity", 0.45, nullptr);
        rootNode.setProperty("delayMidiEnabled", false, nullptr);
        rootNode.setProperty("delayMidiSeconds", 1, nullptr);
        rootNode.setProperty("rememberUiState", true, nullptr);
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
        if (!rootNode.hasProperty("delayMidiEnabled"))
          rootNode.setProperty("delayMidiEnabled", false, nullptr);
        if (!rootNode.hasProperty("delayMidiSeconds"))
          rootNode.setProperty("delayMidiSeconds", 1, nullptr);
        if (!rootNode.hasProperty("hideCursorInPerformanceMode"))
          rootNode.setProperty("hideCursorInPerformanceMode", false, nullptr);
        if (!rootNode.hasProperty("rememberUiState"))
          rootNode.setProperty("rememberUiState", true, nullptr);
      }
      // Ensure UIState child exists even for older settings files.
      if (!rootNode.getChildWithName("UIState").isValid()) {
        rootNode.addChild(juce::ValueTree("UIState"), -1, nullptr);
      }
      // Normalize any invalid or missing UIState indices / flags.
      sanitizeUiStateNode();
      rootNode.addListener(this);
      updateCachedStepsPerSemitone();
      updateCachedMidiModeActive();
      sendChangeMessage();
    }
  }
}

void SettingsManager::valueTreePropertyChanged(
    juce::ValueTree &tree, const juce::Identifier &property) {
  if (tree == rootNode) {
    updateCachedMidiModeActive();
    sendChangeMessage();
  }
}
