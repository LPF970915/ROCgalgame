#pragma once

#include "app_runtime.h"

#include <functional>

struct SystemSettingsCallbacks {
  std::function<void()> refresh_levels;
  std::function<bool(int, uint32_t)> adjust_volume;
  std::function<bool(int, uint32_t)> adjust_brightness;
};

bool ApplyBrightnessAdjustment(
    int delta, uint32_t now, SystemControlLevels &levels, ConfigStore &config,
    const std::function<bool(int, SystemControlLevels &)> &adjust,
    const std::function<void(SystemControlLevels &)> &refresh);

bool SynchronizeBrightnessAtStartup(SystemControlService &system_controls,
                                    SystemControlLevels &levels, ConfigStore &config,
                                    uint32_t now);

SystemSettingsCallbacks MakeSystemSettingsCallbacks(
    VolumeController &volume_controller, SystemControlService &system_controls,
    SystemControlLevels &levels, ConfigStore &config, AppUiState &ui_state,
    const std::function<void(int)> &apply_sfx_volume,
    const std::function<void()> &play_change_sfx);
