#include "app_runtime.h"

#include <SDL.h>

#include <algorithm>

VolumeController::VolumeController(SystemControlService &service, SystemControlLevels &levels,
                                   bool prefer_system)
    : service_(&service), levels_(levels), prefer_system_(prefer_system) {}

VolumeController::VolumeController(SystemControlLevels &levels, AdjustCallback adjust,
                                   RefreshCallback refresh, ApplyCallback apply)
    : levels_(levels), prefer_system_(true), adjust_(std::move(adjust)),
      refresh_(std::move(refresh)), apply_(std::move(apply)) {}

bool VolumeController::UsesSystemVolume() const { return prefer_system_; }

int VolumeController::PercentFromLevel() const {
  return std::clamp((levels_.volume.level * 100) /
                        std::max(1, levels_.volume.max_level),
                    0, 100);
}

bool VolumeController::ApplyConfiguredPercent(int percent, int &out_percent) {
  const int requested = std::clamp(percent, 0, 100);
  const bool ok = apply_ ? apply_(requested, levels_.volume)
                         : service_ && service_->ApplyVolumePercent(requested, levels_.volume);
  if (ok && levels_.volume.available) {
    out_percent = PercentFromLevel();
    return true;
  }
  return RefreshPercent(out_percent);
}

bool VolumeController::AdjustBySteps(int delta_steps, int &out_percent) {
  if (delta_steps == 0) return RefreshPercent(out_percent);
  const bool ok = adjust_ ? adjust_(delta_steps, levels_)
                          : service_ && service_->AdjustVolume(delta_steps, levels_);
  if (!ok || !levels_.volume.available) {
    RefreshPercent(out_percent);
    return false;
  }
  out_percent = PercentFromLevel();
  return true;
}

bool VolumeController::RefreshPercent(int &out_percent) {
  const bool ok = refresh_ ? refresh_(levels_.volume)
                           : service_ && service_->RefreshVolumeOnly(levels_.volume);
  if (!ok || !levels_.volume.available) return false;
  out_percent = PercentFromLevel();
  return true;
}

AppUiState InitializeAppUiState(const AppConfig &config, VolumeController &volume_controller) {
  AppUiState state;
  state.volume_display_percent = std::clamp(config.system_volume_percent, 0, 100);
  int actual_percent = state.volume_display_percent;
  if (volume_controller.ApplyConfiguredPercent(state.volume_display_percent, actual_percent)) {
    state.volume_display_percent = actual_percent;
  }
  return state;
}

int SfxVolumeFromSystemPercent(int percent) {
  return std::clamp((std::clamp(percent, 0, 100) * SDL_MIX_MAXVOLUME + 50) / 100,
                    0, SDL_MIX_MAXVOLUME);
}

VolumeAdjustmentResult ApplyVolumeAdjustment(
    int delta_steps, uint32_t now, VolumeController &volume_controller, ConfigStore &config,
    AppUiState &ui_state, const std::function<void(int)> &apply_sfx_volume,
    const std::function<void()> &play_change_sfx) {
  VolumeAdjustmentResult result;
  result.percent = ui_state.volume_display_percent;
  if (!volume_controller.AdjustBySteps(delta_steps, result.percent)) return result;

  result.changed = true;
  result.percent = std::clamp(result.percent, 0, 100);
  AppConfig &cfg = config.Mutable();
  if (cfg.system_volume_percent != result.percent) {
    cfg.system_volume_percent = result.percent;
    config.MarkDirty(now);
  }
  ui_state.volume_display_percent = result.percent;
  ui_state.volume_display_until = now + 1500;
  if (apply_sfx_volume) apply_sfx_volume(SfxVolumeFromSystemPercent(result.percent));
  if (play_change_sfx) play_change_sfx();
  return result;
}

void ScheduleVolumeReconcile(AppUiState &ui_state, uint32_t now) {
  ui_state.volume_reconcile_pending = true;
  ui_state.volume_reconcile_percent = ui_state.volume_display_percent;
  ui_state.volume_reconcile_due = now + 250;
}

bool TickVolumeReconcile(uint32_t now, VolumeController &volume_controller,
                         ConfigStore &config, AppUiState &ui_state,
                         const std::function<void(int)> &apply_sfx_volume) {
  if (!ui_state.volume_reconcile_pending ||
      !SDL_TICKS_PASSED(now, ui_state.volume_reconcile_due)) {
    return false;
  }
  ui_state.volume_reconcile_pending = false;
  int actual_percent = ui_state.volume_reconcile_percent;
  if (!volume_controller.ApplyConfiguredPercent(ui_state.volume_reconcile_percent,
                                                actual_percent)) {
    return false;
  }
  actual_percent = std::clamp(actual_percent, 0, 100);
  ui_state.volume_display_percent = actual_percent;
  if (config.Mutable().system_volume_percent != actual_percent) {
    config.Mutable().system_volume_percent = actual_percent;
    config.MarkDirty(now);
  }
  if (apply_sfx_volume) apply_sfx_volume(SfxVolumeFromSystemPercent(actual_percent));
  return true;
}
