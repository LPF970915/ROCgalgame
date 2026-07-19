#include "system_settings_runtime.h"

#include <algorithm>

bool ApplyBrightnessAdjustment(
    int delta, uint32_t now, SystemControlLevels &levels, ConfigStore &config,
    const std::function<bool(int, SystemControlLevels &)> &adjust,
    const std::function<void(SystemControlLevels &)> &refresh) {
  const bool ok = adjust && adjust(delta, levels);
  if (!ok || !levels.brightness.available) {
    if (refresh) refresh(levels);
    return false;
  }
  const int applied = std::clamp(levels.brightness.level, 0,
                                 std::max(1, levels.brightness.max_level));
  if (config.Mutable().brightness_level != applied) {
    config.Mutable().brightness_level = applied;
    config.MarkDirty(now);
  }
  return true;
}

bool SynchronizeBrightnessAtStartup(SystemControlService &system_controls,
                                    SystemControlLevels &levels, ConfigStore &config,
                                    uint32_t now) {
  const bool ok = system_controls.ApplyBrightnessLevel(config.Get().brightness_level,
                                                       levels.brightness);
  if (!ok || !levels.brightness.available) {
    system_controls.Refresh(levels);
    return false;
  }
  const int applied = std::clamp(levels.brightness.level, 0,
                                 std::max(1, levels.brightness.max_level));
  if (config.Mutable().brightness_level != applied) {
    config.Mutable().brightness_level = applied;
    config.MarkDirty(now);
  }
  return true;
}

SystemSettingsCallbacks MakeSystemSettingsCallbacks(
    VolumeController &volume_controller, SystemControlService &system_controls,
    SystemControlLevels &levels, ConfigStore &config, AppUiState &ui_state,
    const std::function<void(int)> &apply_sfx_volume,
    const std::function<void()> &play_change_sfx) {
  const auto apply_sfx = apply_sfx_volume;
  const auto play_change = play_change_sfx;
  return SystemSettingsCallbacks{
      [&system_controls, &levels]() { system_controls.Refresh(levels); },
      [&volume_controller, &config, &ui_state, apply_sfx, play_change](int delta,
                                                                      uint32_t now) {
        return ApplyVolumeAdjustment(delta, now, volume_controller, config, ui_state,
                                     apply_sfx, play_change)
            .changed;
      },
      [&system_controls, &levels, &config](int delta, uint32_t now) {
        return ApplyBrightnessAdjustment(
            delta, now, levels, config,
            [&](int steps, SystemControlLevels &current) {
              return system_controls.AdjustBrightness(steps, current);
            },
            [&](SystemControlLevels &current) { system_controls.Refresh(current); });
      },
  };
}
