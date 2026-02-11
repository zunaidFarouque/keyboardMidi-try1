#pragma once
#include "TouchpadMixerTypes.h"
#include <JuceHeader.h>
#include <map>
#include <vector>

class TouchpadMixerManager : public juce::ChangeBroadcaster {
public:
  TouchpadMixerManager() = default;
  ~TouchpadMixerManager() override = default;

  void addLayout(const TouchpadMixerConfig &config);
  void removeLayout(int index);
  void updateLayout(int index, const TouchpadMixerConfig &config);

  std::vector<TouchpadMixerConfig> getLayouts() const {
    juce::ScopedReadLock lock(lock_);
    return layouts_;
  }

  // Layout group registry ----------------------------------------------------
  // Explicit list of named groups. Layouts refer to groups by ID.
  std::vector<TouchpadLayoutGroup> getGroups() const;
  void addGroup(const TouchpadLayoutGroup &group);
  void removeGroup(int groupId);
  void renameGroup(int groupId, const juce::String &newName);

  // Convenience: map of groupId -> groupName for UI / mappings.
  // Groups with id=0 are excluded (0 = No Group).
  std::map<int, juce::String> getLayoutGroups() const;

  juce::ValueTree toValueTree() const;
  void restoreFromValueTree(const juce::ValueTree &vt);

private:
  mutable juce::ReadWriteLock lock_;
  std::vector<TouchpadMixerConfig> layouts_;
  std::vector<TouchpadLayoutGroup> groups_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerManager)
};
