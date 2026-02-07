#include "TouchpadMixerManager.h"

namespace {
const juce::Identifier kTouchpadMixers("TouchpadMixers");
const juce::Identifier kTouchpadMixer("TouchpadMixer");
const juce::Identifier kName("name");
const juce::Identifier kLayerId("layerId");
const juce::Identifier kNumFaders("numFaders");
const juce::Identifier kCcStart("ccStart");
const juce::Identifier kMidiChannel("midiChannel");
const juce::Identifier kInputMin("inputMin");
const juce::Identifier kInputMax("inputMax");
const juce::Identifier kOutputMin("outputMin");
const juce::Identifier kOutputMax("outputMax");
const juce::Identifier kQuickPrecision("quickPrecision");
const juce::Identifier kAbsRel("absRel");
const juce::Identifier kLockFree("lockFree");
const juce::Identifier kMuteButtonsEnabled("muteButtonsEnabled");
const juce::Identifier kType("type");
} // namespace

void TouchpadMixerManager::addStrip(const TouchpadMixerConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    strips_.push_back(config);
  }
  sendChangeMessage();
}

void TouchpadMixerManager::removeStrip(int index) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(strips_.size())) {
      strips_.erase(strips_.begin() + index);
    }
  }
  sendChangeMessage();
}

void TouchpadMixerManager::updateStrip(int index,
                                       const TouchpadMixerConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(strips_.size())) {
      strips_[static_cast<size_t>(index)] = config;
    }
  }
  sendChangeMessage();
}

static int enumToInt(TouchpadMixerQuickPrecision v) {
  return static_cast<int>(v);
}
static int enumToInt(TouchpadMixerAbsRel v) { return static_cast<int>(v); }
static int enumToInt(TouchpadMixerLockFree v) { return static_cast<int>(v); }

static juce::String typeToString(TouchpadType t) {
  return (t == TouchpadType::Mixer) ? "mixer" : "mixer";
}

juce::ValueTree TouchpadMixerManager::toValueTree() const {
  juce::ScopedReadLock lock(lock_);
  juce::ValueTree vt(kTouchpadMixers);
  for (const auto &s : strips_) {
    juce::ValueTree child(kTouchpadMixer);
    child.setProperty(kType, typeToString(s.type), nullptr);
    child.setProperty(kName, juce::String(s.name), nullptr);
    child.setProperty(kLayerId, s.layerId, nullptr);
    child.setProperty(kNumFaders, s.numFaders, nullptr);
    child.setProperty(kCcStart, s.ccStart, nullptr);
    child.setProperty(kMidiChannel, s.midiChannel, nullptr);
    child.setProperty(kInputMin, s.inputMin, nullptr);
    child.setProperty(kInputMax, s.inputMax, nullptr);
    child.setProperty(kOutputMin, s.outputMin, nullptr);
    child.setProperty(kOutputMax, s.outputMax, nullptr);
    child.setProperty(kQuickPrecision, enumToInt(s.quickPrecision), nullptr);
    child.setProperty(kAbsRel, enumToInt(s.absRel), nullptr);
    child.setProperty(kLockFree, enumToInt(s.lockFree), nullptr);
    child.setProperty(kMuteButtonsEnabled, s.muteButtonsEnabled, nullptr);
    vt.addChild(child, -1, nullptr);
  }
  return vt;
}

static TouchpadType parseType(const juce::var &v) {
  juce::String s = v.toString().trim();
  return s.equalsIgnoreCase("mixer") ? TouchpadType::Mixer : TouchpadType::Mixer;
}

void TouchpadMixerManager::restoreFromValueTree(const juce::ValueTree &vt) {
  if (!vt.isValid() || !vt.hasType(kTouchpadMixers))
    return;
  juce::ScopedWriteLock lock(lock_);
  strips_.clear();
  for (int i = 0; i < vt.getNumChildren(); ++i) {
    auto child = vt.getChild(i);
    if (!child.hasType(kTouchpadMixer))
      continue;
    TouchpadMixerConfig s;
    s.type = parseType(child.getProperty(kType, "mixer"));
    s.name = child.getProperty(kName, "Touchpad Mixer").toString().toStdString();
    s.layerId = juce::jlimit(0, 8, (int)child.getProperty(kLayerId, 0));
    s.numFaders =
        juce::jlimit(1, 32, (int)child.getProperty(kNumFaders, 5));
    s.ccStart = juce::jlimit(0, 127, (int)child.getProperty(kCcStart, 50));
    s.midiChannel =
        juce::jlimit(1, 16, (int)child.getProperty(kMidiChannel, 1));
    s.inputMin = static_cast<float>(child.getProperty(kInputMin, 0.0));
    s.inputMax = static_cast<float>(child.getProperty(kInputMax, 1.0));
    s.outputMin = juce::jlimit(0, 127, (int)child.getProperty(kOutputMin, 0));
    s.outputMax = juce::jlimit(0, 127, (int)child.getProperty(kOutputMax, 127));
    int qp = (int)child.getProperty(kQuickPrecision, 0);
    s.quickPrecision = (qp == 1) ? TouchpadMixerQuickPrecision::Precision
                                 : TouchpadMixerQuickPrecision::Quick;
    int ar = (int)child.getProperty(kAbsRel, 0);
    s.absRel =
        (ar == 1) ? TouchpadMixerAbsRel::Relative : TouchpadMixerAbsRel::Absolute;
    int lf = (int)child.getProperty(kLockFree, 1);
    s.lockFree = (lf == 0) ? TouchpadMixerLockFree::Lock
                           : TouchpadMixerLockFree::Free;
    s.muteButtonsEnabled = (bool)child.getProperty(kMuteButtonsEnabled, false);
    strips_.push_back(s);
  }
  sendChangeMessage();
}
