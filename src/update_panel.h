#pragma once

#include "menu_panel.h"
#include "version_update_runtime.h"

#include <SDL.h>

struct UpdatePanelModel {
  int language_index = 0;
  VersionUpdateStatus status = VersionUpdateStatus::Idle;
  bool panel_active = false;
};

void DrawUpdatePanel(const SDL_Rect &preview, int first_row_y,
                     const UpdatePanelModel &model,
                     const MenuPanelDrawServices &services);
