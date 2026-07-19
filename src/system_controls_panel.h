#pragma once

#include "input_manager.h"
#include "menu_panel.h"
#include "settings_runtime.h"
#include "system_controls.h"

#include <functional>

struct SystemControlsPanelModel {
  bool auto_sleep_enabled = true;
  int auto_sleep_interval_index = 2;
  int language_index = 0;
  SystemControlLevels levels;
};

bool HandleSystemControlsPanelInput(
    const InputManager &input, SettingsRuntimeState &menu_state,
    const std::function<void(int, int)> &adjust_setting);

void DrawSystemControlsPanel(const SDL_Rect &preview, int first_row_y, int row_pitch,
                             int row_h, const SettingsRuntimeState &menu_state,
                             const SystemControlsPanelModel &model,
                             const MenuPanelDrawServices &services);
