#pragma once

#include "DeviceManager.h"
#include "MappingTypes.h"
#include "PresetManager.h"
#include "SettingsManager.h"
#include "ZoneManager.h"
#include <memory>

// Phase 50.2 / 50.3: GridCompiler
// Responsible for converting the current preset / device / zone state
// into a static CompiledMapContext (AudioGrid + VisualGrid structures).
class GridCompiler {
public:
  // Build a new compiled context snapshot from the current engine state.
  static std::shared_ptr<CompiledMapContext>
  compile(PresetManager &presetMgr, DeviceManager &deviceMgr,
          ZoneManager &zoneMgr, SettingsManager &settingsMgr);

private:
  // Phase 50.3: Bake zones into the grids (processed before manual mappings).
  static void compileZones(CompiledMapContext &context, ZoneManager &zoneMgr,
                           DeviceManager &deviceMgr, int layerId);
};
