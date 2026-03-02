#pragma once

#include "DeviceManager.h"
#include "MappingTypes.h"
#include "PresetManager.h"
#include "SettingsManager.h"
#include "TouchpadLayoutManager.h"
#include "ZoneManager.h"
#include <memory>

// Phase 50.2 / 50.3: MappingCompiler
// Responsible for converting the current preset / device / zone state
// into a static CompiledMapContext (AudioGrid + VisualGrid structures).
// Compiles both keyboard mappings/zones and touchpad layouts.
class MappingCompiler {
public:
  // Build a new compiled context snapshot from the current engine state.
  static std::shared_ptr<CompiledMapContext>
  compile(PresetManager &presetMgr, DeviceManager &deviceMgr,
          ZoneManager &zoneMgr, TouchpadLayoutManager &touchpadLayoutMgr,
          SettingsManager &settingsMgr);

private:
  static void compileTouchpadPart(CompiledMapContext &context,
      PresetManager &presetMgr, DeviceManager &deviceMgr, ZoneManager &zoneMgr,
      TouchpadLayoutManager &touchpadLayoutMgr, SettingsManager &settingsMgr);
  static void compileKeyboardPart(CompiledMapContext &context,
      PresetManager &presetMgr, DeviceManager &deviceMgr, ZoneManager &zoneMgr,
      SettingsManager &settingsMgr);
  // Phase 50.3: Bake zones into the grids (processed before manual mappings).
  static void compileZones(CompiledMapContext &context, ZoneManager &zoneMgr,
                           DeviceManager &deviceMgr, int layerId);
};
