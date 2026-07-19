#pragma once

#include "app_layout.h"
#include "config.h"
#include "contributor_avatar_runtime.h"
#include "game_settings_runtime.h"
#include "key_calibration_runtime.h"
#include "menu_panel.h"
#include "settings_panel_router.h"
#include "settings_runtime.h"
#include "system_controls.h"
#include "ui_assets.h"
#include "version_update_runtime.h"

#include <functional>
#include <vector>

struct SettingsPanelInputComposition {
  InputManager &input;
  float dt = 0.0f;
  KeyCalibrationState &calibration;
  VersionUpdateState &version_update;
  ContributorAvatarState &contributor_avatar;
  GameSettingsState &game_settings;
  GameSettingsCallbacks game_settings_callbacks;
  size_t contributor_avatar_count = 0;
  bool update_manifest_configured = false;
  std::function<void(int, int)> adjust_system_setting;
  std::function<bool(const KeyCalibrationState &)> save_key_mapping;
  std::function<void()> play_select_sfx;
  std::function<void(int)> confirm_contributor_avatar;
};

SettingsPanelInputHandlers MakeSettingsPanelInputHandlers(
    SettingsPanelInputComposition composition);

struct SettingsPanelDrawComposition {
  SDL_Renderer *renderer = nullptr;
  UiAssets &assets;
  const LayoutMetrics &layout;
  const AppConfig &config;
  const SystemControlLevels &system_levels;
  const SettingsRuntimeState &menu_state;
  const GameSettingsState &game_settings;
  const KeyCalibrationState &calibration;
  const std::vector<ContributorAvatarEntry> &avatars;
  const ContributorAvatarState &contributor_avatar;
  const VersionUpdateState &version_update;
  MenuPanelDrawServices services;
};

SettingsPanelDrawHandlers MakeSettingsPanelDrawHandlers(
    SettingsPanelDrawComposition composition);
