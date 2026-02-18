#include "TouchpadMixerManager.h"

namespace {
const juce::Identifier kTouchpadMixers("TouchpadMixers");
const juce::Identifier kTouchpadMixer("TouchpadMixer");
const juce::Identifier kLayoutGroupsNode("TouchpadLayoutGroups");
const juce::Identifier kLayoutGroupNode("TouchpadLayoutGroup");
const juce::Identifier kTouchpadMappingsNode("TouchpadMappings");
const juce::Identifier kTouchpadMappingNode("TouchpadMapping");
const juce::Identifier kName("name");
const juce::Identifier kLayerId("layerId");
const juce::Identifier kLayoutGroupId("layoutGroupId");
const juce::Identifier kLayoutGroupName("layoutGroupName");
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
const juce::Identifier kDrumPadRows("drumPadRows");
const juce::Identifier kDrumPadColumns("drumPadColumns");
const juce::Identifier kDrumPadMidiNoteStart("drumPadMidiNoteStart");
const juce::Identifier kDrumPadBaseVelocity("drumPadBaseVelocity");
const juce::Identifier kDrumPadVelocityRandom("drumPadVelocityRandom");
const juce::Identifier kDrumPadDeadZoneLeft("drumPadDeadZoneLeft");
const juce::Identifier kDrumPadDeadZoneRight("drumPadDeadZoneRight");
const juce::Identifier kDrumPadDeadZoneTop("drumPadDeadZoneTop");
const juce::Identifier kDrumPadDeadZoneBottom("drumPadDeadZoneBottom");
const juce::Identifier kDrumPadLayoutMode("drumPadLayoutMode");
const juce::Identifier kRegionLeft("regionLeft");
const juce::Identifier kRegionTop("regionTop");
const juce::Identifier kRegionRight("regionRight");
const juce::Identifier kRegionBottom("regionBottom");
const juce::Identifier kZIndex("zIndex");
const juce::Identifier kRegionLock("regionLock");
const juce::Identifier kHarmonicRowInterval("harmonicRowInterval");
const juce::Identifier kHarmonicUseScaleFilter("harmonicUseScaleFilter");
const juce::Identifier kChordPadPreset("chordPadPreset");
const juce::Identifier kChordPadLatchMode("chordPadLatchMode");
const juce::Identifier kDrumFxSplitSplitRow("drumFxSplitSplitRow");
const juce::Identifier kFxCcStart("fxCcStart");
const juce::Identifier kFxOutputMin("fxOutputMin");
const juce::Identifier kFxOutputMax("fxOutputMax");
const juce::Identifier kFxToggleMode("fxToggleMode");
} // namespace

void TouchpadMixerManager::addLayout(const TouchpadMixerConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    layouts_.push_back(config);
  }
  sendChangeMessage();
}

void TouchpadMixerManager::removeLayout(int index) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(layouts_.size())) {
      layouts_.erase(layouts_.begin() + index);
    }
  }
  sendChangeMessage();
}

void TouchpadMixerManager::updateLayout(int index,
                                        const TouchpadMixerConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(layouts_.size())) {
      layouts_[static_cast<size_t>(index)] = config;
    }
  }
  sendChangeMessage();
}

void TouchpadMixerManager::addTouchpadMapping(
    const TouchpadMappingConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    touchpadMappings_.push_back(config);
  }
  sendChangeMessage();
}

void TouchpadMixerManager::removeTouchpadMapping(int index) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(touchpadMappings_.size())) {
      touchpadMappings_.erase(touchpadMappings_.begin() + index);
    }
  }
  sendChangeMessage();
}

void TouchpadMixerManager::updateTouchpadMapping(
    int index, const TouchpadMappingConfig &config) {
  {
    juce::ScopedWriteLock lock(lock_);
    if (index >= 0 && index < static_cast<int>(touchpadMappings_.size())) {
      touchpadMappings_[static_cast<size_t>(index)] = config;
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
  switch (t) {
  case TouchpadType::Mixer:
    return "mixer";
  case TouchpadType::DrumPad:
    return "drumPad";
  case TouchpadType::ChordPad:
    return "chordPad";
  default:
    return "mixer";
  }
}

juce::ValueTree TouchpadMixerManager::toValueTree() const {
  juce::ScopedReadLock lock(lock_);
  juce::ValueTree vt(kTouchpadMixers);
  for (const auto &s : layouts_) {
    juce::ValueTree child(kTouchpadMixer);
    child.setProperty(kType, typeToString(s.type), nullptr);
    child.setProperty(kName, juce::String(s.name), nullptr);
    child.setProperty(kLayerId, s.layerId, nullptr);
    child.setProperty(kLayoutGroupId, s.layoutGroupId, nullptr);
    child.setProperty(kLayoutGroupName, juce::String(s.layoutGroupName), nullptr);
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
    child.setProperty(kRegionLeft, s.region.left, nullptr);
    child.setProperty(kRegionTop, s.region.top, nullptr);
    child.setProperty(kRegionRight, s.region.right, nullptr);
    child.setProperty(kRegionBottom, s.region.bottom, nullptr);
    child.setProperty(kZIndex, s.zIndex, nullptr);
    child.setProperty(kRegionLock, s.regionLock, nullptr);
    if (s.type == TouchpadType::DrumPad ||
        s.type == TouchpadType::ChordPad) {
      child.setProperty(kDrumPadRows, s.drumPadRows, nullptr);
      child.setProperty(kDrumPadColumns, s.drumPadColumns, nullptr);
      child.setProperty(kDrumPadMidiNoteStart, s.drumPadMidiNoteStart, nullptr);
      child.setProperty(kDrumPadBaseVelocity, s.drumPadBaseVelocity, nullptr);
      child.setProperty(kDrumPadVelocityRandom, s.drumPadVelocityRandom,
                        nullptr);
      child.setProperty(kDrumPadDeadZoneLeft, s.drumPadDeadZoneLeft, nullptr);
      child.setProperty(kDrumPadDeadZoneRight, s.drumPadDeadZoneRight, nullptr);
      child.setProperty(kDrumPadDeadZoneTop, s.drumPadDeadZoneTop, nullptr);
      child.setProperty(kDrumPadDeadZoneBottom, s.drumPadDeadZoneBottom,
                        nullptr);
    }
    if (s.type == TouchpadType::DrumPad) {
      child.setProperty(kDrumPadLayoutMode,
                        static_cast<int>(s.drumPadLayoutMode), nullptr);
      child.setProperty(kHarmonicRowInterval, s.harmonicRowInterval, nullptr);
      child.setProperty(kHarmonicUseScaleFilter, s.harmonicUseScaleFilter,
                        nullptr);
    } else if (s.type == TouchpadType::ChordPad) {
      child.setProperty(kChordPadPreset, s.chordPadPreset, nullptr);
      child.setProperty(kChordPadLatchMode, s.chordPadLatchMode, nullptr);
    }
    vt.addChild(child, -1, nullptr);
  }
  // Serialize explicit touchpad mappings (if any)
  if (!touchpadMappings_.empty()) {
    juce::ValueTree mappingsNode(kTouchpadMappingsNode);
    for (const auto &m : touchpadMappings_) {
      juce::ValueTree child(kTouchpadMappingNode);
      child.setProperty(kName, juce::String(m.name), nullptr);
      child.setProperty(kLayerId, m.layerId, nullptr);
      child.setProperty(kLayoutGroupId, m.layoutGroupId, nullptr);
      child.setProperty(kMidiChannel, m.midiChannel, nullptr);
      child.setProperty(kRegionLeft, m.region.left, nullptr);
      child.setProperty(kRegionTop, m.region.top, nullptr);
      child.setProperty(kRegionRight, m.region.right, nullptr);
      child.setProperty(kRegionBottom, m.region.bottom, nullptr);
      child.setProperty(kZIndex, m.zIndex, nullptr);
      child.setProperty(kRegionLock, m.regionLock, nullptr);

      // Store the underlying mapping ValueTree (if valid) as a child.
      if (m.mapping.isValid())
        child.addChild(m.mapping.createCopy(), -1, nullptr);

      mappingsNode.addChild(child, -1, nullptr);
    }
    vt.addChild(mappingsNode, -1, nullptr);
  }
  // Serialize explicit layout groups (if any)
  if (!groups_.empty()) {
    juce::ValueTree groupsNode(kLayoutGroupsNode);
    for (const auto &g : groups_) {
      juce::ValueTree child(kLayoutGroupNode);
      child.setProperty(kLayoutGroupId, g.id, nullptr);
      child.setProperty(kLayoutGroupName, juce::String(g.name), nullptr);
      groupsNode.addChild(child, -1, nullptr);
    }
    vt.addChild(groupsNode, -1, nullptr);
  }
  return vt;
}

static TouchpadType parseType(const juce::var &v) {
  juce::String s = v.toString().trim();
  if (s.equalsIgnoreCase("drumPad"))
    return TouchpadType::DrumPad;
  if (s.equalsIgnoreCase("harmonicGrid"))
    return TouchpadType::DrumPad;
  if (s.equalsIgnoreCase("chordPad"))
    return TouchpadType::ChordPad;
  return TouchpadType::Mixer;
}

void TouchpadMixerManager::restoreFromValueTree(const juce::ValueTree &vt) {
  if (!vt.isValid() || !vt.hasType(kTouchpadMixers))
    return;
  juce::ScopedWriteLock lock(lock_);
  layouts_.clear();
  groups_.clear();
  touchpadMappings_.clear();
  for (int i = 0; i < vt.getNumChildren(); ++i) {
    auto child = vt.getChild(i);
    if (child.hasType(kTouchpadMixer)) {
      TouchpadMixerConfig s;
      auto rawTypeVar = child.getProperty(kType, "mixer");
      juce::String rawTypeStr = rawTypeVar.toString().trim();
      bool typeWasHarmonic = rawTypeStr.equalsIgnoreCase("harmonicGrid");
      s.type = parseType(rawTypeVar);
      s.name =
          child.getProperty(kName, "Touchpad Mixer").toString().toStdString();
      s.layerId = juce::jlimit(0, 8, (int)child.getProperty(kLayerId, 0));
      s.layoutGroupId =
          juce::jlimit(0, 128, (int)child.getProperty(kLayoutGroupId, 0));
      s.layoutGroupName = child
                               .getProperty(kLayoutGroupName, juce::String())
                               .toString()
                               .trim()
                               .toStdString();
      s.numFaders = juce::jlimit(1, 32, (int)child.getProperty(kNumFaders, 5));
      s.ccStart = juce::jlimit(0, 127, (int)child.getProperty(kCcStart, 50));
      s.midiChannel =
          juce::jlimit(1, 16, (int)child.getProperty(kMidiChannel, 1));
      s.inputMin = static_cast<float>(child.getProperty(kInputMin, 0.0));
      s.inputMax = static_cast<float>(child.getProperty(kInputMax, 1.0));
      s.outputMin =
          juce::jlimit(0, 127, (int)child.getProperty(kOutputMin, 0));
      s.outputMax =
          juce::jlimit(0, 127, (int)child.getProperty(kOutputMax, 127));
      int qp = (int)child.getProperty(kQuickPrecision, 0);
      s.quickPrecision = (qp == 1) ? TouchpadMixerQuickPrecision::Precision
                                   : TouchpadMixerQuickPrecision::Quick;
      int ar = (int)child.getProperty(kAbsRel, 0);
      s.absRel = (ar == 1) ? TouchpadMixerAbsRel::Relative
                           : TouchpadMixerAbsRel::Absolute;
      int lf = (int)child.getProperty(kLockFree, 1);
      s.lockFree = (lf == 0) ? TouchpadMixerLockFree::Lock
                             : TouchpadMixerLockFree::Free;
      s.muteButtonsEnabled =
          (bool)child.getProperty(kMuteButtonsEnabled, false);
      if (child.hasProperty(kRegionLeft)) {
        s.region.left =
            static_cast<float>(child.getProperty(kRegionLeft, 0.0));
        s.region.top = static_cast<float>(child.getProperty(kRegionTop, 0.0));
        s.region.right =
            static_cast<float>(child.getProperty(kRegionRight, 1.0));
        s.region.bottom =
            static_cast<float>(child.getProperty(kRegionBottom, 1.0));
      }
      s.zIndex = juce::jlimit(-100, 100, (int)child.getProperty(kZIndex, 0));
      s.regionLock = (bool)child.getProperty(kRegionLock, false);
      if (s.type == TouchpadType::DrumPad ||
          s.type == TouchpadType::ChordPad) {
        s.drumPadRows =
            juce::jlimit(1, 8, (int)child.getProperty(kDrumPadRows, 2));
        s.drumPadColumns =
            juce::jlimit(1, 16, (int)child.getProperty(kDrumPadColumns, 4));
        s.drumPadMidiNoteStart = juce::jlimit(
            0, 127, (int)child.getProperty(kDrumPadMidiNoteStart, 60));
        s.drumPadBaseVelocity = juce::jlimit(
            1, 127, (int)child.getProperty(kDrumPadBaseVelocity, 100));
        s.drumPadVelocityRandom = juce::jlimit(
            0, 127, (int)child.getProperty(kDrumPadVelocityRandom, 0));
        s.drumPadDeadZoneLeft = static_cast<float>(
            child.getProperty(kDrumPadDeadZoneLeft, 0.0));
        s.drumPadDeadZoneRight = static_cast<float>(
            child.getProperty(kDrumPadDeadZoneRight, 0.0));
        s.drumPadDeadZoneTop = static_cast<float>(
            child.getProperty(kDrumPadDeadZoneTop, 0.0));
        s.drumPadDeadZoneBottom = static_cast<float>(
            child.getProperty(kDrumPadDeadZoneBottom, 0.0));
        if (!child.hasProperty(kRegionLeft)) {
          s.region.left = s.drumPadDeadZoneLeft;
          s.region.top = s.drumPadDeadZoneTop;
          s.region.right = 1.0f - s.drumPadDeadZoneRight;
          s.region.bottom = 1.0f - s.drumPadDeadZoneBottom;
        }
      }
      if (s.type == TouchpadType::DrumPad) {
        // Layout mode: persisted as int; fallback to legacy type name when
        // absent.
        int layoutRaw =
            static_cast<int>(child.getProperty(kDrumPadLayoutMode, -1));
        if (layoutRaw == static_cast<int>(DrumPadLayoutMode::Classic) ||
            layoutRaw == static_cast<int>(DrumPadLayoutMode::HarmonicGrid)) {
          s.drumPadLayoutMode = static_cast<DrumPadLayoutMode>(layoutRaw);
        } else {
          s.drumPadLayoutMode =
              typeWasHarmonic ? DrumPadLayoutMode::HarmonicGrid
                              : DrumPadLayoutMode::Classic;
        }

        s.harmonicRowInterval = (int)child.getProperty(
            kHarmonicRowInterval, s.harmonicRowInterval);
        s.harmonicUseScaleFilter = (bool)child.getProperty(
            kHarmonicUseScaleFilter, s.harmonicUseScaleFilter);
      } else if (s.type == TouchpadType::ChordPad) {
        s.chordPadPreset =
            (int)child.getProperty(kChordPadPreset, s.chordPadPreset);
        s.chordPadLatchMode = (bool)child.getProperty(
            kChordPadLatchMode, s.chordPadLatchMode);
      }
      if (s.type == TouchpadType::Mixer && !child.hasProperty(kRegionLeft)) {
        s.region.left = 0.0f;
        s.region.top = 0.0f;
        s.region.right = 1.0f;
        s.region.bottom = 1.0f;
      }
      layouts_.push_back(s);
    } else if (child.hasType(kLayoutGroupsNode)) {
      groups_.clear();
      for (int j = 0; j < child.getNumChildren(); ++j) {
        auto gNode = child.getChild(j);
        if (!gNode.hasType(kLayoutGroupNode))
          continue;
        TouchpadLayoutGroup g;
        g.id = (int)gNode.getProperty(kLayoutGroupId, 0);
        g.name = gNode.getProperty(kLayoutGroupName, juce::String())
                     .toString()
                     .trim()
                     .toStdString();
        if (g.id > 0)
          groups_.push_back(g);
      }
    } else if (child.hasType(kTouchpadMappingsNode)) {
      touchpadMappings_.clear();
      for (int j = 0; j < child.getNumChildren(); ++j) {
        auto mNode = child.getChild(j);
        if (!mNode.hasType(kTouchpadMappingNode))
          continue;

        TouchpadMappingConfig cfg;
        cfg.name =
            mNode.getProperty(kName, "Touchpad Mapping").toString().toStdString();
        cfg.layerId =
            juce::jlimit(0, 8, (int)mNode.getProperty(kLayerId, 0));
        cfg.layoutGroupId =
            juce::jlimit(0, 128, (int)mNode.getProperty(kLayoutGroupId, 0));
        cfg.midiChannel =
            juce::jlimit(1, 16, (int)mNode.getProperty(kMidiChannel, 1));
        cfg.region.left =
            static_cast<float>(mNode.getProperty(kRegionLeft, 0.0));
        cfg.region.top =
            static_cast<float>(mNode.getProperty(kRegionTop, 0.0));
        cfg.region.right =
            static_cast<float>(mNode.getProperty(kRegionRight, 1.0));
        cfg.region.bottom =
            static_cast<float>(mNode.getProperty(kRegionBottom, 1.0));
        cfg.zIndex =
            juce::jlimit(-100, 100, (int)mNode.getProperty(kZIndex, 0));
        cfg.regionLock = (bool)mNode.getProperty(kRegionLock, false);

        // Underlying mapping tree is stored as a child; take the first Mapping
        // child if present.
        juce::ValueTree mappingChild;
        for (int k = 0; k < mNode.getNumChildren(); ++k) {
          auto c = mNode.getChild(k);
          if (c.hasType("Mapping")) {
            mappingChild = c;
            break;
          }
        }
        if (mappingChild.isValid())
          cfg.mapping = mappingChild.createCopy();

        touchpadMappings_.push_back(std::move(cfg));
      }
    }
  }

  // Backward compatibility: if no groups were serialized but layouts refer to
  // non-zero layoutGroupId values, synthesise groups from layouts.
  if (groups_.empty()) {
    std::map<int, juce::String> derived;
    for (const auto &layout : layouts_) {
      if (layout.layoutGroupId <= 0)
        continue;
      auto it = derived.find(layout.layoutGroupId);
      juce::String name = layout.layoutGroupName.empty()
                              ? ("Group " + juce::String(layout.layoutGroupId))
                              : juce::String(layout.layoutGroupName);
      if (it == derived.end())
        derived[layout.layoutGroupId] = name;
    }
    for (const auto &p : derived) {
      TouchpadLayoutGroup g;
      g.id = p.first;
      g.name = p.second.toStdString();
      groups_.push_back(g);
    }
  }

  sendChangeMessage();
}

std::vector<TouchpadLayoutGroup> TouchpadMixerManager::getGroups() const {
  juce::ScopedReadLock lock(lock_);
  return groups_;
}

void TouchpadMixerManager::addGroup(const TouchpadLayoutGroup &group) {
  juce::ScopedWriteLock lock(lock_);
  if (group.id <= 0)
    return;
  // Avoid duplicates by id.
  for (const auto &g : groups_) {
    if (g.id == group.id)
      return;
  }
  groups_.push_back(group);
  sendChangeMessage();
}

void TouchpadMixerManager::removeGroup(int groupId) {
  if (groupId <= 0)
    return;
  juce::ScopedWriteLock lock(lock_);
  // Remove from groups_
  groups_.erase(
      std::remove_if(groups_.begin(), groups_.end(),
                     [groupId](const TouchpadLayoutGroup &g) {
                       return g.id == groupId;
                     }),
      groups_.end());
  // Clear layout references
  for (auto &layout : layouts_) {
    if (layout.layoutGroupId == groupId) {
      layout.layoutGroupId = 0;
      layout.layoutGroupName.clear();
    }
  }
  sendChangeMessage();
}

void TouchpadMixerManager::renameGroup(int groupId,
                                       const juce::String &newName) {
  if (groupId <= 0)
    return;
  juce::ScopedWriteLock lock(lock_);
  for (auto &g : groups_) {
    if (g.id == groupId) {
      g.name = newName.toStdString();
      break;
    }
  }
  sendChangeMessage();
}

std::map<int, juce::String> TouchpadMixerManager::getLayoutGroups() const {
  juce::ScopedReadLock lock(lock_);
  std::map<int, juce::String> out;
  for (const auto &g : groups_) {
    if (g.id <= 0)
      continue;
    out[g.id] = juce::String(g.name.empty() ? ("Group " + juce::String(g.id))
                                            : g.name);
  }
  return out;
}
