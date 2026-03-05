#pragma once

#include "KeyboardLayoutUtils.h"
#include "Zone.h"
#include <memory>
#include <vector>

enum class ZonePresetId {
  BottomRowChords,
  BottomTwoRowsPiano,
  QwerNumbersPiano,
  JankoFourRows,
  IsomorphicFourRows
};

namespace ZonePresets {

// Create one or more preconfigured Zone objects for the given preset.
// The caller is responsible for adding them to a ZoneManager.
std::vector<std::shared_ptr<Zone>> createPresetZones(ZonePresetId presetId);

} // namespace ZonePresets

