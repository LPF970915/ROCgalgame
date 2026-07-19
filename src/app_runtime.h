#pragma once

#include "app_stores.h"
#include "system_controls.h"

#include <cstdint>
#include <functional>

struct AppUiState {
  int volume_display_percent = 0;
  uint32_t volume_display_until = 0;
  bool volume_reconcile_pending = false;
  int volume_reconcile_percent = 0;
  uint32_t volume_reconcile_due = 0;
};

struct VolumeAdjustmentResult {
  bool changed = false;
  int percent = 0;
};

class VolumeController {
public:
  using AdjustCallback = std::function<bool(int, SystemControlLevels &)>;
  using RefreshCallback = std::function<bool(SystemControlValue &)>;
  using ApplyCallback = std::function<bool(int, SystemControlValue &)>;

  VolumeController(SystemControlService &service, SystemControlLevels &levels, bool prefer_system);
  VolumeController(SystemControlLevels &levels, AdjustCallback adjust, RefreshCallback refresh,
                   ApplyCallback apply = {});

  bool UsesSystemVolume() const;
  bool ApplyConfiguredPercent(int percent, int &out_percent);
  bool AdjustBySteps(int delta_steps, int &out_percent);
  bool RefreshPercent(int &out_percent);

private:
  int PercentFromLevel() const;

  SystemControlService *service_ = nullptr;
  SystemControlLevels &levels_;
  bool prefer_system_ = true;
  AdjustCallback adjust_;
  RefreshCallback refresh_;
  ApplyCallback apply_;
};

AppUiState InitializeAppUiState(const AppConfig &config, VolumeController &volume_controller);
int SfxVolumeFromSystemPercent(int percent);
VolumeAdjustmentResult ApplyVolumeAdjustment(
    int delta_steps, uint32_t now, VolumeController &volume_controller, ConfigStore &config,
    AppUiState &ui_state, const std::function<void(int)> &apply_sfx_volume,
    const std::function<void()> &play_change_sfx);
void ScheduleVolumeReconcile(AppUiState &ui_state, uint32_t now);
bool TickVolumeReconcile(uint32_t now, VolumeController &volume_controller,
                         ConfigStore &config, AppUiState &ui_state,
                         const std::function<void(int)> &apply_sfx_volume);
