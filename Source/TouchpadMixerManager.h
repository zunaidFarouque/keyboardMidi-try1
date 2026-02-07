#pragma once
#include "TouchpadMixerTypes.h"
#include <JuceHeader.h>
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

  juce::ValueTree toValueTree() const;
  void restoreFromValueTree(const juce::ValueTree &vt);

private:
  mutable juce::ReadWriteLock lock_;
  std::vector<TouchpadMixerConfig> layouts_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerManager)
};
