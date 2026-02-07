#pragma once
#include "TouchpadMixerTypes.h"
#include <JuceHeader.h>
#include <vector>

class TouchpadMixerManager : public juce::ChangeBroadcaster {
public:
  TouchpadMixerManager() = default;
  ~TouchpadMixerManager() override = default;

  void addStrip(const TouchpadMixerConfig &config);
  void removeStrip(int index);
  void updateStrip(int index, const TouchpadMixerConfig &config);

  std::vector<TouchpadMixerConfig> getStrips() const {
    juce::ScopedReadLock lock(lock_);
    return strips_;
  }

  juce::ValueTree toValueTree() const;
  void restoreFromValueTree(const juce::ValueTree &vt);

private:
  mutable juce::ReadWriteLock lock_;
  std::vector<TouchpadMixerConfig> strips_;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TouchpadMixerManager)
};
