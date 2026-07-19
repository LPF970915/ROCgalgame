#pragma once

#include "input_manager.h"
#include "game_settings_runtime.h"
#include "menu_panel.h"
#include "settings_runtime.h"

struct GameSettingsPanelModel {
  int language_index = 0;
  GameSettingsState state;
};

bool HandleGameSettingsPanelInput(
    const InputManager &input, SettingsRuntimeState &menu_state,
    GameSettingsState &state, const GameSettingsCallbacks &callbacks);

void DrawGameSettingsPanel(const SDL_Rect &preview, int first_row_y, int row_pitch,
                           int row_h, const SettingsRuntimeState &menu_state,
                           const GameSettingsPanelModel &model,
                           const MenuPanelDrawServices &services);
